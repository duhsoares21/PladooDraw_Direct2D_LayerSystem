#include "pch.h"
#include "Base.h"
#include "Structs.h"
#include "Constants.h"
#include "Helpers.h"
#include "Tools.h"
#include "ToolsAux.h"
#include "Layers.h"

/* TOOLS */

void TEraserTool(int left, int top) {
    if (prevLeft == -1 && prevTop == -1) {
        prevLeft = static_cast<float>(left) / zoomFactor;
        prevTop = static_cast<float>(top) / zoomFactor;
    }

    pRenderTarget->SetTarget(layers[layerIndex].pBitmap.Get());
    pRenderTarget->BeginDraw();
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

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

            pRenderTarget->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            pRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
            pRenderTarget->PopAxisAlignedClip();

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

    pRenderTarget->PushAxisAlignedClip(rect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
    pRenderTarget->PopAxisAlignedClip();

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

    pRenderTarget->EndDraw();
}

void TBrushTool(int left, int top, COLORREF hexColor, bool pixelMode, int pPixelSizeRatio) {
    if (pBrush == nullptr) {
        pRenderTarget->CreateSolidColorBrush(HGetRGBColor(hexColor), &pBrush);
    }
    else {
        pBrush->SetColor(HGetRGBColor(hexColor));
    }

    if (pixelSizeRatio == -1) {
        pixelSizeRatio = pPixelSizeRatio;
    }

    currentColor = hexColor;
    isDrawingBrush = true;
    isPixelMode = pixelMode;

    pRenderTarget->SetTarget(layers[layerIndex].pBitmap.Get());
    pRenderTarget->BeginDraw();

    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    // Scale coordinates and sizes
    float scaledLeft = static_cast<float>(left) / zoomFactor;
    float scaledTop = static_cast<float>(top) / zoomFactor;
    float scaledBrushSize = static_cast<float>(currentBrushSize) / zoomFactor;

    if (pixelMode) {
        RECT rect;

        GetWindowRect(docHWND, &rect);

        int DocumentWidth = rect.right - rect.left;
        int DocumentHeight = rect.bottom - rect.top;

        int CanvasWidth = DocumentWidth / 16;
        int CanvasHeight = DocumentHeight / 16;

        pixelSizeRatio = DocumentWidth / CanvasWidth;

        // PIXEL MODE: 1x1 rectangles only
        int snappedLeft = static_cast<int>(scaledLeft / pixelSizeRatio) * pixelSizeRatio;
        int snappedTop = static_cast<int>(scaledTop / pixelSizeRatio) * pixelSizeRatio;

        D2D1_RECT_F pixel = D2D1::RectF(
            static_cast<float>(snappedLeft),
            static_cast<float>(snappedTop),
            static_cast<float>(snappedLeft + pixelSizeRatio),
            static_cast<float>(snappedTop + pixelSizeRatio)
        );

        pRenderTarget->FillRectangle(pixel, pBrush.Get());

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

                pRenderTarget->DrawRectangle(rect, pBrush.Get());
                pRenderTarget->FillRectangle(rect, pBrush.Get());

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

        pRenderTarget->DrawRectangle(rect, pBrush.Get());
        pRenderTarget->FillRectangle(rect, pBrush.Get());

        if (scaledLeft != -1 && scaledTop != -1) {
            Vertices.emplace_back(VERTICE{ static_cast<float>(scaledLeft), static_cast<float>(scaledTop), static_cast<int>(currentBrushSize) });
        }
    }

    pRenderTarget->EndDraw();

    prevLeft = scaledLeft;
    prevTop = scaledTop;
}

void TRectangleTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    if (pBrush == nullptr) {
        pRenderTarget->CreateSolidColorBrush(HGetRGBColor(hexColor), &pBrush);
    }
    else {
        pBrush->SetColor(HGetRGBColor(hexColor));
    }

    if (!isDrawingRectangle) {
        TAddLayer(false);
    }

    isDrawingRectangle = true;

    currentColor = hexColor;

    pRenderTarget->SetTarget(layers[layerIndex].pBitmap.Get());
    pRenderTarget->BeginDraw();
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    // Scale coordinates
    float scaledLeft = static_cast<float>(left) / zoomFactor;
    float scaledTop = static_cast<float>(top) / zoomFactor;
    float scaledRight = static_cast<float>(right) / zoomFactor;
    float scaledBottom = static_cast<float>(bottom) / zoomFactor;

    rectangle = D2D1::RectF(scaledLeft, scaledTop, scaledRight, scaledBottom);

    if (prevRectangle.left != 0 || prevRectangle.top != 0 || prevRectangle.right != 0 || prevRectangle.bottom != 0) {
        pRenderTarget->PushAxisAlignedClip(prevRectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
        pRenderTarget->PopAxisAlignedClip();
    }

    pRenderTarget->PushAxisAlignedClip(rectangle, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    pRenderTarget->FillRectangle(rectangle, pBrush.Get());
    pRenderTarget->PopAxisAlignedClip();

    prevRectangle = D2D1::RectF(scaledLeft, scaledTop, scaledRight, scaledBottom);

    pRenderTarget->EndDraw();
}

void TEllipseTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    if (pBrush == nullptr) {
        pRenderTarget->CreateSolidColorBrush(HGetRGBColor(hexColor), &pBrush);
    }
    else {
        pBrush->SetColor(HGetRGBColor(hexColor));
    }

    if (!isDrawingEllipse) {
        TAddLayer(false);
    }

    isDrawingEllipse = true;

    currentColor = hexColor;

    pRenderTarget->SetTarget(layers[layerIndex].pBitmap.Get());
    pRenderTarget->BeginDraw();
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

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

        pRenderTarget->PushAxisAlignedClip(prevRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
        pRenderTarget->PopAxisAlignedClip();
    }

    pRenderTarget->FillEllipse(ellipse, pBrush.Get());

    prevEllipse = ellipse;

    pRenderTarget->EndDraw();
}

void TLineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor) {
    
    if (pBrush == nullptr) {
        pRenderTarget->CreateSolidColorBrush(HGetRGBColor(hexColor), &pBrush);
    }
    else {
        pBrush->SetColor(HGetRGBColor(hexColor));
    }

    if (!isDrawingLine) {
        TAddLayer(false);
    }

    isDrawingLine = true;

    currentColor = hexColor;

    pRenderTarget->SetTarget(layers[layerIndex].pBitmap.Get());
    pRenderTarget->BeginDraw();
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

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

        pRenderTarget->PushAxisAlignedClip(lineBounds, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
        pRenderTarget->PopAxisAlignedClip();
    }

    startPoint = D2D1::Point2F(scaledXInitial, scaledYInitial);
    endPoint = D2D1::Point2F(scaledX, scaledY);

    pRenderTarget->DrawLine(startPoint, endPoint, pBrush.Get(), scaledBrushSize, nullptr);

    pRenderTarget->EndDraw();
}

