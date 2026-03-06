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

D2D1_RECT_F GetActionBounds(const ACTION& action)
{
    switch (action.Tool)
    {
    case TRectangle:
    case TWrite:
        return action.Position;

    case TEllipse:
        return D2D1::RectF(
            action.Ellipse.point.x - action.Ellipse.radiusX,
            action.Ellipse.point.y - action.Ellipse.radiusY,
            action.Ellipse.point.x + action.Ellipse.radiusX,
            action.Ellipse.point.y + action.Ellipse.radiusY
        );

    case TLine:
        return D2D1::RectF(
            min(action.Line.startPoint.x, action.Line.endPoint.x),
            min(action.Line.startPoint.y, action.Line.endPoint.y),
            max(action.Line.startPoint.x, action.Line.endPoint.x),
            max(action.Line.startPoint.y, action.Line.endPoint.y)
        );

    case TBrush: {
        float left = FLT_MAX, top = FLT_MAX, right = -FLT_MAX, bottom = -FLT_MAX;
        for (const auto& v : action.FreeForm.vertices) {
            left = min(left, v.x);
            top = min(top, v.y);
            right = max(right, v.x);
            bottom = max(bottom, v.y);
        }
        return D2D1::RectF(left, top, right, bottom);
    }

    default:
        return D2D1::RectF(0, 0, 0, 0);
    }
}

void DrawResizeHandles(
    ID2D1DeviceContext* dc,
    const D2D1_RECT_F& r,
    ID2D1Brush* fill,
    ID2D1Brush* border
)
{
    constexpr float HANDLE_SIZE = 6.0f;
    constexpr float HANDLE_HALF = HANDLE_SIZE * 0.5f;

    auto drawHandle = [&](float x, float y)
        {
            D2D1_RECT_F h = D2D1::RectF(
                x - HANDLE_HALF,
                y - HANDLE_HALF,
                x + HANDLE_HALF,
                y + HANDLE_HALF
            );

            dc->FillRectangle(h, fill);
            dc->DrawRectangle(h, border, 1.0f);
        };

    float cx = (r.left + r.right) * 0.5f;
    float cy = (r.top + r.bottom) * 0.5f;

    drawHandle(r.left, r.top);    // TL
    drawHandle(cx, r.top);    // TM
    drawHandle(r.right, r.top);    // TR

    drawHandle(r.left, cy);       // ML
    drawHandle(r.right, cy);       // MR

    drawHandle(r.left, r.bottom); // BL
    drawHandle(cx, r.bottom); // BM
    drawHandle(r.right, r.bottom); // BR
}


