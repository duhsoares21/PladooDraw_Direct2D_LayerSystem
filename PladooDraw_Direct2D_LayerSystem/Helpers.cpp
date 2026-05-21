#include "pch.h"
#include "Constants.h"
#include "Helpers.h"

#include "Render.h"

#include "Tools.h"
#include "Layers.h"
#include "SurfaceDial.h"
#include "Replay.h"
#include "ToolsAux.h"
#include "Animation.h"

int g_scrollOffset = 0;

int HGetActiveLayersCount() {
    size_t count = std::count_if(layers.begin(), layers.end(), [](auto& l) { return l.has_value(); });
    size_t countHidden = 0;

    for (size_t i = 0; i < layers.size(); i++)
    {
        if (layers[i].has_value()) {
            if (!layers[i].value().isActive) {
                countHidden++;
            }
       }
    }

    count = count > countHidden ? count - countHidden : countHidden - count;

    return count;
}

int HGetMaxFrameIndex() {
    int maxFrameIndex = 0;

    for (const auto& action : Actions) {
        if (action.FrameIndex > maxFrameIndex)
            maxFrameIndex = action.FrameIndex;
    }

    return maxFrameIndex;
}

int GetActionStepCount(const ACTION& a) {
    switch (a.Tool) {
    case TBrush:
        return static_cast<int>((std::max)(size_t{ 1 }, a.FreeForm.vertices.size()));
    case TPaintBucket: {
        constexpr int kBucketPixelsPerStep = 128;
        if (a.pixelsToFill.empty()) return 1;
        return static_cast<int>((a.pixelsToFill.size() + kBucketPixelsPerStep - 1) / kBucketPixelsPerStep);
    }
    default:
        return 1;
    }
}

ACTION MakeStepAction(const ACTION& orig, int stepIndex) {
    ACTION partial = orig;
    int totalSteps = GetActionStepCount(orig);
    int clampedStep = (std::max)(1, (std::min)(stepIndex, totalSteps));

    switch (orig.Tool) {
    case TBrush: {
        size_t totalVertices = orig.FreeForm.vertices.size();
        if (totalVertices == 0) break;
        size_t take = (std::min)(totalVertices, static_cast<size_t>(clampedStep));
        partial.FreeForm.vertices.assign(
            orig.FreeForm.vertices.begin(),
            orig.FreeForm.vertices.begin() + take
        );
        break;
    }
    case TPaintBucket: {
        constexpr int kBucketPixelsPerStep = 128;
        size_t totalPixels = orig.pixelsToFill.size();
        if (totalPixels == 0) break;
        size_t take = (std::min)(totalPixels, static_cast<size_t>(clampedStep) * static_cast<size_t>(kBucketPixelsPerStep));
        partial.pixelsToFill.assign(
            orig.pixelsToFill.begin(),
            orig.pixelsToFill.begin() + take
        );
        break;
    }
    default:
        break;
    }

    return partial;
}

/*RENDER*/

RectF GetActionBounds(const ACTION& action)
{
    switch (action.Tool)
    {
    case TRectangle:
    case TWrite:
        return action.Position;

    case TEllipse:
        return RectF{
            action.Ellipse.point.x - action.Ellipse.radiusX,
            action.Ellipse.point.y - action.Ellipse.radiusY,
            action.Ellipse.point.x + action.Ellipse.radiusX,
            action.Ellipse.point.y + action.Ellipse.radiusY
        };

    case TLine:
        return RectF{
            min(action.Line.startPoint.x, action.Line.endPoint.x),
            min(action.Line.startPoint.y, action.Line.endPoint.y),
            max(action.Line.startPoint.x, action.Line.endPoint.x),
            max(action.Line.startPoint.y, action.Line.endPoint.y)
        };

    case TBrush: {
        float left = FLT_MAX, top = FLT_MAX, right = -FLT_MAX, bottom = -FLT_MAX;
        for (const auto& v : action.FreeForm.vertices) {
            left = min(left, v.x);
            top = min(top, v.y);
            right = max(right, v.x);
            bottom = max(bottom, v.y);
        }
        return RectF{ left, top, right, bottom };
    }

    default:
        return RectF{};
    }
}