void TPaintBucketTool(int mouseX, int mouseY, COLORREF fillColor, HWND hWnd) {
    RECT rc;
    GetClientRect(docHWND, &rc);
    
    int capW = static_cast<int>(logicalWidth);
    int capH = static_cast<int>(logicalHeight);

    std::vector<COLORREF> pixels = CaptureCanvasPixels();
    if (pixels.size() < static_cast<size_t>(capW * capH)) {
        HCreateLogData("error.log", "PaintBucketTool: falha ao capturar pixels ampliados");
        return;
    }

    int startX = static_cast<int>(mouseX / zoomFactor);
    int startY = static_cast<int>(mouseY / zoomFactor);

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

    std::vector<POINT> pixelsToFill = rawPixelsToFill;

    if (pBrush == nullptr) {
        pRenderTarget->CreateSolidColorBrush(HGetRGBColor(fillColor), &pBrush);
    }
    else {
        pBrush->SetColor(HGetRGBColor(fillColor));
    }

    const int threadCount = std::thread::hardware_concurrency();
    std::vector<std::thread> threads;
    std::mutex drawMutex;


    pRenderTarget->SetTarget(layers[layerIndex].pBitmap.Get());
    pRenderTarget->BeginDraw();
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t j = t; j < pixelsToFill.size(); j += threadCount) {
                POINT ip = pixelsToFill[j];
                D2D1_RECT_F r = D2D1::RectF(
                    float(ip.x), float(ip.y),
                    float(ip.x + 1), float(ip.y + 1)
                );
                
                pRenderTarget->FillRectangle(r, pBrush.Get());
            }
        });
    }
    for (auto& th : threads) th.join();
    pRenderTarget->EndDraw();

    // 6) Salva ACTION
    ACTION action;
    action.Tool = TPaintBucket;
    action.Layer = layerIndex;
    action.FillColor = fillColor;
    action.mouseX = startX;
    action.mouseY = startY;
    action.pixelsToFill = std::move(pixelsToFill);
    Actions.emplace_back(std::move(action));

    TRenderLayers();
}

