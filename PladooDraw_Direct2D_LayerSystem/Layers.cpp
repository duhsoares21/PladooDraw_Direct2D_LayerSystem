#include "pch.h"
#include "Base.h"
#include "Constants.h"
#include "Helpers.h"
#include "Tools.h"
#include "Layers.h"
#include "Render.h"
#include "Main.h"

/* LAYERS */
    
int TLayersCount() {
    return layers.size();
}

void TUpdateLayerButtonsPosition() {
    size_t count = std::count_if(layers.begin(), layers.end(), [](auto& l) { return l.has_value(); });
    std::vector<LayerOrder> sortedLayers = layersOrder;

    std::vector<LayerOrder> result;
    result.reserve(count);

    for (auto sortedLayer : sortedLayers) {
        if (layers[sortedLayer.layerIndex].has_value() && sortedLayer.layerIndex == layers[sortedLayer.layerIndex].value().LayerID) {
            result.push_back(sortedLayer);
        }
    }

    std::sort(result.begin(), result.end(),
    [](const LayerOrder& a, const LayerOrder& b) {
        return a.layerOrder > b.layerOrder;
    });

    for (size_t i = 0; i < result.size(); ++i) {
        int currentLayerIndex = result[i].layerIndex;

        if (currentLayerIndex >= 0 && currentLayerIndex < LayerButtons.size() && LayerButtons[currentLayerIndex].has_value()) {
            MoveWindow(LayerButtons[currentLayerIndex].value().button, 0, i * btnHeight, btnWidth, btnHeight, TRUE);
        }
    }
}

Microsoft::WRL::ComPtr<ID2D1Bitmap1> CreateEmptyLayerBitmap()
{
    if (!pRenderTarget) return nullptr;

    D2D1_SIZE_U size = D2D1::SizeU(logicalWidth, logicalHeight);

    FLOAT dpiX, dpiY;
    pRenderTarget->GetDpi(&dpiX, &dpiY);

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpiX, dpiY
    );

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap;
    HRESULT hr = pRenderTarget->CreateBitmap(size, nullptr, 0, &props, &bitmap);

    if (FAILED(hr)) {
        std::wcerr << L"Falha ao criar bitmap vazio para layer. HRESULT: " << hr << std::endl;
        return nullptr;
    }

    return bitmap;
}

HRESULT TAddLayer(bool fromFile = false, int currentLayer = -1) {
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> pBitmap = CreateEmptyLayerBitmap();
   
    pRenderTarget->SetTarget(pBitmap.Get());
    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0)); // fully transparent
    pRenderTarget->EndDraw();

    // Add to layers
    Layer layer = { currentLayer, true, pBitmap };
    layers.emplace_back(layer);

    if (!fromFile) {
        LayerOrder layerOrder = { static_cast<int>(layers.size()) - 1, static_cast<int>(layers.size()) - 1 };
        layersOrder.emplace_back(layerOrder);
    }

    if (!fromFile) {
        ACTION action;
        action.Tool = TLayer;
        action.Layer = currentLayer;
        action.isLayerVisible = 1;

        Actions.push_back(action);
    }

    return S_OK;
}

void TReorderLayers(bool isAddingLayer) {
    int counter = isAddingLayer ? 0 : -1;
    for (auto it = LayerButtons.rbegin(); it != LayerButtons.rend(); ++it) {
        if (it->has_value() && IsWindowVisible(it->value().button)) {
            counter++;
            MoveWindow(it->value().button, 0, counter * btnHeight, btnWidth, btnHeight, true);
        }
    }
}