void DrawResizeHandles(
    IRenderSurface& renderSurface,
    const RectF& r,
    const ColorRGBA& fill,
    const ColorRGBA& border
)
{
    constexpr float HANDLE_SIZE = 6.0f;
    constexpr float HANDLE_HALF = HANDLE_SIZE * 0.5f;

    auto drawHandle = [&](float x, float y)
        {
            RectF h = RectF{
                x - HANDLE_HALF,
                y - HANDLE_HALF,
                x + HANDLE_HALF,
                y + HANDLE_HALF
            };

            renderSurface.FillRect(h, fill);
            renderSurface.StrokeRect(h, border, 1.0f);
        };

    float cx = (r.left + r.right) * 0.5f;
    float cy = (r.top + r.bottom) * 0.5f;

    drawHandle(r.left, r.top);
    drawHandle(cx, r.top);
    drawHandle(r.right, r.top);
    drawHandle(r.left, cy);
    drawHandle(r.right, cy);
    drawHandle(r.left, r.bottom);
    drawHandle(cx, r.bottom);
    drawHandle(r.right, r.bottom);
}

void HRenderAction(ACTION& action, IRenderSurface& renderSurface, std::optional<ColorRGBA> customColor) {
    ColorRGBA color = customColor.has_value() ? *customColor : HGetRGBAColor(action.Color);
    ColorRGBA fillColor = HGetRGBAColor(action.FillColor);

    switch (action.Tool) {
    case TEraser:
        renderSurface.PushClip(action.Position);
        renderSurface.Clear(ColorRGBA{ 1.0f, 1.0f, 1.0f, 0.0f });
        renderSurface.PopClip();
        break;

    case TRectangle:
        renderSurface.PushClip(action.Position);
        renderSurface.FillRect(action.Position, color);
        renderSurface.PopClip();
        break;

    case TEllipse:
        renderSurface.FillEllipse(action.Ellipse, color);
        break;

    case TLine:
        renderSurface.DrawLine(action.Line.startPoint, action.Line.endPoint, color, static_cast<float>(action.BrushSize));
        break;

    case TBrush:
        for (const auto& vertex : action.FreeForm.vertices) {
            if (action.isPixelMode) {
                int snappedLeft = static_cast<int>(vertex.x / pixelSizeRatio) * pixelSizeRatio;
                int snappedTop = static_cast<int>(vertex.y / pixelSizeRatio) * pixelSizeRatio;

                renderSurface.FillRect(
                    RectF{
                        static_cast<float>(snappedLeft),
                        static_cast<float>(snappedTop),
                        static_cast<float>(snappedLeft + pixelSizeRatio),
                        static_cast<float>(snappedTop + pixelSizeRatio)
                    },
                    color
                );
            }
            else {
                float scaledLeft = static_cast<float>(vertex.x);
                float scaledTop = static_cast<float>(vertex.y);
                float scaledBrushSize = static_cast<float>(currentBrushSize);

                RectF rect = RectF{
                    scaledLeft - scaledBrushSize * 0.5f,
                    scaledTop - scaledBrushSize * 0.5f,
                    scaledLeft + scaledBrushSize * 0.5f,
                    scaledTop + scaledBrushSize * 0.5f
                };

                renderSurface.StrokeRect(rect, color, 1.0f);
                renderSurface.FillRect(rect, color);
            }
        }
        break;

    case TPaintBucket:
        renderSurface.SetPrimitiveBlendCopy(true);
        renderSurface.SetAliased(true);
        for (const auto& p : action.pixelsToFill) {
            renderSurface.FillRect(
                RectF{ static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.x + 1), static_cast<float>(p.y + 1) },
                fillColor
            );
        }
        renderSurface.SetPrimitiveBlendCopy(false);
        renderSurface.SetAliased(false);
        break;

    case TWrite: {
        if (action.Text.empty()) break;

        FontDesc font{
            action.FontFamily,
            action.FontSize,
            action.FontWeight,
            action.FontItalic,
            action.FontUnderline,
            action.FontStrike
        };
        renderSurface.DrawText(action.Text, font, action.Position, color);
        break;
    }

    case TMove: {
        if (action.LastMovedPosition == true) {
            int target = action.TargetID;

            auto moveAction = std::find_if(
                Actions.begin(),
                Actions.end(),
                [target](const ACTION& currentAction) {
                    return currentAction.Id == target;
                }
            );

            if (moveAction == Actions.end()) break;

            if (moveAction->Tool == TRectangle || moveAction->Tool == TWrite) {
                moveAction->Position = action.Position;
            }
            else if (moveAction->Tool == TBrush) {
                moveAction->FreeForm = action.FreeForm;
            }
            else if (moveAction->Tool == TEllipse) {
                moveAction->Ellipse = action.Ellipse;
            }
            else if (moveAction->Tool == TLine) {
                moveAction->Line = action.Line;
            }
        }
        break;
    }

    default:
        break;
    }

    if (action.Id == selectedActionId)
    {
        RectF bounds = GetActionBounds(action);
        ColorRGBA highlightColor{ 0.2f, 0.6f, 1.0f, 0.8f };
        ColorRGBA handleFill{ 1.0f, 1.0f, 1.0f, 1.0f };
        ColorRGBA handleBorder{ 0.2f, 0.6f, 1.0f, 1.0f };

        const float padding = 4.0f;
        bounds.left -= padding;
        bounds.top -= padding;
        bounds.right += padding;
        bounds.bottom += padding;

        renderSurface.StrokeRect(bounds, highlightColor, 2.0f);
        DrawResizeHandles(renderSurface, bounds, handleFill, handleBorder);
        renderSurface.StrokeEllipse(
            EllipseF{
                PointF{ (bounds.left + bounds.right) * 0.5f, bounds.top - 20.0f },
                6.0f,
                6.0f
            },
            handleBorder,
            2.0f
        );
    }
}

