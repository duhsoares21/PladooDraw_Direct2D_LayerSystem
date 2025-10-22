#include "pch.h"
#include "Base.h"
#include "Constants.h"
#include "Helpers.h"
#include "Layers.h"
#include "Tools.h"
#include "Replay.h"
#include "Animation.h"

/* TOOLS AUX */

void THandleMouseUp() {
    if (!layers[layerIndex].has_value()) return;

    if (isDrawingRectangle || isDrawingEllipse || isDrawingLine || isDrawingWindowText) {

        ComPtr<ID2D1SolidColorBrush> brush;
        pRenderTarget->CreateSolidColorBrush(HGetRGBColor(currentColor), &brush);

        auto it = std::find_if(
            layers.begin(),
            layers.end(),
            [](const std::optional<Layer>& optLayer) {
                return optLayer.has_value() && optLayer->LayerID == layerIndex && optLayer->FrameIndex == CurrentFrameIndex;
            }
        );

        pRenderTarget->SetTarget(it->value().pBitmap.Get());
        pRenderTarget->BeginDraw();
        pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

        ACTION action;

        if (isDrawingRectangle) {
            pRenderTarget->PushAxisAlignedClip(rectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            pRenderTarget->FillRectangle(rectangle, brush.Get());
            pRenderTarget->PopAxisAlignedClip();

            action.Tool = TRectangle;
            action.Layer = layerIndex;
            action.FrameIndex = CurrentFrameIndex;
            action.Position = rectangle;
            action.Color = currentColor;

            rectangle = D2D1::RectF(0, 0, 0, 0);
            //isDrawingRectangle = false;
        }

        if (isDrawingEllipse) {
            pRenderTarget->FillEllipse(ellipse, brush.Get());

            action.Tool = TEllipse;
            action.Layer = layerIndex;
            action.FrameIndex = CurrentFrameIndex;
            action.Position = rectangle;
            action.Ellipse = ellipse;
            action.Color = currentColor;

            ellipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);
            //isDrawingEllipse = false;
        }

        if (isDrawingLine) {
            pRenderTarget->DrawLine(startPoint, endPoint, brush.Get(), currentBrushSize, nullptr);

            action.Tool = TLine;
            action.Layer = layerIndex;
            action.FrameIndex = CurrentFrameIndex;
            action.Line = { startPoint, endPoint };
            action.Color = currentColor;
            action.BrushSize = currentBrushSize;

            //isDrawingLine = false;
        }

        if (isDrawingWindowText) {
            if (!isWritingText) {
                float scaledLeft = static_cast<float>(textArea.left) / zoomFactor;
                float scaledTop = static_cast<float>(textArea.top) / zoomFactor;
                float scaledRight = static_cast<float>(textArea.right) / zoomFactor;
                float scaledBottom = static_cast<float>(textArea.bottom) / zoomFactor;

                int width = static_cast<int>(scaledRight - scaledLeft);
                int height = static_cast<int>(scaledBottom - scaledTop);

                TInitTextTool(scaledLeft, scaledTop, width, height);

                RedrawWindow(docHWND, nullptr, nullptr, RDW_UPDATENOW | RDW_FRAME | RDW_ALLCHILDREN);

                isWritingText = true;
            }
        }

        pRenderTarget->EndDraw();

        if (isDrawingRectangle || isDrawingEllipse || isDrawingLine) {
            layersOrder.pop_back();
            if (layers[TLayersCount() - 1].has_value()) {
                layers[TLayersCount() - 1].reset();
            }
            layers.pop_back();
            Actions.pop_back();

            isDrawingRectangle = false;
            isDrawingEllipse = false;
            isDrawingLine = false;
        }

        Actions.push_back(action);
        action = ACTION();
    }

    if (isDrawingBrush) {
        ACTION action;
        action.Tool = TBrush;
        action.Layer = layerIndex;
        action.FrameIndex = CurrentFrameIndex;
        action.Color = currentColor;
        action.BrushSize = currentBrushSize;
        action.FillColor = RGB(255, 255, 255);
        action.FreeForm.vertices = Vertices;
        action.IsFilled = false;
        action.isPixelMode = isPixelMode;
        Actions.push_back(action);

        Vertices.clear();

        isDrawingBrush = false;
    }

    startPoint = D2D1::Point2F(0, 0);
    endPoint = D2D1::Point2F(0, 0);

    bitmapRect = D2D1::RectF(0, 0, 0, 0);
    prevRectangle = D2D1::RectF(0, 0, 0, 0);
    prevEllipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);
    prevLeft = -1;
    prevTop = -1;

    if (selectedAction) {
        TUnSelectTool();
    }
    
    TUpdateLayers(layerIndex, CurrentFrameIndex);
    
    TUpdateAnimation();
    
    TRenderLayers();
}

