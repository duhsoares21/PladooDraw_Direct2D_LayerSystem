#include "pch.h"
#include "Base.h"
#include "Constants.h"
#include "Helpers.h"
#include "Tools.h"
#include "Layers.h"

/* LAYERS */

int TLayersCount() {
    return layers.size();
}

HRESULT TAddLayer(bool fromFile = false) {

    ComPtr<ID2D1BitmapRenderTarget> pBitmapRenderTarget;

    D2D1_SIZE_F size = pRenderTarget->GetSize();

    HRESULT renderTargetCreated = pRenderTarget->CreateCompatibleRenderTarget(
        D2D1::SizeF(size.width, size.height), &pBitmapRenderTarget);

    if (FAILED(renderTargetCreated)) {
        MessageBox(NULL, L"Erro ao criar BitmapRenderTarget", L"Erro", MB_OK);
        return renderTargetCreated;
    }

    ComPtr<ID2D1Bitmap> pBitmap;
    pBitmapRenderTarget->GetBitmap(&pBitmap);

    pBitmapRenderTarget->BeginDraw();
    pBitmapRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
    pBitmapRenderTarget->EndDraw();

    Layer layer = Layer{ pBitmapRenderTarget, pBitmap };
    layers.emplace_back(layer);

    if (fromFile == false) {
        LayerOrder layerOrder = { static_cast<int>(layers.size()) - 1, static_cast<int>(layers.size()) - 1 };
        layersOrder.emplace_back(layerOrder);
    }

    return S_OK;
}

void TAddLayerButton(HWND layerButton) {

    RECT rc;
    GetClientRect(layerButton, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HDC hdc = GetDC(layerButton);

    HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, size.width, size.height);

    SelectObject(hdc, hBrush);
    FillRect(hdc, &rc, hBrush);

    SendMessage(layerButton, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap);

    LayerButtons.push_back({ layerButton , hBitmap });

    DeleteObject(hBrush);
    ReleaseDC(layerButton, hdc);
}

HRESULT TRemoveLayer() {

    layers.erase(layers.begin() + layerIndex);

    for (auto it = layersOrder.begin(); it != layersOrder.end(); ++it) {
        if (it->layerIndex == layerIndex) {
            layersOrder.erase(it);
            break;
        }
    }

    for (auto& lo : layersOrder) {
        if (lo.layerIndex > layerIndex) {
            lo.layerIndex--; // shift left
        }

        lo.layerOrder--;
    }

    Actions.erase(
        std::remove_if(Actions.begin(), Actions.end(), [](const ACTION& a) {
            return a.Layer == layerIndex;
            }),
        Actions.end()
    );

    for (auto& a : Actions) {
        if (a.Layer > layerIndex)
            a.Layer--; // shift left
    }

    if (layers.size() == 0) TAddLayer();

    return S_OK;
}

HRESULT __stdcall TRecreateLayers(HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int& layerID, const wchar_t* szButtonClass, const wchar_t* msgText) {
    for (auto layerButton : LayerButtons) {
        DestroyWindow(layerButton.button);
    }

    LayerButtons.clear();

    int maxLayer = layers.size();

    for (int i = 0; i < maxLayer; ++i) {
        int yPos = btnHeight * i;
        HWND button = CreateWindowEx(
            0,                           // dwExStyle
            L"Button",                   // lpClassName
            msgText,                     // lpWindowName
            WS_CHILD | WS_VISIBLE | BS_BITMAP, // dwStyle
            0,                           // x
            yPos,                        // y
            btnWidth,                          // nWidth
            btnHeight,                   // nHeight
            hWndLayer,                   // hWndParent
            (HMENU)(intptr_t)layerID,    // hMenu (control ID)
            hLayerInstance,              // hInstance
            NULL                         // lpParam
        );

        if (button == NULL) {
            DWORD dwError = GetLastError();
            std::string errorMsg = "Failed to create button for layer " + std::to_string(i) + ": Error " + std::to_string(dwError);
            HCreateLogData("error.log", errorMsg);
            return E_FAIL;
        }

        ShowWindow(button, SW_SHOWDEFAULT);
        UpdateWindow(button);

        hLayerButtons[layerID] = button;

        // Add button to LayerButtons
        TAddLayerButton(button);

        if (i == maxLayer - 1) {
            layerID = i; // Atualiza layerID para o último índice usado
        }
    }

    return S_OK;
}

int TGetLayer() {
    return layerIndex;
}

void TSetLayer(int index) {
    layerIndex = index;
    return;
}

