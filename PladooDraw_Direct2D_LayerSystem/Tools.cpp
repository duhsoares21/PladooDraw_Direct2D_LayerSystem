#include "pch.h"
#include "Structs.h"
#include "Constants.h"
#include "Helpers.h"
#include "Tools.h"
#include "Layers.h"

/* TOOLS AUX */

void THandleMouseUp() {

    if (isDrawingRectangle || isDrawingEllipse || isDrawingLine) {

        ComPtr<ID2D1SolidColorBrush> brush;
        pRenderTarget->CreateSolidColorBrush(HGetRGBColor(currentColor), &brush);

        layers[layerIndex].pBitmapRenderTarget->BeginDraw();

        ACTION action;

        if (isDrawingRectangle) {
            layers[layerIndex].pBitmapRenderTarget->PushAxisAlignedClip(rectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            layers[layerIndex].pBitmapRenderTarget->FillRectangle(rectangle, brush.Get());
            layers[layerIndex].pBitmapRenderTarget->PopAxisAlignedClip();

            action.Tool = TRectangle;
            action.Layer = layerIndex;
            action.Position = rectangle;
            action.Color = currentColor;

            rectangle = D2D1::RectF(0, 0, 0, 0);
            isDrawingRectangle = false;
        }

        if (isDrawingEllipse) {
            layers[layerIndex].pBitmapRenderTarget->FillEllipse(ellipse, brush.Get());

            action.Tool = TEllipse;
            action.Layer = layerIndex;
            action.Position = rectangle;
            action.Ellipse = ellipse;
            action.Color = currentColor;

            ellipse = D2D1::Ellipse(D2D1::Point2F(0, 0), 0, 0);
            isDrawingEllipse = false;
        }

        if (isDrawingLine) {
            layers[layerIndex].pBitmapRenderTarget->DrawLine(startPoint, endPoint, brush.Get(), currentBrushSize, nullptr);

            action.Tool = TLine;
            action.Layer = layerIndex;
            action.Line = { startPoint, endPoint };
            action.Color = currentColor;
            action.BrushSize = currentBrushSize;

            isDrawingLine = false;
        }

        layers[layerIndex].pBitmapRenderTarget->EndDraw();

        layersOrder.pop_back();
        layers.pop_back();
        Actions.push_back(action);
        action = ACTION();
    }

    if (isDrawingBrush) {
        ACTION action;
        action.Tool = TBrush;
        action.Layer = layerIndex;
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

    TRenderLayers();
    TDrawLayerPreview(layerIndex);
}

void TUndo() {
    if (Actions.size() > 0) {
        ACTION lastAction = Actions.back();
        RedoActions.push_back(lastAction);
        Actions.pop_back();

        TUpdateLayers(layerIndex);
        TRenderLayers();
        TDrawLayerPreview(layerIndex);
    }
}

void TRedo() {
    if (RedoActions.size() > 0) {
        ACTION action = RedoActions.back();
        Actions.push_back(action);
        RedoActions.pop_back();

        TUpdateLayers(layerIndex);
        TRenderLayers();
        TDrawLayerPreview(layerIndex);
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

std::vector<COLORREF> CaptureWindowPixels(HWND hWnd, int width, int height) {
    HDC hdcWindow = GetDC(hWnd);
    HDC hdcMemDC = CreateCompatibleDC(hdcWindow);

    HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, width, height);

    SelectObject(hdcMemDC, hBitmap);

    BitBlt(hdcMemDC, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    std::vector<COLORREF> pixels(width * height);
    GetDIBits(hdcMemDC, hBitmap, 0, height, pixels.data(), &bmi, DIB_RGB_COLORS);

    DeleteObject(hBitmap);
    DeleteDC(hdcMemDC);
    ReleaseDC(hWnd, hdcWindow);

    return pixels;
}

/* TOOLS */

void TEraserTool(int left, int top) {
    if (prevLeft == -1 && prevTop == -1) {
        prevLeft = static_cast<float>(left) / zoomFactor;
        prevTop = static_cast<float>(top) / zoomFactor;
    }

    auto target = layers[layerIndex].pBitmapRenderTarget;

    target->BeginDraw();

    // Scale coordinates and size
    float scaledLeft = static_cast<float>(left) / zoomFactor;
    float scaledTop = static_cast<float>(top) / zoomFactor;
    float scaledBrushSize = static_cast<float>(currentEraserSize) / zoomFactor;

    float dx = scaledLeft - prevLeft;
    float dy = scaledTop - prevTop;
    float distance = sqrt(dx * dx + dy * dy);

    float step = scaledBrushSize * 0.5f;

    if (distance > step) {
        float steps = distance / step;
        float deltaX = dx / steps;
        float deltaY = dy / steps;

        for (float i = 0; i < steps; i++) {
            float x = prevLeft + i * deltaX;
            float y = prevTop + i * deltaY;

            D2D1_RECT_F rect = D2D1::RectF(
                x - scaledBrushSize,
                y - scaledBrushSize,
                x + scaledBrushSize,
                y + scaledBrushSize
            );

            target->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            target->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
            target->PopAxisAlignedClip();

            if (x != prevLeft || y != prevTop) {
                if (x != -1 && y != -1) {
                    ACTION action;
                    action.Tool = 0;
                    action.Layer = layerIndex;
                    action.Position = rect;
                    action.BrushSize = scaledBrushSize;
                    action.IsFilled = false;
                    Actions.emplace_back(action);
                }
            }
        }
    }

    D2D1_RECT_F rect = D2D1::RectF(
        scaledLeft - scaledBrushSize,
        scaledTop - scaledBrushSize,
        scaledLeft + scaledBrushSize,
        scaledTop + scaledBrushSize
    );

    target->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    target->Clear(D2D1::ColorF(0, 0, 0, 0));
    target->PopAxisAlignedClip();

    if (scaledLeft != prevLeft || scaledTop != prevTop) {
        if (scaledLeft != -1 && scaledTop != -1) {
            ACTION action;
            action.Tool = 0;
            action.Layer = layerIndex;
            action.Position = rect;
            action.BrushSize = scaledBrushSize;
            action.IsFilled = false;
            Actions.emplace_back(action);
        }
    }

    prevLeft = scaledLeft;
    prevTop = scaledTop;

    target->EndDraw();
}

void TBrushTool(int left, int top, COLORREF hexColor, bool pixelMode, int pPixelSizeRatio) {
    ComPtr<ID2D1SolidColorBrush> brush;

    if (pixelSizeRatio == -1) {
        pixelSizeRatio = pPixelSizeRatio;
    }

    currentColor = hexColor;
    isDrawingBrush = true;
    isPixelMode = pixelMode;

    pRenderTarget->CreateSolidColorBrush(HGetRGBColor(hexColor), &brush);

    pRenderTarget->BeginDraw();
    layers[layerIndex].pBitmapRenderTarget->BeginDraw();

    // Scale coordinates and sizes
    float scaledLeft = static_cast<float>(left) / zoomFactor;
    float scaledTop = static_cast<float>(top) / zoomFactor;
    float scaledBrushSize = static_cast<float>(currentBrushSize) / zoomFactor;
    float scaledPixelSizeRatio = static_cast<float>(pixelSizeRatio) / zoomFactor;

    if (pixelMode) {
        // PIXEL MODE: 1x1 rectangles only
        int snappedLeft = static_cast<int>(scaledLeft / scaledPixelSizeRatio) * scaledPixelSizeRatio;
        int snappedTop = static_cast<int>(scaledTop / scaledPixelSizeRatio) * scaledPixelSizeRatio;

        D2D1_RECT_F pixel = D2D1::RectF(
            static_cast<float>(snappedLeft),
            static_cast<float>(snappedTop),
            static_cast<float>(snappedLeft + scaledPixelSizeRatio),
            static_cast<float>(snappedTop + scaledPixelSizeRatio)
        );

        layers[layerIndex].pBitmapRenderTarget->FillRectangle(pixel, brush.Get());
        Vertices.emplace_back(VERTICE{ static_cast<float>(snappedLeft), static_cast<float>(snappedTop), static_cast<int>(currentBrushSize) });
    }
    else {
        if (prevLeft == -1 && prevTop == -1) {
            prevLeft = scaledLeft;
            prevTop = scaledTop;
        }

        // NORMAL MODE (smooth brush stroke)
        float dx = scaledLeft - prevLeft;
        float dy = scaledTop - prevTop;
        float distance = sqrt(dx * dx + dy * dy);
        float step = scaledBrushSize * 0.5f;

        if (distance > step) {
            float steps = distance / step;
            float deltaX = dx / steps;
            float deltaY = dy / steps;

            for (float i = 0; i < steps; i++) {
                float x = prevLeft + i * deltaX;
                float y = prevTop + i * deltaY;

                D2D1_RECT_F rect = D2D1::RectF(
                    x - scaledBrushSize * 0.5f,
                    y - scaledBrushSize * 0.5f,
                    x + scaledBrushSize * 0.5f,
                    y + scaledBrushSize * 0.5f
                );

                layers[layerIndex].pBitmapRenderTarget->DrawRectangle(rect, brush.Get());
                layers[layerIndex].pBitmapRenderTarget->FillRectangle(rect, brush.Get());

                /*D2D1_ELLIPSE ellipse = D2D1::Ellipse(
                    D2D1::Point2F(x - scaledBrushSize, y - scaledBrushSize),
                    scaledBrushSize / 2.0f,
                    scaledBrushSize / 2.0f
                );

                layers[layerIndex].pBitmapRenderTarget->FillEllipse(ellipse, brush.Get());*/

                if (x != -1 && y != -1) {
                    Vertices.emplace_back(VERTICE{ x, y });
                }
            }
        }

        D2D1_RECT_F rect = D2D1::RectF(
            scaledLeft - scaledBrushSize * 0.5f,
            scaledTop - scaledBrushSize * 0.5f,
            scaledLeft + scaledBrushSize * 0.5f,
            scaledTop + scaledBrushSize * 0.5f
        );

        layers[layerIndex].pBitmapRenderTarget->DrawRectangle(rect, brush.Get());
        layers[layerIndex].pBitmapRenderTarget->FillRectangle(rect, brush.Get());

        /*D2D1_ELLIPSE ellipse = D2D1::Ellipse(
            D2D1::Point2F(scaledLeft - scaledBrushSize, scaledTop - scaledBrushSize),
            scaledBrushSize / 2.0f,
            scaledBrushSize / 2.0f
        );

        layers[layerIndex].pBitmapRenderTarget->FillEllipse(ellipse, brush.Get());*/

        if (scaledLeft != -1 && scaledTop != -1) {
            Vertices.emplace_back(VERTICE{ static_cast<float>(scaledLeft), static_cast<float>(scaledTop), static_cast<int>(currentBrushSize) });
        }
    }

    layers[layerIndex].pBitmapRenderTarget->EndDraw();
    pRenderTarget->EndDraw();

    prevLeft = scaledLeft;
    prevTop = scaledTop;
}

void TRectangleTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    ComPtr<ID2D1SolidColorBrush> brush;
    pRenderTarget->CreateSolidColorBrush(HGetRGBColor(hexColor), &brush);

    if (!isDrawingRectangle) {
        TAddLayer(false);
    }

    isDrawingRectangle = true;

    currentColor = hexColor;

    layers[TLayersCount() - 1].pBitmapRenderTarget->BeginDraw();

    // Scale coordinates
    float scaledLeft = static_cast<float>(left) / zoomFactor;
    float scaledTop = static_cast<float>(top) / zoomFactor;
    float scaledRight = static_cast<float>(right) / zoomFactor;
    float scaledBottom = static_cast<float>(bottom) / zoomFactor;

    rectangle = D2D1::RectF(scaledLeft, scaledTop, scaledRight, scaledBottom);

    if (prevRectangle.left != 0 || prevRectangle.top != 0 || prevRectangle.right != 0 || prevRectangle.bottom != 0) {
        layers[TLayersCount() - 1].pBitmapRenderTarget->PushAxisAlignedClip(prevRectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        layers[TLayersCount() - 1].pBitmapRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
        layers[TLayersCount() - 1].pBitmapRenderTarget->PopAxisAlignedClip();
    }

    layers[TLayersCount() - 1].pBitmapRenderTarget->PushAxisAlignedClip(rectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    layers[TLayersCount() - 1].pBitmapRenderTarget->FillRectangle(rectangle, brush.Get());
    layers[TLayersCount() - 1].pBitmapRenderTarget->PopAxisAlignedClip();

    prevRectangle = D2D1::RectF(scaledLeft, scaledTop, scaledRight, scaledBottom);

    layers[TLayersCount() - 1].pBitmapRenderTarget->EndDraw();
}

void TEllipseTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    ComPtr<ID2D1SolidColorBrush> brush;
    pRenderTarget->CreateSolidColorBrush(HGetRGBColor(hexColor), &brush);

    if (!isDrawingEllipse) {
        TAddLayer(false);
    }

    isDrawingEllipse = true;

    currentColor = hexColor;

    layers[TLayersCount() - 1].pBitmapRenderTarget->BeginDraw();

    // Scale coordinates
    float scaledLeft = static_cast<float>(left) / zoomFactor;
    float scaledTop = static_cast<float>(top) / zoomFactor;
    float scaledRight = static_cast<float>(right) / zoomFactor;
    float scaledBottom = static_cast<float>(bottom) / zoomFactor;

    ellipse = D2D1::Ellipse(
        D2D1::Point2F(scaledLeft, scaledTop),
        abs(scaledRight - scaledLeft) / 2.0f,
        abs(scaledBottom - scaledTop) / 2.0f
    );

    if (prevEllipse.point.x != 0 || prevEllipse.point.y != 0 || prevEllipse.radiusX != 0 || prevEllipse.radiusY != 0) {
        D2D1_RECT_F prevRect = D2D1::RectF(
            prevEllipse.point.x - prevEllipse.radiusX,
            prevEllipse.point.y - prevEllipse.radiusY,
            prevEllipse.point.x + prevEllipse.radiusX,
            prevEllipse.point.y + prevEllipse.radiusY
        );

        layers[TLayersCount() - 1].pBitmapRenderTarget->PushAxisAlignedClip(prevRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        layers[TLayersCount() - 1].pBitmapRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
        layers[TLayersCount() - 1].pBitmapRenderTarget->PopAxisAlignedClip();
    }

    layers[TLayersCount() - 1].pBitmapRenderTarget->FillEllipse(ellipse, brush.Get());

    prevEllipse = ellipse;

    layers[TLayersCount() - 1].pBitmapRenderTarget->EndDraw();
}

void TLineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor) {
    ComPtr<ID2D1SolidColorBrush> brush;
    pRenderTarget->CreateSolidColorBrush(HGetRGBColor(hexColor), &brush);

    if (!isDrawingLine) {
        TAddLayer(false);
    }

    isDrawingLine = true;

    currentColor = hexColor;

    layers[TLayersCount() - 1].pBitmapRenderTarget->BeginDraw();

    // Scale coordinates and size
    float scaledXInitial = static_cast<float>(xInitial) / zoomFactor;
    float scaledYInitial = static_cast<float>(yInitial) / zoomFactor;
    float scaledX = static_cast<float>(x) / zoomFactor;
    float scaledY = static_cast<float>(y) / zoomFactor;
    float scaledBrushSize = static_cast<float>(currentBrushSize) / zoomFactor;

    if (startPoint.x != 0 || startPoint.y != 0 || endPoint.x != 0 || endPoint.y != 0) {
        D2D1_RECT_F lineBounds = D2D1::RectF(
            min(startPoint.x, endPoint.x) - scaledBrushSize,
            min(startPoint.y, endPoint.y) - scaledBrushSize,
            max(startPoint.x, endPoint.x) + scaledBrushSize,
            max(startPoint.y, endPoint.y) + scaledBrushSize
        );

        layers[TLayersCount() - 1].pBitmapRenderTarget->PushAxisAlignedClip(lineBounds, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        layers[TLayersCount() - 1].pBitmapRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
        layers[TLayersCount() - 1].pBitmapRenderTarget->PopAxisAlignedClip();
    }

    startPoint = D2D1::Point2F(scaledXInitial, scaledYInitial);
    endPoint = D2D1::Point2F(scaledX, scaledY);

    layers[TLayersCount() - 1].pBitmapRenderTarget->DrawLine(startPoint, endPoint, brush.Get(), scaledBrushSize, nullptr);

    layers[TLayersCount() - 1].pBitmapRenderTarget->EndDraw();
}

void TPaintBucketTool(int mouseX, int mouseY, COLORREF fillColor, HWND hWnd) {
    // 1) Captura o tamanho real (ampliado) da janela de desenho
    RECT rc;
    GetClientRect(docHWND, &rc);
    int capW = rc.right - rc.left;   // width * zoomFactor
    int capH = rc.bottom - rc.top;   // height * zoomFactor

    // 2) Captura pixels da tela ampliada
    std::vector<COLORREF> pixels = CaptureWindowPixels(docHWND, capW, capH);
    if (pixels.size() < static_cast<size_t>(capW * capH)) {
        HCreateLogData("error.log", "PaintBucketTool: falha ao capturar pixels ampliados");
        return;
    }

    // 3) Flood-fill em screen coords
    int startX = mouseX;
    int startY = mouseY;
    int startIdx = startY * capW + startX;
    COLORREF targetColor = pixels[startIdx];
    if (targetColor == fillColor) return;

    std::vector<POINT> rawPixelsToFill;
    rawPixelsToFill.reserve(10000);
    std::queue<POINT> q;
    q.push({ startX, startY });
    std::vector<bool> visited(capW * capH);
    visited[startIdx] = true;

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    while (!q.empty()) {
        POINT p = q.front(); q.pop();
        rawPixelsToFill.push_back(p);
        for (int i = 0; i < 4; ++i) {
            int nx = p.x + dx[i];
            int ny = p.y + dy[i];
            if (nx >= 0 && nx < capW && ny >= 0 && ny < capH) {
                int ni = ny * capW + nx;
                if (!visited[ni] && pixels[ni] == targetColor) {
                    visited[ni] = true;
                    q.push({ nx, ny });
                }
            }
        }
    }

    // 4) Converte para canvas “sem zoom”
    std::vector<POINT> pixelsToFill;
    pixelsToFill.reserve(rawPixelsToFill.size());
    for (auto& p : rawPixelsToFill) {
        pixelsToFill.push_back({
            static_cast<int>(std::floor(p.x / zoomFactor)),
            static_cast<int>(std::floor(p.y / zoomFactor))
            });
    }

    // 5) Desenha em múltiplas threads no BitmapRenderTarget
    auto target = layers[layerIndex].pBitmapRenderTarget.Get();
    ComPtr<ID2D1SolidColorBrush> brush;
    pRenderTarget->CreateSolidColorBrush(HGetRGBColor(fillColor), &brush);

    const int threadCount = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    std::mutex drawMutex;

    target->BeginDraw();
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t j = t; j < pixelsToFill.size(); j += threadCount) {
                POINT ip = pixelsToFill[j];
                D2D1_RECT_F r = D2D1::RectF(
                    float(ip.x), float(ip.y),
                    float(ip.x + 1), float(ip.y + 1)
                );
                // serialize chamadas D2D
                std::lock_guard<std::mutex> lk(drawMutex);
                target->FillRectangle(r, brush.Get());
            }
            });
    }
    for (auto& th : threads) th.join();
    target->EndDraw();

    // 6) Salva ACTION
    ACTION action;
    action.Tool = TPaintBucket;
    action.Layer = layerIndex;
    action.FillColor = fillColor;
    action.mouseX = static_cast<int>(std::floor(startX / zoomFactor));
    action.mouseY = static_cast<int>(std::floor(startY / zoomFactor));
    action.pixelsToFill = std::move(pixelsToFill);
    Actions.emplace_back(std::move(action));

    TRenderLayers();
    TDrawLayerPreview(layerIndex);
}

void __stdcall TSelectTool(int xInitial, int yInitial) {
    if (selectedAction) return;

    // Scale coordinates
    float scaledXInitial = static_cast<float>(xInitial) / zoomFactor;
    float scaledYInitial = static_cast<float>(yInitial) / zoomFactor;

    for (int i = Actions.size() - 1; i >= 0; --i) {

        const ACTION& Action = Actions[i];

        if (Action.Layer != layerIndex) continue;

        if (HitTestAction(Action, scaledXInitial, scaledYInitial)) {
            selectedIndex = i;
            selectedAction = true;
            break;
        }
    }

    if (selectedAction == false) return;
}

void __stdcall TMoveTool(int xInitial, int yInitial, int x, int y) {
    if (layerIndex < 0 || selectedIndex < 0 || selectedIndex >= Actions.size()) {
        return;
    }

    const ACTION& selected = Actions[selectedIndex];

    // Scale coordinates
    float scaledXInitial = static_cast<float>(xInitial) / zoomFactor;
    float scaledYInitial = static_cast<float>(yInitial) / zoomFactor;
    float scaledX = static_cast<float>(x) / zoomFactor;
    float scaledY = static_cast<float>(y) / zoomFactor;

    // Step 1: Calculate bounding box
    float minX = FLT_MAX;
    float minY = FLT_MAX;
    float maxX = -FLT_MAX;
    float maxY = -FLT_MAX;

    if (selected.Tool == TBrush) {
        for (const auto& action : Actions) {
            if (action.Layer != layerIndex || action.Tool != TBrush) continue;

            for (const auto& v : action.FreeForm.vertices) {
                minX = (std::min)(minX, v.x);
                minY = (std::min)(minY, v.y);
                maxX = (std::max)(maxX, v.x);
                maxY = (std::max)(maxY, v.y);
            }
        }
    }
    else {
        switch (selected.Tool) {
        case TRectangle:
            minX = selected.Position.left;
            minY = selected.Position.top;
            maxX = selected.Position.right;
            maxY = selected.Position.bottom;
            break;

        case TEllipse:
            minX = selected.Ellipse.point.x;
            minY = selected.Ellipse.point.y;
            maxX = selected.Ellipse.point.x;
            maxY = selected.Ellipse.point.y;
            break;

        case TLine:
            minX = (std::min)(selected.Line.startPoint.x, selected.Line.endPoint.x);
            minY = (std::min)(selected.Line.startPoint.y, selected.Line.endPoint.y);
            maxX = (std::max)(selected.Line.startPoint.x, selected.Line.endPoint.x);
            maxY = (std::max)(selected.Line.startPoint.y, selected.Line.endPoint.y);
            break;

        default:
            break;
        }
    }

    if (minX == FLT_MAX || minY == FLT_MAX) {
        return;
    }

    // Step 2: Calculate movement delta
    float centerX = (minX + maxX) / 2.0f;
    float centerY = (minY + maxY) / 2.0f;
    float deltaX = scaledX - centerX;
    float deltaY = scaledY - centerY;

    // Step 3: Apply movement
    if (selected.Tool == TBrush) {
        for (auto& action : Actions) {
            if (action.Layer != layerIndex || action.Tool != TBrush) continue;

            for (auto& v : action.FreeForm.vertices) {
                v.x += deltaX;
                v.y += deltaY;
            }
        }
    }
    else {
        ACTION& action = Actions[selectedIndex];

        switch (action.Tool) {
        case TRectangle:
            action.Position.left += deltaX;
            action.Position.top += deltaY;
            action.Position.right += deltaX;
            action.Position.bottom += deltaY;
            break;

        case TEllipse:
            action.Ellipse.point.x += deltaX;
            action.Ellipse.point.y += deltaY;
            break;

        case TLine:
            action.Line.startPoint.x += deltaX;
            action.Line.startPoint.y += deltaY;
            action.Line.endPoint.x += deltaX;
            action.Line.endPoint.y += deltaY;
            break;

        default:
            break;
        }
    }

    TUpdateLayers(layerIndex);
}

void __stdcall TUnSelectTool() {
    selectedAction = false;
    TRenderLayers();
}