void TAddLayerButton(int LayerButtonID, bool visible = true) {

    HINSTANCE hLayerInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(layersHWND, GWLP_HINSTANCE));

    DWORD style = visible ? WS_CHILD | WS_VISIBLE | BS_BITMAP : WS_CHILD | BS_BITMAP;

    TReorderLayers(true);

    HWND layerButton = CreateWindowEx(
        0,                        
        L"Button",               
        L"",                     
        style,
        0,                           
        0,   
        btnWidth,                    
        btnHeight,                   
        layersHWND,                  
        (HMENU)(intptr_t)LayerButtonID,
        hLayerInstance,              
        NULL                         
    );
    
    SetLayer(LayerButtonID);

    RECT rc;
    GetClientRect(layerButton, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    Microsoft::WRL::ComPtr<ID2D1DeviceContext> layerDeviceContext;
    HRESULT hr = g_pD2DDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        layerDeviceContext.GetAddressOf()
    );

    Microsoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice;
    hr = g_pD3DDevice.As(&dxgiDevice);
    
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);

    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;
    hr = adapter->GetParent(__uuidof(IDXGIFactory2), &dxgiFactory);

    DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
    swapDesc.Width = size.width;
    swapDesc.Height = size.height;
    swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 2;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> layerSwapChain;
    hr = dxgiFactory->CreateSwapChainForHwnd(
        g_pD3DDevice.Get(),
        layerButton,
        &swapDesc,
        nullptr, nullptr,
        &layerSwapChain
    );

    Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
    layerSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    layerDeviceContext->CreateBitmapFromDxgiSurface(
        backBuffer.Get(),
        &bitmapProperties,
        &targetBitmap
    );

    layerDeviceContext->SetTarget(targetBitmap.Get());

    LayerButtons.push_back(LayerButton{
        LayerButtonID,
        layerButton,
        layerDeviceContext,
        layerSwapChain,
    });

    TDrawLayerPreview(LayerButtonID);
}

HRESULT TRemoveLayer(int currentLayer = -1) {

    if (currentLayer != -1) {
        layerIndex = currentLayer;
    }

    if (layers[layerIndex].has_value()) {
        layers[layerIndex].reset();
    }

    for (auto it = Actions.begin(); it != Actions.end();) {
        if (it->Layer == layerIndex) {
            it = Actions.erase(it); // erase returns iterator to next element
        }
        else {
            ++it; // only increment if no erase
        }
    }

    return S_OK;
}

void TRemoveLayerButton(int currentLayer = -1) {
    if (currentLayer != -1) {
        layerIndex = currentLayer;
    }

    if (!LayerButtons[layerIndex].has_value()) return;  
    
    DestroyWindow(LayerButtons[layerIndex].value().button);
    LayerButtons[layerIndex].reset();

    TReorderLayers(false);
}

int TGetLayer() {
    return layerIndex;
}

void TSetLayer(int index) {
    layerIndex = index;
    TSelectedLayerHighlight(layerIndex);
}

void TReorderLayerUp() {
    if (layerIndex < 0 || layerIndex >= static_cast<int>(layersOrder.size()))
        return; // invalido

    int previousOrder = layersOrder[layerIndex].layerOrder;
    if (previousOrder <= 0) return; // já no topo

    int targetOrder = previousOrder - 1;

    auto it = std::find_if(layersOrder.begin(), layersOrder.end(),
        [targetOrder](const LayerOrder& l) { return l.layerOrder == targetOrder; });

    if (it == layersOrder.end()) return; // target não existe
    int index = static_cast<int>(std::distance(layersOrder.begin(), it));

    // troca os valores
    std::swap(layersOrder[layerIndex].layerOrder, layersOrder[index].layerOrder);

    TUpdateLayerButtonsPosition();
}

void TReorderLayerDown() {
    if (layerIndex < 0 || layerIndex >= static_cast<int>(layersOrder.size()))
        return; // invalido

    int previousOrder = layersOrder[layerIndex].layerOrder;
    if (previousOrder >= static_cast<int>(layers.size()) - 1) return; // já no fundo

    int targetOrder = previousOrder + 1;

    auto it = std::find_if(layersOrder.begin(), layersOrder.end(),
        [targetOrder](const LayerOrder& l) { return l.layerOrder == targetOrder; });

    if (it == layersOrder.end()) return; // target não existe
    int index = static_cast<int>(std::distance(layersOrder.begin(), it));

    std::swap(layersOrder[layerIndex].layerOrder, layersOrder[index].layerOrder);

    TUpdateLayerButtonsPosition();
}