void HRenderAction(ACTION& action, Microsoft::WRL::ComPtr<ID2D1DeviceContext> deviceContext, D2D1_COLOR_F customColor = COLOR_UNDEFINED) {

    D2D1_COLOR_F color = HGetRGBColor(action.Color);
    D2D1_COLOR_F fillColor = HGetRGBColor(action.FillColor);

    if (customColor.r != COLOR_UNDEFINED.r ||
        customColor.g != COLOR_UNDEFINED.g ||
        customColor.b != COLOR_UNDEFINED.b ||
        customColor.a != COLOR_UNDEFINED.a) {
        color = D2D1::ColorF(customColor.r, customColor.g, customColor.b, customColor.a);
    }

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> fillBrush;

    if (brush == nullptr) {
        deviceContext->CreateSolidColorBrush(color, &brush);
    }
    else {
        brush->SetColor(color);
    }

    if (fillBrush == nullptr) {
        deviceContext->CreateSolidColorBrush(fillColor, &fillBrush);
    }
    else {
        fillBrush->SetColor(fillColor);
    }

    if (deviceContext == pRenderTarget) {
        if (pBrush == nullptr) {
            pRenderTarget->CreateSolidColorBrush(color, &pBrush);
        }
        else {
            pBrush->SetColor(color);
        }

        brush = pBrush;
    }

    switch (action.Tool) {
    case TEraser:
        deviceContext->PushAxisAlignedClip(action.Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        deviceContext->Clear(D2D1::ColorF(1, 1, 1, 0));
        deviceContext->PopAxisAlignedClip();
        break;

    case TRectangle: {
        deviceContext->PushAxisAlignedClip(action.Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        deviceContext->FillRectangle(action.Position, brush.Get());
        deviceContext->PopAxisAlignedClip();
        break;
    }

    case TEllipse: {
        deviceContext->FillEllipse(action.Ellipse, brush.Get());
        break;
    }

    case TLine: {
        deviceContext->DrawLine(action.Line.startPoint, action.Line.endPoint, brush.Get(), action.BrushSize, nullptr);
        break;
    }

    case TBrush: {
        for (const auto& vertex : action.FreeForm.vertices) {
            if (action.isPixelMode) {
                int snappedLeft = static_cast<int>(vertex.x / pixelSizeRatio) * pixelSizeRatio;
                int snappedTop = static_cast<int>(vertex.y / pixelSizeRatio) * pixelSizeRatio;

                D2D1_RECT_F pixel = D2D1::RectF(
                    static_cast<float>(snappedLeft),
                    static_cast<float>(snappedTop),
                    static_cast<float>(snappedLeft + pixelSizeRatio),
                    static_cast<float>(snappedTop + pixelSizeRatio)
                );

                deviceContext->FillRectangle(pixel, brush.Get());
            }
            else {

                float scaledLeft = static_cast<float>(vertex.x);
                float scaledTop = static_cast<float>(vertex.y);
                float scaledBrushSize = static_cast<float>(currentBrushSize);
                float scaledPixelSizeRatio = static_cast<float>(pixelSizeRatio);

                D2D1_RECT_F rect = D2D1::RectF(
                    scaledLeft - scaledBrushSize * 0.5f,
                    scaledTop - scaledBrushSize * 0.5f,
                    scaledLeft + scaledBrushSize * 0.5f,
                    scaledTop + scaledBrushSize * 0.5f
                );

                deviceContext->DrawRectangle(rect, brush.Get());
                deviceContext->FillRectangle(rect, brush.Get());
            }
        }
        break;
    }

    case TPaintBucket: {
        deviceContext->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
        deviceContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
        for (const auto& p : action.pixelsToFill) {
            D2D1_RECT_F rect = D2D1::RectF((float)p.x, (float)p.y, (float)(p.x + 1), (float)(p.y + 1));
            deviceContext->FillRectangle(&rect, fillBrush.Get());
        }
        deviceContext->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
        deviceContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);        
        break;
    }

    case TWrite: {
        if (action.Text.empty()) break;

        Microsoft::WRL::ComPtr<IDWriteTextFormat> localTextFormat;

        pDWriteFactory->CreateTextFormat(
            action.FontFamily.c_str(),
            nullptr,
            static_cast<DWRITE_FONT_WEIGHT>(action.FontWeight),
            action.FontItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            static_cast<FLOAT>(action.FontSize) / 10.0f,   // if stored in tenths
            L"en-us",
            &localTextFormat
        );

        // Apply underline/strikethrough with a TextLayout if needed
        Microsoft::WRL::ComPtr<IDWriteTextLayout> textLayout;
        pDWriteFactory->CreateTextLayout(
            action.Text.c_str(),
            static_cast<UINT32>(action.Text.size()),
            localTextFormat.Get(),
            action.Position.right - action.Position.left,
            action.Position.bottom - action.Position.top,
            &textLayout
        );

        if (action.FontUnderline)
            textLayout->SetUnderline(TRUE, DWRITE_TEXT_RANGE{ 0, (UINT32)action.Text.size() });
        if (action.FontStrike)
            textLayout->SetStrikethrough(TRUE, DWRITE_TEXT_RANGE{ 0, (UINT32)action.Text.size() });

        // Draw it
        deviceContext->DrawTextLayout(
            D2D1::Point2F(action.Position.left, action.Position.top),
            textLayout.Get(),
            brush.Get()
        );
        break;
    }

    case TMove: {
        if (action.LastMovedPosition == true) {
            int target = action.TargetID;

            auto MoveAction = std::find_if(
                Actions.begin(),
                Actions.end(),
                [target](const ACTION& action) {
                    return action.Id == target;
                }
            );

			if (MoveAction == Actions.end()) break;

            if (MoveAction->Tool == TRectangle || MoveAction->Tool == TWrite) {
                MoveAction->Position = action.Position;
            }
            else if (MoveAction->Tool == TBrush) {
                MoveAction->FreeForm = action.FreeForm;
            }
            else if (MoveAction->Tool == TEllipse) {
                MoveAction->Ellipse = action.Ellipse;
            }
            else if (MoveAction->Tool == TLine) {
                MoveAction->Line = action.Line;
            }
        }
    }

    default:
        break;
    }

    ComPtr<ID2D1SolidColorBrush> handleFillBrush;
    ComPtr<ID2D1SolidColorBrush> handleBorderBrush;

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> highlightBrush;

    if (!highlightBrush)
    {
        deviceContext->CreateSolidColorBrush(
            D2D1::ColorF(0.2f, 0.6f, 1.0f, 0.8f), // azul selection vibe
            &highlightBrush
        );
    }

    if (!handleFillBrush)
    {
        deviceContext->CreateSolidColorBrush(
            D2D1::ColorF(1, 1, 1, 1),   // branco
            &handleFillBrush
        );

        deviceContext->CreateSolidColorBrush(
            D2D1::ColorF(0.2f, 0.6f, 1.0f, 1.0f), // azul selection
            &handleBorderBrush
        );
    }

    if (action.Id == selectedActionId)
    {
        D2D1_RECT_F bounds = GetActionBounds(action);

        // Margem pra não colar no desenho
        const float padding = 4.0f;
        bounds.left -= padding;
        bounds.top -= padding;
        bounds.right += padding;
        bounds.bottom += padding;

        deviceContext->DrawRectangle(
            bounds,
            highlightBrush.Get(),
            2.0f,
            nullptr
        );

        DrawResizeHandles(
            deviceContext.Get(),
            bounds,
            handleFillBrush.Get(),
            handleBorderBrush.Get()
        );

        D2D1_POINT_2F rotationHandle = {
            (bounds.left + bounds.right) * 0.5f,
            bounds.top - 20.0f
        };

        deviceContext->DrawEllipse(
            D2D1::Ellipse(rotationHandle, 6, 6),
            handleBorderBrush.Get(),
            2.0f
        );

    }
}

RenderData HCreateRenderData(D2D1_SIZE_U size) {
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> deviceContext;
    HRESULT hr = g_pD2DDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        deviceContext.GetAddressOf()
    );

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    deviceContext->CreateBitmap(size, nullptr, 0, &bitmapProperties, &targetBitmap);

    RenderData renderData = RenderData{
        size,
        deviceContext,
        nullptr,
        targetBitmap
    };

    return renderData;
}