void TUndo() {

    auto it = std::find_if(
        Actions.rbegin(),
        Actions.rend(),
        [](const ACTION& act) {
            return act.Layer == layerIndex && act.FrameIndex == CurrentFrameIndex;
        }
    );

    if (it != Actions.rend()) {
        // converter iterator reverso para iterator normal para erase
        auto normalIt = std::prev(it.base());

        RedoActions.push_back(*normalIt);
        Actions.erase(normalIt);

        // TLayer check
        ACTION& lastAction = RedoActions.back();
        if (lastAction.Tool == TLayer && !isAnimationMode) {
            if (layers[lastAction.Layer].has_value()) {
                lastAction.isLayerVisible = 0;
                layers[lastAction.Layer].value().isActive = false;
                ShowWindow(LayerButtons[lastAction.Layer].value().button, SW_HIDE);
            }
        }
    }

    for (size_t i = 0; i < layers.size(); i++)
    {
        if (!layers[i].has_value()) continue;
        TUpdateLayers(layers[i].value().LayerID, layers[i].value().FrameIndex);
    }

    TRenderLayers();
    if (isAnimationMode) {
        TUpdateAnimation();
        TRenderAnimation();
    }
}

void TRedo() {
    if (RedoActions.empty()) return; // nada a refazer

    // Pega a última ação adicionada ao RedoActions (LIFO)
    ACTION lastAction = RedoActions.back();
    RedoActions.pop_back(); // remove do redo

    // Adiciona novamente ao Actions
    Actions.push_back(lastAction);

    // Se for TLayer, reativa o layer
    if (lastAction.Tool == TLayer && !isAnimationMode) {
        if (layers[lastAction.Layer].has_value()) {
            lastAction.isLayerVisible = 1;
            layers[lastAction.Layer].value().isActive = true;
            ShowWindow(LayerButtons[lastAction.Layer].value().button, SW_SHOW);
        }
    }

    // Atualiza todos os layers (mesma lógica do TUndo)
    for (size_t i = 0; i < layers.size(); ++i) {
        if (!layers[i].has_value()) continue;
        TUpdateLayers(layers[i].value().LayerID, layers[i].value().FrameIndex);
    }

    TRenderLayers();

    if (isAnimationMode) {
        TUpdateAnimation();
        TRenderAnimation();
    }
}

/* MOVE TOOL AUX */

bool IsPointNearSegment(float px, float py, float x1, float y1, float x2, float y2, float tolerance = 5.0f) {
    float dx = x2 - x1;
    float dy = y2 - y1;

    if (dx == 0 && dy == 0) {
        return (std::hypot(px - x1, py - y1) <= tolerance);
    }

    float t = ((px - x1) * dx + (py - y1) * dy) / (dx * dx + dy * dy);
    t = (std::max)(0.0f, (std::min)(1.0f, t));

    float projX = x1 + t * dx;
    float projY = y1 + t * dy;

    float distance = std::hypot(px - projX, py - projY);
    return (distance <= tolerance);
}

bool IsPointNearEdge(const std::vector<VERTICE> vertices, float px, float py) {
    for (size_t i = 1; i < vertices.size(); ++i) {
        float x1 = vertices[i - 1].x;
        float y1 = vertices[i - 1].y;
        float x2 = vertices[i].x;
        float y2 = vertices[i].y;

        if (IsPointNearSegment(px, py, x1, y1, x2, y2)) {
            return true;
        }
    }
    return false;
}

bool IsPointInsideRect(const D2D1_RECT_F& rect, float x, float y) {
    return (x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom);
}

bool IsPointInsideEllipse(const D2D1_ELLIPSE& ellipse, float x, float y) {
    float dx = x - ellipse.point.x;
    float dy = y - ellipse.point.y;
    return ((dx * dx) / (ellipse.radiusX * ellipse.radiusX) + (dy * dy) / (ellipse.radiusY * ellipse.radiusY)) <= 1.0f;
}

bool IsPointNearLine(const D2D1_RECT_F& lineRect, float x, float y, float tolerance) {
    // lineRect.left/top = ponto A
    // lineRect.right/bottom = ponto B
    float x1 = lineRect.left, y1 = lineRect.top;
    float x2 = lineRect.right, y2 = lineRect.bottom;

    float A = x - x1;
    float B = y - y1;
    float C = x2 - x1;
    float D = y2 - y1;

    float dot = A * C + B * D;
    float len_sq = C * C + D * D;
    float param = len_sq != 0 ? dot / len_sq : -1;

    float xx, yy;
    if (param < 0) {
        xx = x1;
        yy = y1;
    }
    else if (param > 1) {
        xx = x2;
        yy = y2;
    }
    else {
        xx = x1 + param * C;
        yy = y1 + param * D;
    }

    float dx = x - xx;
    float dy = y - yy;
    return (dx * dx + dy * dy) <= (tolerance * tolerance);
}