void TReorderLayerUp() {
    if (layersOrder[layerIndex].layerOrder > 0) {
        int previousOrder = layersOrder[layerIndex].layerOrder;
        int targetOrder = previousOrder - 1;

        int index = std::distance(layersOrder.begin(),
            std::find_if(layersOrder.begin(), layersOrder.end(),
                [targetOrder](const LayerOrder& l) { return l.layerOrder == targetOrder; })
        );

        layersOrder[layerIndex].layerOrder = layersOrder[index].layerOrder;
        layersOrder[index].layerOrder = previousOrder;
    }
    return;
}

void TReorderLayerDown() {
    if (layersOrder[layerIndex].layerOrder < layers.size() - 1) {
        int previousOrder = layersOrder[layerIndex].layerOrder;
        int targetOrder = previousOrder + 1;

        int index = std::distance(layersOrder.begin(),
            std::find_if(layersOrder.begin(), layersOrder.end(),
                [targetOrder](const LayerOrder& l) { return l.layerOrder == targetOrder; })
        );

        layersOrder[layerIndex].layerOrder = layersOrder[index].layerOrder;
        layersOrder[index].layerOrder = previousOrder;
    }
    return;
}

void TRenderActionToTarget(const ACTION& action, ID2D1RenderTarget* target) {
    if (!target) return;

    switch (action.Tool) {
    case TEraser:
        target->PushAxisAlignedClip(action.Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        target->Clear(D2D1::ColorF(0, 0, 0, 0));
        target->PopAxisAlignedClip();
        break;

    case TRectangle: {
        ComPtr<ID2D1SolidColorBrush> brush;
        target->CreateSolidColorBrush(HGetRGBColor(action.Color), &brush);

        target->PushAxisAlignedClip(action.Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        target->FillRectangle(action.Position, brush.Get());
        target->PopAxisAlignedClip();
        break;
    }

    case TEllipse: {
        ComPtr<ID2D1SolidColorBrush> brush;
        target->CreateSolidColorBrush(HGetRGBColor(action.Color), &brush);
        target->FillEllipse(action.Ellipse, brush.Get());
        break;
    }

    case TLine: {
        ComPtr<ID2D1SolidColorBrush> brush;
        target->CreateSolidColorBrush(HGetRGBColor(action.Color), &brush);
        target->DrawLine(action.Line.startPoint, action.Line.endPoint, brush.Get(), action.BrushSize, nullptr);
        break;
    }

    case TBrush: {
        ComPtr<ID2D1SolidColorBrush> brush;
        for (const auto& vertex : action.FreeForm.vertices) {

            D2D1_COLOR_F color = HGetRGBColor(action.Color);
            target->CreateSolidColorBrush(color, &brush);

            if (action.isPixelMode) {
                int snappedLeft = static_cast<int>(vertex.x / pixelSizeRatio) * pixelSizeRatio;
                int snappedTop = static_cast<int>(vertex.y / pixelSizeRatio) * pixelSizeRatio;

                D2D1_RECT_F pixel = D2D1::RectF(
                    static_cast<float>(snappedLeft),
                    static_cast<float>(snappedTop),
                    static_cast<float>(snappedLeft + pixelSizeRatio),
                    static_cast<float>(snappedTop + pixelSizeRatio)
                );

                target->FillRectangle(pixel, brush.Get());
            } else {

                float scaledLeft = static_cast<float>(vertex.x) / zoomFactor;
                float scaledTop = static_cast<float>(vertex.y) / zoomFactor;
                float scaledBrushSize = static_cast<float>(currentBrushSize) / zoomFactor;
                float scaledPixelSizeRatio = static_cast<float>(pixelSizeRatio) / zoomFactor;

                D2D1_RECT_F rect = D2D1::RectF(
                    scaledLeft - scaledBrushSize * 0.5f,
                    scaledTop - scaledBrushSize * 0.5f,
                    scaledLeft + scaledBrushSize * 0.5f,
                    scaledTop + scaledBrushSize * 0.5f
                );

                layers[layerIndex].pBitmapRenderTarget->DrawRectangle(rect, brush.Get());
                layers[layerIndex].pBitmapRenderTarget->FillRectangle(rect, brush.Get());
            }
        }
        break;
    }

    case TPaintBucket: {
        ComPtr<ID2D1SolidColorBrush> brush;
        target->CreateSolidColorBrush(HGetRGBColor(action.FillColor), &brush);
        for (const auto& p : action.pixelsToFill) {
            D2D1_RECT_F rect = D2D1::RectF((float)p.x, (float)p.y, (float)(p.x + 1), (float)(p.y + 1));
            target->FillRectangle(&rect, brush.Get());
        }
        break;
    }

    default:
        break;
    }
}

void TUpdateLayers(int layerIndexTarget = -1) {

    if (layerIndex < 0 || layerIndex >= layers.size()) {
        layerIndex = 0;
    }

    if (layerIndexTarget == -1) {
        layerIndexTarget = layerIndex;
    }

    auto& layer = layers[layerIndexTarget];

    if (!layer.pBitmapRenderTarget) return;

    layer.pBitmapRenderTarget->BeginDraw();
    layer.pBitmapRenderTarget->Clear(D2D1::ColorF(255, 255, 255, 0)); // Transparente

    for (const auto& action : Actions) {
        if (action.Layer == layerIndexTarget) {
            TRenderActionToTarget(action, layer.pBitmapRenderTarget.Get());
        }
    }

    layer.pBitmapRenderTarget->EndDraw();
}

void TRenderLayers() {
    if (!pRenderTarget) {
        return;
    }

    pRenderTarget->BeginDraw();

    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

    std::vector<LayerOrder> sortedLayers = layersOrder;
    std::sort(sortedLayers.begin(), sortedLayers.end(), [](const LayerOrder& a, const LayerOrder& b) {
        return a.layerOrder < b.layerOrder;
        });

    for (size_t i = 0; i < sortedLayers.size(); ++i) {
        const auto& layer = layers[sortedLayers[i].layerIndex];

        if (layer.pBitmap) {
            pRenderTarget->DrawBitmap(layer.pBitmap.Get());
        }
    }

    HRESULT hr = pRenderTarget->EndDraw();

    if (FAILED(hr)) {
        MessageBox(NULL, L"Failed to render layers", L"Error", MB_OK);
    }
}

void TDrawLayerPreview(int currentLayer) {

    HBITMAP hBitmap = LayerButtons[layerIndex].hBitmap;

    RECT WindowRC;
    GetClientRect(docHWND, &WindowRC);

    RECT rc;
    GetClientRect(LayerButtons[layerIndex].button, &rc);

    HDC hdc = GetDC(LayerButtons[layerIndex].button);
    HDC compatibleDC = CreateCompatibleDC(hdc);

    SelectObject(compatibleDC, hBitmap);

    BITMAP bmp = {};
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    RECT bmpRect = { 0, 0, bmp.bmWidth, bmp.bmHeight };

    HBRUSH whiteBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);
    FillRect(compatibleDC, &bmpRect, whiteBrush);

    if (!LayerButtons[layerIndex].isInitialPainted) {
        HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
        SelectObject(compatibleDC, whiteBrush);

        COLORREF initialClickedColor = GetPixel(compatibleDC, 0, 0);

        ExtFloodFill(compatibleDC, 0, 0, initialClickedColor, FLOODFILLSURFACE);

        DeleteObject(whiteBrush);

        LayerButtons[layerIndex].isInitialPainted = true;
    }

    for (int i = 0; i < Actions.size(); i++) {
        if (Actions[i].Layer == layerIndex) {
            if (Actions[i].Tool == TBrush) {
                for (int j = 0; j < Actions[i].FreeForm.vertices.size(); j++) {
                    float scaledLeft = static_cast<float>(Actions[i].FreeForm.vertices[j].x);
                    float scaledTop = static_cast<float>(Actions[i].FreeForm.vertices[j].y);

                    float scaledBrushSize = static_cast<float>(Actions[i].BrushSize) / zoomFactor;
                    float scaledPixelSizeRatio = static_cast<float>(pixelSizeRatio) / zoomFactor;
                    
                    if (!Actions[i].isPixelMode) {
                        scaledBrushSize = static_cast<float>(Actions[i].FreeForm.vertices[j].BrushSize) / zoomFactor;

                        if (scaledBrushSize > 12.0f)
                            scaledBrushSize = 12.0f;

                        if (scaledBrushSize < 1.0f)
                            scaledBrushSize = 1.0f;
                    }

                    int snappedLeft = static_cast<int>(scaledLeft / scaledPixelSizeRatio) * scaledPixelSizeRatio;
                    int snappedTop = static_cast<int>(scaledTop / scaledPixelSizeRatio) * scaledPixelSizeRatio;

                    HBRUSH hBrush = CreateSolidBrush(Actions[i].Color);
                    HPEN hPen = CreatePen(PS_SOLID, scaledBrushSize, Actions[i].Color);
                   
                    SelectObject(compatibleDC, hBrush);
                    SelectObject(compatibleDC, hPen);

                    if (Actions[i].isPixelMode) {
                        RECT pXY = HScalePointsToButton(snappedLeft, snappedTop, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), true, pixelSizeRatio);
                        RECT rectPoint = { pXY.left, pXY.top, pXY.right, pXY.bottom };
                        Rectangle(compatibleDC, rectPoint.left, rectPoint.top, rectPoint.right, rectPoint.bottom);
                    }
                    else
                    {
                        RECT pXY = HScalePointsToButton(Actions[i].FreeForm.vertices[j].x, Actions[i].FreeForm.vertices[j].y, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);
                        RECT rectPoint = { pXY.left - 1, pXY.top - 1, pXY.left + 1, pXY.top + 1 };
                        Ellipse(compatibleDC, rectPoint.left, rectPoint.top, rectPoint.right, rectPoint.bottom);
                    }

                    DeleteObject(hBrush);
                    DeleteObject(hPen);
                }
            }
            else if (Actions[i].Tool == TEraser) {
                HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
                HPEN hPen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));

                SelectObject(compatibleDC, hBrush);
                SelectObject(compatibleDC, hPen);

                RECT pXY = HScalePointsToButton(Actions[i].Position.left, Actions[i].Position.top, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);
                RECT pWZ = HScalePointsToButton(Actions[i].Position.right, Actions[i].Position.bottom, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);

                RECT rectPoint = { pXY.left - 1, pXY.top - 1, pWZ.left + 1, pWZ.top + 1 };

                Ellipse(compatibleDC, rectPoint.left, rectPoint.top, rectPoint.right, rectPoint.bottom);

                DeleteObject(hBrush);
                DeleteObject(hPen);
            }
            else if (Actions[i].Tool == TRectangle) {
                HBRUSH hBrush = CreateSolidBrush(Actions[i].Color);
                HPEN hPen = CreatePen(PS_SOLID, 1, Actions[i].Color);

                SelectObject(compatibleDC, hBrush);
                SelectObject(compatibleDC, hPen);

                RECT pXY = HScalePointsToButton(Actions[i].Position.left, Actions[i].Position.top, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);
                RECT pWZ = HScalePointsToButton(Actions[i].Position.right, Actions[i].Position.bottom, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);

                RECT rectPoint = { pXY.left, pXY.top, pWZ.left, pWZ.top };

                Rectangle(compatibleDC, rectPoint.left, rectPoint.top, rectPoint.right, rectPoint.bottom);

                DeleteObject(hBrush);
                DeleteObject(hPen);
            }
            else if (Actions[i].Tool == TEllipse) {
                HBRUSH hBrush = CreateSolidBrush(Actions[i].Color);
                HPEN hPen = CreatePen(PS_SOLID, 1, Actions[i].Color);

                SelectObject(compatibleDC, hBrush);
                SelectObject(compatibleDC, hPen);

                RECT pXY = HScalePointsToButton(Actions[i].Position.left, Actions[i].Position.top, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);
                RECT pWZ = HScalePointsToButton(Actions[i].Position.right, Actions[i].Position.bottom, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);

                RECT rectPoint = { pXY.left - 1, pXY.top - 1, pWZ.left + 1, pWZ.top + 1 };

                Ellipse(compatibleDC, rectPoint.left, rectPoint.top, rectPoint.right, rectPoint.bottom);

                DeleteObject(hBrush);
                DeleteObject(hPen);
            }
            else if (Actions[i].Tool == TLine) {
                HPEN hPen = CreatePen(PS_SOLID, 1, Actions[i].Color);

                SelectObject(compatibleDC, hPen);

                RECT pXY = HScalePointsToButton(Actions[i].Line.startPoint.x, Actions[i].Line.startPoint.y, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);
                RECT pWZ = HScalePointsToButton(Actions[i].Line.endPoint.x, Actions[i].Line.endPoint.y, (WindowRC.right - WindowRC.left), (WindowRC.bottom - WindowRC.top), (rc.right - rc.left), (rc.bottom - rc.top), false, 1);

                RECT rectPoint = { pXY.left - 1, pXY.top - 1, pWZ.left + 1, pWZ.top + 1 };

                MoveToEx(compatibleDC, rectPoint.left, rectPoint.top, NULL);
                LineTo(compatibleDC, rectPoint.right, rectPoint.bottom);

                DeleteObject(hPen);
            }
            else if (Actions[i].Tool == TPaintBucket) {
                RECT XY = HScalePointsToButton(
                    Actions[i].mouseX,
                    Actions[i].mouseY,
                    (WindowRC.right - WindowRC.left),
                    (WindowRC.bottom - WindowRC.top),
                    (rc.right - rc.left),
                    (rc.bottom - rc.top),
                    false,
                    1
                );

                COLORREF clickedColor = GetPixel(compatibleDC, XY.left, XY.top);

                HBRUSH fillBrush = CreateSolidBrush(Actions[i].FillColor);
                SelectObject(compatibleDC, fillBrush);

                ExtFloodFill(compatibleDC, XY.left, XY.top, clickedColor, FLOODFILLSURFACE);

                DeleteObject(fillBrush);
            }
        }
    }

    DeleteDC(compatibleDC);
    ReleaseDC(LayerButtons[layerIndex].button, hdc);

    RedrawWindow(layersHWND, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}