RenderData HCreateRenderDataHWND(HWND hWnd) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    Microsoft::WRL::ComPtr<ID2D1DeviceContext> deviceContext;
    HRESULT hr = g_pD2DDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        deviceContext.GetAddressOf()
    );

    DXGI_SWAP_CHAIN_DESC1 swapDesc = TSetSwapChainDescription(size.width, size.height, DXGI_ALPHA_MODE_IGNORE);

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    hr = g_dxgiFactory->CreateSwapChainForHwnd(
        g_pD3DDevice.Get(),
        hWnd,
        &swapDesc,
        nullptr, nullptr,
        &swapChain
    );

    Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
    swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)
    );

    deviceContext->CreateBitmapFromDxgiSurface(
        backBuffer.Get(),
        &bitmapProperties,
        &targetBitmap
    );

    RenderData renderData = RenderData{
        size,
        deviceContext,
        swapChain,
        targetBitmap
    };
    
    return renderData;
}

void CreateGlobalRenderBitmap() {
    ComPtr<IDXGISurface> backBuffer;
    HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(IDXGISurface), &backBuffer);

    ComPtr<ID3D11Resource> backBufferResource;
    // Converte o IDXGISurface para um recurso D3D11 (ID3D11Resource ou ID3D11Texture2D)
    hr = backBuffer.As(&backBufferResource);
    if (FAILED(hr)) return;

    // Cria a visão que permite ao D3D11 limpar/renderizar no back buffer
    // Usa ReleaseAndGetAddressOf() para liberar a visão anterior (se houver) e armazenar a nova
    hr = g_pD3DDevice->CreateRenderTargetView(
        backBufferResource.Get(),
        nullptr, // Usa as configurações padrão
        g_pRenderTargetView.ReleaseAndGetAddressOf() // Armazena a nova visão
    );
    if (FAILED(hr)) return;

    FLOAT dpiX, dpiY;
    pRenderTarget->GetDpi(&dpiX, &dpiY);

    D2D1_BITMAP_PROPERTIES1 bitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpiX, dpiY
    );

    hr = pRenderTarget->CreateBitmapFromDxgiSurface(backBuffer.Get(), &bitmapProps, pD2DTargetBitmap.ReleaseAndGetAddressOf());
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

    RECT rc;
    GetClientRect(highlightFrame, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    Microsoft::WRL::ComPtr<ID2D1DeviceContext> replayDeviceContext;
    HRESULT hr = g_pD2DDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        replayDeviceContext.GetAddressOf()
    );

    DXGI_SWAP_CHAIN_DESC1 swapDesc = TSetSwapChainDescription(size.width, size.height, DXGI_ALPHA_MODE_IGNORE);

    Microsoft::WRL::ComPtr<IDXGISwapChain1> replaySwapChain;
    hr = g_dxgiFactory->CreateSwapChainForHwnd(
        g_pD3DDevice.Get(),
        highlightFrame,
        &swapDesc,
        nullptr, nullptr,
        &replaySwapChain
    );

    Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
    replaySwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    replayDeviceContext->CreateBitmapFromDxgiSurface(
        backBuffer.Get(),
        &bitmapProperties,
        &targetBitmap
    );

    replayDeviceContext->SetTarget(targetBitmap.Get());

    replayDeviceContext->BeginDraw();
    replayDeviceContext->Clear(D2D1::ColorF(D2D1::ColorF::White, 1));

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> highlightReplayBrush;

    D2D1_COLOR_F color = D2D1::ColorF(0.00f, 0.00f, 1.00f, 1);

    if (highlightReplayBrush == nullptr) {
        replayDeviceContext->CreateSolidColorBrush(color, &highlightReplayBrush);
    }
    else {
        highlightReplayBrush->SetColor(color);
    }

    replayDeviceContext->FillRectangle(D2D1::RectF(rc.left, rc.top, rc.right, rc.bottom), highlightReplayBrush.Get());
    replayDeviceContext->EndDraw();

    replaySwapChain->Present(1, 0);
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
        replayPartialStepCount = 0;

        ReplayActions = Actions;
        ReplayRedoActions = RedoActions;

        std::vector<ACTION> filteredActions;

        for (int i = 0; i < Actions.size(); i++) {
            if (Actions[i].Tool == TLayer) continue;
            filteredActions.push_back(Actions[i]);
        }

        std::reverse(filteredActions.begin(), filteredActions.end());

        RedoActions = filteredActions;
        Actions.clear();

        for (int i = 0; i < TimelineFrameButtons.size(); i++) {
            DestroyWindow(TimelineFrameButtons[i].value().frame);
            TimelineFrameButtons[i].reset();
        }

        TimelineFrameButtons.clear();

        int totalFrames = static_cast<int>(RedoActions.size()) + 1;  // N+1 frames (0 vazio)

        for (int i = 0; i < totalFrames; i++) {
            TCreateReplayFrame(i);
        }
        
        HCreateHighlightFrame();
        HSetTimelineHighlight();
        TReplayClearLayers();
        TReplayRender();
    }
    else {
        replayPartialStepCount = 0;
        Actions.clear();
        RedoActions.clear();

        Actions = ReplayActions;
        RedoActions = ReplayRedoActions;
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

D2D1_COLOR_F HGetRGBColor(COLORREF color) {
    float r = (color & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = ((color >> 16) & 0xFF) / 255.0f;
    float a = 1.0f;

    D2D1_COLOR_F RGBColor = { r, g, b, a };

    return RGBColor;
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
    pBrush.Reset();
    pRenderTarget.Reset();
    hWndLayerRenderTarget.Reset();
    pRenderTargetLayer.Reset();
    pD2DFactory.Reset();
    pD2DTargetBitmap.Reset();
    pDWriteFactory.Reset();
    g_pRenderTargetView.Reset();

    for (auto& layer : layers) {
        if (layer.has_value()) {
            layer.value().pBitmap.Reset();
        }
    }

    for (auto& layer : layerBitmaps) {
        layer.pBitmap.Reset();
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

    // Release D3D resources
    g_pSwapChain.Reset();
    g_pD2DDevice.Reset();
    g_pD3DContext.Reset();
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
   
    int itemWidth = btnWidth;
    int itemHeight = btnHeight;

    g_scrollOffsetTimeline += direction * delta;

    RECT rcParent;
    GetClientRect(timelineHWND, &rcParent);

    int timelineParentWidth = rcParent.right - rcParent.left;

    int index = 0;

    for (size_t i = 0; i < TimelineFrameButtons.size(); i++) {
        if (!TimelineFrameButtons[i].has_value()) continue;
        //int currentFrameIndex = TimelineFrameButtons[i].value().FrameIndex;

        int x = ((timelineParentWidth / 2) - (itemWidth / 2) + (itemWidth * index)) - g_scrollOffsetTimeline;
        int y = 10;

        index++;

        MoveWindow(TimelineFrameButtons[i].value().frame,
            x, y, itemWidth, itemHeight, TRUE);
    }
}

void HSetScrollPosition(int index) {
    
    int delta = btnWidth;

    int itemWidth = btnWidth;
    int itemHeight = btnHeight;

    g_scrollOffsetTimeline = index * delta;

    RECT rcParent;
    GetClientRect(timelineHWND, &rcParent);

    int timelineParentWidth = rcParent.right - rcParent.left;

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