bool HitTestAction(const ACTION& action, float x, float y) {
    switch (action.Tool) {
    case TBrush:
        return IsPointNearEdge(action.FreeForm.vertices, x, y);
    case TRectangle:
    case TWrite:
        return IsPointInsideRect(action.Position, x, y);
    case TEllipse:
        return IsPointInsideEllipse(action.Ellipse, x, y);
    case TLine:
        return IsPointNearLine(action.Position, x, y, 5.0f); // 5 px tolerance
    default:
        return false;
    }
}

/*FLOODFILL AUX*/

std::vector<COLORREF> CaptureCanvasPixels() {
    // Validate render target and dimensions
    if (!pRenderTarget || logicalWidth <= 0 || logicalHeight <= 0) {
        OutputDebugStringW(L"CaptureCanvasPixels: Invalid render target or dimensions\n");
        return {};
    }

    // Create a render-target bitmap (GPU-resident)
    D2D1_SIZE_U size = D2D1::SizeU(static_cast<UINT32>(logicalWidth), static_cast<UINT32>(logicalHeight));
    D2D1_BITMAP_PROPERTIES1 renderBitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f // Standard DPI
    );

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> renderBitmap;
    HRESULT hr = pRenderTarget->CreateBitmap(size, nullptr, 0, &renderBitmapProps, &renderBitmap);
    if (FAILED(hr)) {
        OutputDebugStringW((L"CaptureCanvasPixels: Failed to create render bitmap, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return {};
    }

    // Create a temporary device context for rendering
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> captureContext;
    hr = g_pD2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &captureContext);
    if (FAILED(hr)) {
        OutputDebugStringW((L"CaptureCanvasPixels: Failed to create device context, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return {};
    }

    // Render layers to the render bitmap
    captureContext->SetTarget(renderBitmap.Get());
    captureContext->BeginDraw();
    captureContext->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f)); // Match TRenderLayers

    D2D1_RECT_F destRect = D2D1::RectF(0, 0, logicalWidth, logicalHeight);

    auto it = std::find_if(
        layers.begin(),
        layers.end(),
        [](const std::optional<Layer>& optLayer) {
            return optLayer.has_value() && optLayer->LayerID == layerIndex && optLayer->FrameIndex == CurrentFrameIndex;
        }
    );

    if (it->has_value()) {
        captureContext->DrawBitmap(it->value().pBitmap.Get(), destRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
    }

    hr = captureContext->EndDraw();
    if (FAILED(hr)) {
        OutputDebugStringW((L"CaptureCanvasPixels: EndDraw failed, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return {};
    }

    // Create a CPU-readable bitmap
    D2D1_BITMAP_PROPERTIES1 cpuBitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f
    );

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> cpuBitmap;
    hr = pRenderTarget->CreateBitmap(size, nullptr, 0, &cpuBitmapProps, &cpuBitmap);
    if (FAILED(hr)) {
        OutputDebugStringW((L"CaptureCanvasPixels: Failed to create CPU-readable bitmap, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return {};
    }

    // Copy render bitmap to CPU-readable bitmap
    hr = cpuBitmap->CopyFromBitmap(nullptr, renderBitmap.Get(), nullptr);
    if (FAILED(hr)) {
        OutputDebugStringW((L"CaptureCanvasPixels: Failed to copy bitmap, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return {};
    }

    // Map the CPU-readable bitmap
    D2D1_MAPPED_RECT mappedRect;
    hr = cpuBitmap->Map(D2D1_MAP_OPTIONS_READ, &mappedRect);
    if (FAILED(hr)) {
        OutputDebugStringW((L"CaptureCanvasPixels: Failed to map CPU-readable bitmap, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return {};
    }

    // Convert to COLORREF (BGRA to RGB)
    std::vector<COLORREF> pixels(static_cast<size_t>(logicalWidth) * logicalHeight);
    BYTE* src = mappedRect.bits;
    for (size_t i = 0; i < pixels.size(); ++i) {
        BYTE b = src[i * 4 + 0];
        BYTE g = src[i * 4 + 1];
        BYTE r = src[i * 4 + 2];
        BYTE a = src[i * 4 + 3];
        pixels[i] = RGB(r, g, b); // Ignore alpha for COLORREF
    }

    cpuBitmap->Unmap();
    OutputDebugStringW((L"CaptureCanvasPixels: Captured " + std::to_wstring(logicalWidth) + L"x" +
        std::to_wstring(logicalHeight) + L" pixels\n").c_str());
    return pixels;
}