RenderData HCreateRenderData(const SizeU& size) {
    if (!gGraphicsBackend) {
        return {};
    }

    RenderData rd = gGraphicsBackend->CreateBitmapRenderData(size);
    if (!rd.surfaceHandle || !rd.bitmapHandle) {
        // CreateBitmapRenderData failed (e.g. GL context not ready yet).
        // Log and return empty so callers can detect and skip.
        HCreateLogData("error.log", "HCreateRenderData: CreateBitmapRenderData returned empty RenderData");
        return {};
    }
    return rd;
}


RenderData HCreateRenderDataHWND(HWND hWnd) {
    if (!gGraphicsBackend) {
        return {};
    }

    return gGraphicsBackend->CreateWindowRenderData(hWnd);
}

/*TIMELINE*/

HWND HCreateTimelineFrameButton(int FrameIndex) {
    RECT rcParent;
    GetClientRect(timelineHWND, &rcParent);

    int timelineParentWidth = rcParent.right - rcParent.left;

    HINSTANCE hTimelineInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(timelineHWND, GWLP_HINSTANCE));

    DWORD style = WS_CHILD | WS_VISIBLE | BS_BITMAP;

    HWND timelineFrame = CreateWindowEx(
        0,
        L"Button",
        L"",
        style,
        (timelineParentWidth / 2) - (btnWidth / 2) + (btnWidth * FrameIndex),
        10,
        btnWidth,
        btnHeight,
        timelineHWND,
        (HMENU)(intptr_t)FrameIndex,
        hTimelineInstance,
        NULL
    );
    
    return timelineFrame;
}

void HCreateHighlightFrame() {
    HINSTANCE hReplayInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(timelineHWND, GWLP_HINSTANCE));

    DWORD style = WS_CHILD | WS_VISIBLE | BS_BITMAP;

    highlightFrame = CreateWindowEx(
        0,
        L"Button",
        L"",
        style,
        btnWidth * 0,
        0,
        btnWidth,
        10,
        timelineHWND,
        (HMENU)(intptr_t)-1,
        hReplayInstance,
        NULL
    );

    RECT rc{};
    GetClientRect(highlightFrame, &rc);

    RenderData renderData = HCreateRenderDataHWND(highlightFrame);
    if (!renderData.surfaceHandle) {
        return;
    }

    RectF highlightRect = MakeRectF(
        static_cast<float>(rc.left),
        static_cast<float>(rc.top),
        static_cast<float>(rc.right),
        static_cast<float>(rc.bottom)
    );

    renderData.surfaceHandle->BeginDraw();

    renderData.surfaceHandle->Clear(ColorRGBA{ 1.0f, 1.0f, 1.0f, 1.0f });
    renderData.surfaceHandle->FillRect(highlightRect, ColorRGBA{ 0.0f, 0.0f, 1.0f, 1.0f });
    renderData.surfaceHandle->EndDraw();

    renderData.surfaceHandle->Present(1);
}