void TWriteTool(int xInitial, int yInitial, int x, int y) {
    if (pBrush == nullptr) {
        pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &pBrush);
    }
    else {
        pBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black));
    }

    pRenderTarget->SetTarget(layers[layerIndex].pBitmap.Get());
    pRenderTarget->BeginDraw();
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    isWritingText = true;

    // Scale coordinates
    float scaledLeft = static_cast<float>(xInitial) / zoomFactor;
    float scaledTop = static_cast<float>(yInitial) / zoomFactor;
    float scaledRight = static_cast<float>(x) / zoomFactor;
    float scaledBottom = static_cast<float>(y) / zoomFactor;

    textArea = D2D1::RectF(scaledLeft, scaledTop, scaledRight, scaledBottom);

    if (prevTextArea.left != 0 || prevTextArea.top != 0 || prevTextArea.right != 0 || prevTextArea.bottom != 0) {
        pRenderTarget->PushAxisAlignedClip(prevTextArea, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
        pRenderTarget->PopAxisAlignedClip();
    }

    D2D1_STROKE_STYLE_PROPERTIES strokeProps = {};
    strokeProps.dashStyle = D2D1_DASH_STYLE_DASH;       // dashed line
    strokeProps.lineJoin = D2D1_LINE_JOIN_ROUND;       // line corners
    strokeProps.startCap = D2D1_CAP_STYLE_ROUND;       // start of line
    strokeProps.endCap = D2D1_CAP_STYLE_ROUND;       // end of line
    strokeProps.dashOffset = 0.0f;
    
    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> pStrokeStyle;
    pD2DFactory->CreateStrokeStyle(
        strokeProps,
        nullptr,        // nullptr = default dash array
        0,              // number of elements in dash array
        &pStrokeStyle
    );

    pRenderTarget->PushAxisAlignedClip(textArea, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    pRenderTarget->DrawRectangle(textArea, pBrush.Get(), 1.0f, pStrokeStyle.Get());
    pRenderTarget->PopAxisAlignedClip();

    prevTextArea = D2D1::RectF(scaledLeft, scaledTop, scaledRight, scaledBottom);

    pRenderTarget->EndDraw();

    /*
     if (isWritingText) {
            ComPtr<ID2D1SolidColorBrush> textBrush;
            pRenderTarget->CreateSolidColorBrush(HGetRGBColor(fontColor), &textBrush);

            Microsoft::WRL::ComPtr<IDWriteTextFormat> pTextFormat;

            FLOAT fontSizeDIP = fontSize > 0 ? ((fontSize / 10.0f) * (96.0f / 72.0f)) : 0;

            HRESULT hr = pDWriteFactory->CreateTextFormat(
                fontFace.length() > 0 ? fontFace.c_str() : L"Arial",                // Font family
                nullptr,                    // Font collection (nullptr = system)
                (fontWeight >= FW_BOLD) ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_REGULAR,
                fontItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                fontSize > 0 ? fontSizeDIP : 24.0f,                // Font size in DIPs
                L"en-us",                   // Locale
                &pTextFormat
            );

            if (FAILED(hr)) {
                std::wcout << hr;
            }

            const WCHAR* text = L"Hello World";
            size_t textLength = wcslen(text);

            pRenderTarget->DrawTextW(
                text,               // Text
                static_cast<UINT32>(textLength),
                pTextFormat.Get(),  // Text format
                textArea,           // Layout rectangle
                textBrush.Get()         // Brush
            );

            action.Tool = TWrite;
            action.Layer = layerIndex;
            action.Position = textArea;
            action.Text = text;
            action.Color = currentColor;

            textArea = D2D1::RectF(0, 0, 0, 0);
            isWritingText = false;
        }
    */
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