void TRenderActionToTarget(const ACTION& action) {

    D2D1_COLOR_F color = HGetRGBColor(action.Color);
    if (action.Tool == TPaintBucket) {
        color = HGetRGBColor(action.FillColor);
    }

    if (pBrush == nullptr) {
        pRenderTarget->CreateSolidColorBrush(color, &pBrush);
    }
    else {
        pBrush->SetColor(color);
    }

    switch (action.Tool) {
    case TEraser:
        pRenderTarget->PushAxisAlignedClip(action.Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
        pRenderTarget->PopAxisAlignedClip();
        break;

    case TRectangle: {
        pRenderTarget->PushAxisAlignedClip(action.Position, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        pRenderTarget->FillRectangle(action.Position, pBrush.Get());
        pRenderTarget->PopAxisAlignedClip();
        break;
    }

    case TEllipse: {
        pRenderTarget->FillEllipse(action.Ellipse, pBrush.Get());
        break;
    }

    case TLine: {
        pRenderTarget->DrawLine(action.Line.startPoint, action.Line.endPoint, pBrush.Get(), action.BrushSize, nullptr);
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

                pRenderTarget->FillRectangle(pixel, pBrush.Get());
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

                pRenderTarget->DrawRectangle(rect, pBrush.Get());
                pRenderTarget->FillRectangle(rect, pBrush.Get());
            }
        }
        break;
    }

    case TPaintBucket: {
        for (const auto& p : action.pixelsToFill) {
            D2D1_RECT_F rect = D2D1::RectF((float)p.x * zoomFactor, (float)p.y * zoomFactor, (float)(p.x * zoomFactor + 1), (float)(p.y * zoomFactor + 1));
            pRenderTarget->FillRectangle(&rect, pBrush.Get());
        }
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
        pRenderTarget->DrawTextLayout(
            D2D1::Point2F(action.Position.left, action.Position.top),
            textLayout.Get(),
            pBrush.Get()
        );
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

    pRenderTarget->SetTarget(layer.value().pBitmap.Get());
    pRenderTarget->BeginDraw();
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    pRenderTarget->Clear(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.0f)); // Transparente

    for (const auto& action : Actions) {
        if (action.Layer == layerIndexTarget) {
            TRenderActionToTarget(action);
        }
    }

    pRenderTarget->EndDraw();
}

void TRenderLayers() {

    // Check swap chain
    if (!g_pSwapChain) {
        OutputDebugStringW(L"TRenderLayers: g_pSwapChain is nullptr. Attempting to reinitialize.\n");
        HRESULT hr = TInitializeDocument(docHWND, -1, -1, -1, btnWidth, btnHeight);
        if (FAILED(hr)) {
            OutputDebugStringW((L"TRenderLayers: Failed to reinitialize document, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
            return;
        }
    }

    // Get backbuffer surface
    Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
    HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(IDXGISurface), &backBuffer);
    if (FAILED(hr)) {
        OutputDebugStringW((L"TRenderLayers: Failed to get backbuffer, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return;
    }

    // Get DPI from pRenderTarget
    FLOAT dpiX, dpiY;
    pRenderTarget->GetDpi(&dpiX, &dpiY);

    // Create bitmap from backbuffer
    D2D1_BITMAP_PROPERTIES1 bitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpiX, dpiY
    );

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
    hr = pRenderTarget->CreateBitmapFromDxgiSurface(backBuffer.Get(), &bitmapProps, pD2DTargetBitmap.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        OutputDebugStringW((L"TRenderLayers: Failed to create bitmap from DXGI surface, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return;
    }

    // Set as target
    pRenderTarget->SetTarget(pD2DTargetBitmap.Get());

    pRenderTarget->BeginDraw();
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));   
        
    std::vector<LayerOrder> sortedLayers = layersOrder;
    std::sort(sortedLayers.begin(), sortedLayers.end(),
        [](const LayerOrder& a, const LayerOrder& b) {
            return a.layerOrder < b.layerOrder; // higher order drawn first
        });

    for (const auto& lo : sortedLayers) {
        int index = lo.layerIndex;
        if (index < 0 || index >= layers.size()) continue;
        if (layers[index].has_value()) {
            if (layers[index].value().isActive) {
                pRenderTarget->DrawBitmap(layers[index].value().pBitmap.Get());
            }
        }
    }

    pRenderTarget->EndDraw();

    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    // Present
    hr = g_pSwapChain->Present(0, 0);
    if (FAILED(hr)) {
        OutputDebugStringW((L"TRenderLayers: Present failed, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
    }

    if (layers[layerIndex].has_value()) {
        TDrawLayerPreview(layerIndex);
    }
}
    
// This function now uses the global pBrush, creating it if it's null.
void TSelectedLayerHighlight(int currentLayer) {
    // Exit if there's nothing to draw on.
    if (LayerButtons.empty()) {
        return;
    }

    // Iterate through all potential layer buttons
    for (size_t i = 0; i < LayerButtons.size(); i++) {
        // Skip if this button slot is empty (e.g., after a layer was deleted)
        if (!LayerButtons[i].has_value()) {
            continue;
        }

        auto& btn = LayerButtons[i].value();
        auto& deviceContext = btn.deviceContext;
        auto& swapChain = btn.swapChain;

        // --- Begin Drawing on the Button's Surface ---
        deviceContext->BeginDraw();
        deviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
        deviceContext->Clear(D2D1::ColorF(D2D1::ColorF::White));

        // Get the button's client area rectangle
        RECT rc;
        GetClientRect(btn.button, &rc);
        D2D1_RECT_F drawRect = D2D1::RectF(0.0f, 0.0f, static_cast<float>(rc.right), static_cast<float>(rc.bottom));

        // 1. Draw the layer's bitmap preview
        if (i < layers.size() && layers[i].has_value()) {
            deviceContext->DrawBitmap(layers[i].value().pBitmap.Get(), drawRect);
        }

        // --- Logic for Creating/Setting the Brush Color ---
        bool isActive = (static_cast<int>(i) == currentLayer);
        D2D1_COLOR_F borderColor;
        float borderWidth;

        if (isActive) {
            // Active layer: use a highlight color and a thicker border
            borderColor = D2D1::ColorF(D2D1::ColorF(0.05f,0.63f,1.0f,0.3f)); // Highlight color
            borderWidth = 3.0f;
        }
        else {
            // Inactive layer: use a subtle color and a standard border
            borderColor = D2D1::ColorF(D2D1::ColorF::LightGray, 0.0f); // Default border color
            borderWidth = 1.0f;
        }

        // Check if the global pBrush needs to be created
        if (pBrush == nullptr) {
            // It doesn't exist, so create it with the current needed color
            deviceContext->CreateSolidColorBrush(borderColor, &pBrush);
        }
        else {
            // It already exists, just update its color
            pBrush->SetColor(borderColor);
        }

        // 2. Draw the border rectangle using the configured pBrush
        if (pBrush) { // Safety check
            deviceContext->FillRectangle(drawRect, pBrush.Get());
        }

        // --- End Drawing and Present ---
        HRESULT hr = deviceContext->EndDraw();
        if (SUCCEEDED(hr)) {
            swapChain->Present(1, 0);
        }
    }
}

void TDrawLayerPreview(int currentLayer) {
    TSelectedLayerHighlight(currentLayer);
}