void HSetTimelineHighlight() {

    RECT rc;
    GetClientRect(timelineHWND, &rc);

    int timelineParentWidth = rc.right - rc.left;

    SetWindowPos(highlightFrame, HWND_BOTTOM, (timelineParentWidth / 2) - (btnWidth / 2), 0, btnWidth, 10, 0);
}

/*REPLAY MODE*/

void HSetReplayMode(int pReplayMode) {
    isReplayMode = pReplayMode;

    if (isReplayMode == 1) {

        lastActiveReplayFrame = 0;
        g_scrollOffsetTimeline = 0;

        ReplayActions = Actions;
        ReplayRedoActions = RedoActions;

        RedoActions.clear();

        for (int i = 0; i < ReplayActions.size(); i++) {
            if (ReplayActions[i].Tool != TLayer) {
                RedoActions.push_back(ReplayActions[i]);
            }
        }

        Actions.clear();

        std::reverse(RedoActions.begin(), RedoActions.end());

        for (int i = 0; i < TimelineFrameButtons.size(); i++) {
            DestroyWindow(TimelineFrameButtons[i].value().frame);
            TimelineFrameButtons[i].reset();
        }

        TimelineFrameButtons.clear();

        for (int i = 0; i < RedoActions.size() + 1; i++) {
            TCreateReplayFrame(i);
        }
        
        HCreateHighlightFrame();
        HSetTimelineHighlight();
        TReplayClearLayers();
        TReplayRender();
    }
    else {
        Actions = ReplayActions;
        RedoActions = ReplayRedoActions;
		TimelineFrameButtons.clear();
        TRenderLayers();
    }

    CleanupSurfaceDial();
    TInitializeSurfaceDial(mainHWND);
}

/*ANIMATION MODE*/
void HSetAnimationMode(int pAnimationMode) {
    isAnimationMode = pAnimationMode;

    if (isAnimationMode == 1) {
        HCreateHighlightFrame();
        HSetTimelineHighlight();
        TRenderAnimation();
    }

    CleanupSurfaceDial();
    TInitializeSurfaceDial(mainHWND);
}

void HSetSelectedTool(int pselectedTool) {
    selectedTool = pselectedTool;
}

void HCreateLogData(std::string fileName, std::string content) {

    std::ofstream outFile(fileName, std::ios::app);

    if (!outFile.is_open()) {
        MessageBox(nullptr, L"Failed to open file for writing", L"Error", MB_OK);
        return;
    }

    outFile << content << std::endl;

    outFile.close();

    return;
}

ColorRGBA HGetRGBAColor(COLORREF color) {
    return ColorRGBA{
        (color & 0xFF) / 255.0f,
        ((color >> 8) & 0xFF) / 255.0f,
        ((color >> 16) & 0xFF) / 255.0f,
        1.0f
    };
}

RECT HScalePointsToButton(int x, int y, int drawingAreaWidth, int drawingAreaHeight, int buttonWidth, int buttonHeight, bool pixelMode, int pPixelSizeRatio) {
    float scaleX = buttonWidth / static_cast<float>(drawingAreaWidth);
    float scaleY = buttonHeight / static_cast<float>(drawingAreaHeight);

    float scale = min(scaleX, scaleY);

    int offsetX = (buttonWidth - static_cast<int>(drawingAreaWidth * scale * zoomFactor)) / 2;
    int offsetY = (buttonHeight - static_cast<int>(drawingAreaHeight * scale * zoomFactor)) / 2;

    x = static_cast<int>(x * scale * zoomFactor) + offsetX;
    y = static_cast<int>(y * scale * zoomFactor) + offsetY;
    int scaledSize = static_cast<int>(pPixelSizeRatio * scale * zoomFactor);

    RECT rect = { x, y, 0, 0 };

    if (pixelMode) {
        rect = { x, y, x + scaledSize, y + scaledSize };
    }

    return rect;
}

