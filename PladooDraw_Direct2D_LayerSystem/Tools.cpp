#include "pch.h"
#include "Base.h"
#include "Structs.h"
#include "Constants.h"
#include "Helpers.h"
#include "Tools.h"
#include "ToolsAux.h"
#include "Layers.h"
#include "Main.h"

/* TOOLS */

void TEraserTool(int left, int top) {

    auto it = std::find_if(
        layers.begin(),
        layers.end(),
        [](const std::optional<Layer>& optLayer) {
            return optLayer.has_value() && optLayer->LayerID == layerIndex && optLayer->FrameIndex == CurrentFrameIndex;
        }
    );

    if (!it->has_value()) return;

    if (prevLeft == -1 && prevTop == -1) {
        prevLeft = static_cast<float>(left) / zoomFactor;
        prevTop = static_cast<float>(top) / zoomFactor;
    }

    pRenderTarget->SetTarget(it->value().pBitmap.Get());
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
            action.FrameIndex = CurrentFrameIndex;
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

    auto it = std::find_if(
        layers.begin(),
        layers.end(),
        [](const std::optional<Layer>& optLayer) {
            return optLayer.has_value() && optLayer->LayerID == layerIndex && optLayer->FrameIndex == CurrentFrameIndex;
        }
    );

    if (!it->has_value()) return;

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

    pRenderTarget->SetTarget(it->value().pBitmap.Get());
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
    if (!layers[layerIndex].has_value()) return;

    if (pBrush == nullptr) {
        pRenderTarget->CreateSolidColorBrush(HGetRGBColor(hexColor), &pBrush);
    }
    else {
        pBrush->SetColor(HGetRGBColor(hexColor));
    }

    if (!isDrawingRectangle) {
        TAddLayer(false, -1, CurrentFrameIndex);
    }

    isDrawingRectangle = true;

    currentColor = hexColor;

    pRenderTarget->SetTarget(layers[TLayersCount() - 1].value().pBitmap.Get());
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
    if (!layers[layerIndex].has_value()) return;

    if (pBrush == nullptr) {
        pRenderTarget->CreateSolidColorBrush(HGetRGBColor(hexColor), &pBrush);
    }
    else {
        pBrush->SetColor(HGetRGBColor(hexColor));
    }

    if (!isDrawingEllipse) {
        TAddLayer(false, -1, -1);
    }

    isDrawingEllipse = true;

    currentColor = hexColor;

    pRenderTarget->SetTarget(layers[TLayersCount() - 1].value().pBitmap.Get());
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
    if (!layers[layerIndex].has_value()) return;

    if (pBrush == nullptr) {
        pRenderTarget->CreateSolidColorBrush(HGetRGBColor(hexColor), &pBrush);
    }
    else {
        pBrush->SetColor(HGetRGBColor(hexColor));
    }

    if (!isDrawingLine) {
        TAddLayer(false, -1, -1);
    }

    isDrawingLine = true;

    currentColor = hexColor;

    pRenderTarget->SetTarget(layers[TLayersCount() - 1].value().pBitmap.Get());
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
    auto it = std::find_if(
        layers.begin(),
        layers.end(),
        [](const std::optional<Layer>& optLayer) {
            return optLayer.has_value() && optLayer->LayerID == layerIndex && optLayer->FrameIndex == CurrentFrameIndex;
        }
    );

    if (!it->has_value()) return;

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

    if (startX < 0 || startX >= capW || startY < 0 || startY >= capH) return;

    COLORREF targetColor = pixels[startIdx];
    if (targetColor == fillColor) return;

    // --- OPTIMIZED BFS ---
    std::vector<POINT> pixelsToFill;
    pixelsToFill.reserve(10000);

    std::vector<POINT> q;  // flat vector replaces queue
    q.reserve(capW * capH);
    q.push_back({ startX, startY });

    std::vector<uint8_t> visited(capW * capH, 0); // faster visited map
    visited[startIdx] = 1;

    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };

    size_t idx = 0;
    while (idx < q.size()) {
        POINT p = q[idx++];
        pixelsToFill.push_back(p);

        for (int i = 0; i < 4; ++i) {
            int nx = p.x + dx[i];
            int ny = p.y + dy[i];
            if (nx >= 0 && nx < capW && ny >= 0 && ny < capH) {
                int ni = ny * capW + nx;
                if (!visited[ni] && pixels[ni] == targetColor) {
                    visited[ni] = 1;
                    q.push_back({ nx, ny });
                }
            }
        }
    }
    // --- END OPTIMIZED BFS ---

    if (pBrush == nullptr) {
        pRenderTarget->CreateSolidColorBrush(HGetRGBColor(fillColor), &pBrush);
    }
    else {
        pBrush->SetColor(HGetRGBColor(fillColor));
    }

    std::vector<D2D1_RECT_F> rects(pixelsToFill.size());
    
    for (size_t j = 0; j < pixelsToFill.size(); j++) {
        auto [px, py] = pixelsToFill[j];
        rects[j] = D2D1::RectF(
            (FLOAT)px, (FLOAT)py,
            (FLOAT)px + 1, (FLOAT)py + 1
        );
    }
    
    pRenderTarget->SetTarget(it->value().pBitmap.Get());
    pRenderTarget->BeginDraw();
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    for (auto& r : rects) {
        pRenderTarget->FillRectangle(r, pBrush.Get());
    }
    pRenderTarget->EndDraw();

    // 6) Salva ACTION
    ACTION action;
    action.Tool = TPaintBucket;
    action.Layer = layerIndex;
    action.FrameIndex = CurrentFrameIndex;
    action.FillColor = fillColor;
    action.mouseX = startX;
    action.mouseY = startY;
    action.pixelsToFill = std::move(pixelsToFill);
    Actions.emplace_back(std::move(action));

    TRenderLayers();
}