std::vector<std::string> HSplit(std::string s, std::string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

void HPrintHResultError(HRESULT hr) {
    _com_error err(hr);
    LPCTSTR errMsg = err.ErrorMessage();
    std::wcout << "Erro HRESULT: 0x" << std::hex << hr << " - " << errMsg << std::endl;
}

void HCleanup() {
    for (auto& layer : layers) {
        if (layer.has_value()) {
            layer.value().surfaceHandle.reset();
            layer.value().bitmapHandle.reset();
        }
    }

    for (auto& layer : layerBitmaps) {
        layer.surfaceHandle.reset();
        layer.bitmapHandle.reset();
    }

    for (auto& layerbuttons : LayerButtons) {
        if (layerbuttons.has_value()) {
            layerbuttons.reset();
        }
    }

    for (auto& timelineFrameButton : TimelineFrameButtons) {
        if (timelineFrameButton.has_value()) {
            timelineFrameButton.reset();
        }
    }

    layers.clear();
    layersOrder.clear();
    Actions.clear();
    RedoActions.clear();
    LayerButtons.clear();
    TimelineFrameButtons.clear();
}

void HOnScrollWheelLayers(int wParam) {
    int direction = (wParam > 0) ? -1 : 1;

    int delta = btnHeight;

    std::vector<LayerOrder> result;
    result.reserve(layers.size());
    for (auto& sortedLayer : layersOrder) {
        if (layers[sortedLayer.layerIndex].has_value() &&
            sortedLayer.layerIndex == layers[sortedLayer.layerIndex].value().LayerID) {
            result.push_back(sortedLayer);
        }
    }

    std::sort(result.begin(), result.end(),
        [](const LayerOrder& a, const LayerOrder& b) {
            return a.layerOrder > b.layerOrder;
        });

    int itemWidth = btnWidth;
    int itemHeight = btnHeight;
    int contentHeight = ((int)result.size() - 1) * itemHeight;

    RECT parentRc;
    GetClientRect(layersHWND, &parentRc);
    int viewHeight = parentRc.bottom - parentRc.top - itemHeight;

    g_scrollOffset += direction * delta;

    if (g_scrollOffset < 0)
        g_scrollOffset = 0;

    if (g_scrollOffset > contentHeight - viewHeight)
        g_scrollOffset = contentHeight - viewHeight;

    if (contentHeight <= viewHeight)
        g_scrollOffset = 0;

    for (size_t i = 0; i < result.size(); i++) {
        int currentLayerIndex = result[i].layerIndex;

        int x = 0;
        int y = (int)(i * itemHeight) - g_scrollOffset; // posição base - offset

        MoveWindow(LayerButtons[currentLayerIndex].value().button,
            x, y, itemWidth, itemHeight, TRUE);
    }
}

void HOnScrollWheelTimeline(int wParam) {
    int direction = (wParam > 0) ? -1 : 1;

    int delta = btnWidth;
    RECT rcParent;
    GetClientRect(timelineHWND, &rcParent);

    int timelineParentWidth = rcParent.right - rcParent.left;
    int itemWidth = btnWidth;
    int itemHeight = btnHeight;

    g_scrollOffsetTimeline += direction * delta;

    for (size_t i = 0; i < TimelineFrameButtons.size(); i++) {
        if (TimelineFrameButtons[i].has_value() == false) continue;
        int currentFrameIndex = TimelineFrameButtons[i].value().FrameIndex;

        int x = ((timelineParentWidth / 2) - (itemWidth / 2) + (itemWidth * i)) - g_scrollOffsetTimeline;
        int y = 10;

        MoveWindow(TimelineFrameButtons[currentFrameIndex].value().frame,
            x, y, itemWidth, itemHeight, TRUE);
    }
}

void HSetScrollPosition(int index) {
    int delta = btnWidth;
    RECT rcParent;
    GetClientRect(timelineHWND, &rcParent);

    int timelineParentWidth = rcParent.right - rcParent.left;
    int itemWidth = btnWidth;
    int itemHeight = btnHeight;

    g_scrollOffsetTimeline = index * delta;

    for (size_t i = 0; i < TimelineFrameButtons.size(); i++) {
		if (!TimelineFrameButtons[i].has_value()) continue;
        int currentFrameIndex = TimelineFrameButtons[i].value().FrameIndex;

        int x = ((timelineParentWidth / 2) - (itemWidth / 2) + (itemWidth * i)) - g_scrollOffsetTimeline;
        int y = 10;

        MoveWindow(TimelineFrameButtons[currentFrameIndex].value().frame,
            x, y, itemWidth, itemHeight, TRUE);
    }
}

template <class T> void SafeRelease(T** ppT)  
{  
    if (ppT && *ppT)  
    {  
        (*ppT)->Release();  
        *ppT = nullptr;  
    }  
}