void TInitTextTool(float scaledLeft, float scaledTop, float width, float height) {
    if (isWritingText) return;
    HINSTANCE hInstance = GetModuleHandle(NULL);

    hTextInput = CreateWindowEx(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        scaledLeft, scaledTop, width, height,        // temporary size, will update per textArea
        docHWND, nullptr, hInstance , nullptr
    );

    oldEditProc = (WNDPROC)SetWindowLongPtr(
        hTextInput,
        GWLP_WNDPROC,
        (LONG_PTR)TextEditProc
    );

    SetFocus(hTextInput);
    
}

void TWriteTool(int x, int y) {
    if (fontSize == 0) {
        SetFont();
        return;
    }

    float scaledX = static_cast<float>(x) / zoomFactor;
    float scaledY = static_cast<float>(y) / zoomFactor;
    float scaledW = 200 / zoomFactor;
    float scaledH = 30 / zoomFactor;

    textArea = D2D1::RectF(scaledX, scaledY, scaledX + scaledW, scaledY + scaledH);

    HINSTANCE hInstance = GetModuleHandle(NULL);

    hTextInput = CreateWindowEx(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        scaledX, scaledY, scaledW, scaledH,
        docHWND, nullptr, hInstance, nullptr
    );

    SetFocus(hTextInput);

    oldEditProc = (WNDPROC)SetWindowLongPtr(
        hTextInput,
        GWLP_WNDPROC,
        (LONG_PTR)TextEditProc
    );
}

void TWriteToolCommitText() {
    auto it = std::find_if(
        layers.begin(),
        layers.end(),
        [](const std::optional<Layer>& optLayer) {
            return optLayer.has_value() && optLayer->LayerID == layerIndex && optLayer->FrameIndex == CurrentFrameIndex;
        }
    );

    if (!it->has_value()) return;

    wchar_t buffer[1024] = {};
    GetWindowText(hTextInput, buffer, 1024);
    std::wstring text(buffer);

    // Draw the text onto the layer bitmap
    pRenderTarget->SetTarget(it->value().pBitmap.Get());
    pRenderTarget->BeginDraw();
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    if (pBrush) {
        pBrush->SetColor(HGetRGBColor(fontColor));
    }
    else {
        pRenderTarget->CreateSolidColorBrush(HGetRGBColor(fontColor), &pBrush);
    }

    pDWriteFactory->CreateTextFormat(
        fontFace.c_str(),                                     // Font family
        nullptr,                                           // Font collection (nullptr = system)
        static_cast<DWRITE_FONT_WEIGHT>(
            (fontWeight > DWRITE_FONT_WEIGHT_BLACK) ? DWRITE_FONT_WEIGHT_BLACK : fontWeight
            ),
        fontItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        static_cast<FLOAT>(fontSize) / 10.0f,       // Convert tenths of a point to DIPs
        L"en-us",                                         // Locale
        &pTextFormat
    );

    Microsoft::WRL::ComPtr<IDWriteTextLayout> textLayout;
    pDWriteFactory->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        pTextFormat.Get(),
        textArea.right - textArea.left,
        textArea.bottom - textArea.top,
        &textLayout
    );

    if (fontUnderline)
        textLayout->SetUnderline(TRUE, DWRITE_TEXT_RANGE{ 0, (UINT32)text.size() });
    if (fontStrike)
        textLayout->SetStrikethrough(TRUE, DWRITE_TEXT_RANGE{ 0, (UINT32)text.size() });

    pRenderTarget->DrawTextLayout(
        D2D1::Point2F(textArea.left, textArea.top),
        textLayout.Get(),
        pBrush.Get()
    );

    pRenderTarget->EndDraw();
    
    isWritingText = false;

    DestroyWindow(hTextInput);

    TRenderLayers();

    InvalidateRect(docHWND, nullptr, TRUE);

    SetFocus(docHWND);

    ACTION action;

    action.Tool = TWrite;
    action.Text = text;
    action.Layer = layerIndex;
    action.FrameIndex = CurrentFrameIndex;
    action.Position = textArea;
    action.Color = fontColor;

    action.FontFamily = fontFace;
    action.FontSize = fontSize;
    action.FontWeight = fontWeight;
    action.FontItalic = (fontItalic != 0);
    action.FontUnderline = (fontUnderline != 0);
    action.FontStrike = (fontStrike != 0);

    Actions.push_back(action);
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
        case TWrite:
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
        case TWrite:
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

    TUpdateLayers(layerIndex, CurrentFrameIndex);
}

void __stdcall TUnSelectTool() {
    selectedAction = false;
    TRenderLayers();
}

/* SUBCLASS */

LRESULT CALLBACK TextEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) {
            TWriteToolCommitText(); // bake text into bitmap
            return 0; // swallow Enter
        }
        break;

    case WM_KILLFOCUS:
        //TWriteToolCommitText(); // user clicked away
        break;
    }

    return CallWindowProc(oldEditProc, hwnd, msg, wParam, lParam);
}
