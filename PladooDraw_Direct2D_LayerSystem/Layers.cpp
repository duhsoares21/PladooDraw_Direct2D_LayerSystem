#include "pch.h"
#include "Base.h"
#include "Constants.h"
#include "Helpers.h"
#include "Tools.h"
#include "Layers.h"
#include "Render.h"

/* LAYERS */

int TLayersCount() {
    return layers.size();
}

HRESULT TAddLayer(bool fromFile = false) {
    // Use logical size (not physical/zoomed size from pRenderTarget)
    D2D1_SIZE_F size = D2D1::SizeF(logicalWidth, logicalHeight);

    FLOAT dpiX, dpiY;
    pRenderTarget->GetDpi(&dpiX, &dpiY);

    // Create offscreen bitmap (targetable)
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> pBitmap;
    D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        dpiX, dpiY
    );

    HRESULT hr = pRenderTarget->CreateBitmap(
        D2D1::SizeU(static_cast<UINT32>(size.width), static_cast<UINT32>(size.height)),
        nullptr, 0, &bp, &pBitmap
    );

    if (FAILED(hr)) {
        MessageBox(NULL, L"Failed to create bitmap", L"Error", MB_OK);
        return hr;
    }

    pRenderTarget->SetTarget(pBitmap.Get());
    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0)); // fully transparent
    pRenderTarget->EndDraw();

    // Add to layers
    Layer layer = { pBitmap };
    layers.emplace_back(layer);

    if (!fromFile) {
        LayerOrder layerOrder = { static_cast<int>(layers.size()) - 1, static_cast<int>(layers.size()) - 1 };
        layersOrder.emplace_back(layerOrder);
    }

    return S_OK;
}

void TAddLayerButton(HWND layerButton) {
    // Get client rect for size
    RECT rc;
    GetClientRect(layerButton, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    // Create another device context from the same D2D device (not HWND render target)
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> layerDeviceContext;
    HRESULT hr = g_pD2DDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        &layerDeviceContext
    );

    Microsoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice;
    hr = g_pD3DDevice.As(&dxgiDevice);
    
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);

    // Create swap chain for this layer window
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

    // Set the swap chain as target for the device context
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

    // Store the device context and swap chain
    LayerButtons.push_back({
        layerButton,
        layerDeviceContext.Get(),
        layerSwapChain.Get(),
    });

    TDrawLayerPreview(GetDlgCtrlID(layerButton));
}

HRESULT TRemoveLayer() {

    int idx = layerIndex;

    // Validate layerIndex
    if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
        return E_INVALIDARG;

    // Remove the layer
    layers.erase(layers.begin() + layerIndex);

    // Remove from layersOrder
    layersOrder.erase(
        std::remove_if(layersOrder.begin(), layersOrder.end(),
            [idx](const LayerOrder& lo) { return lo.layerIndex == layerIndex; }),
        layersOrder.end()
    );

    // Update remaining layer indices
    for (auto& lo : layersOrder) {
        if (lo.layerIndex > layerIndex)
            lo.layerIndex--;
        lo.layerOrder--; // only if this is necessary
    }

    // Remove associated Actions
    Actions.erase(
        std::remove_if(Actions.begin(), Actions.end(),
            [idx](const ACTION& a) { return a.Layer == layerIndex; }),
        Actions.end()
    );

    // Shift layers in Actions
    for (auto& a : Actions) {
        if (a.Layer > layerIndex)
            a.Layer--;
    }

    // Ensure at least 1 layer exists
    if (layers.empty())
        TAddLayer();

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
}

void TRenderActionToTarget(const ACTION& action) {

    D2D1_COLOR_F color = HGetRGBColor(action.Color);

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
        Microsoft::WRL::ComPtr<IDWriteTextFormat> pTextFormat;

        pDWriteFactory->CreateTextFormat(
            L"Segoe UI",                // Font family
            nullptr,                    // Font collection (nullptr = system)
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            24.0f,                      // Font size in DIPs
            L"en-us",                   // Locale
            &pTextFormat
        );

        const WCHAR* text = action.Text;
        size_t textLength = wcslen(text);

        pRenderTarget->DrawTextW(
            text,               // Text
            static_cast<UINT32>(textLength),
            pTextFormat.Get(),  // Text format
            action.Position,           // Layout rectangle
            pBrush.Get()         // Brush
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

    pRenderTarget->SetTarget(layer.pBitmap.Get());
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
        HRESULT hr = TInitializeDocument(docHWND, -1, -1, -1);
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
            return a.layerOrder < b.layerOrder; // lower order drawn first
        });

    for (const auto& lo : sortedLayers) {
        int index = lo.layerIndex;
        if (index < 0 || index >= layers.size()) continue;
        pRenderTarget->DrawBitmap(layers[index].pBitmap.Get());
    }

    pRenderTarget->EndDraw();

    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

    // Present
    hr = g_pSwapChain->Present(0, 0);
    if (FAILED(hr)) {
        OutputDebugStringW((L"TRenderLayers: Present failed, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
    }

    TDrawLayerPreview(layerIndex);
}

void TDrawLayerPreview(int currentLayer) {
    RECT rc;
    GetClientRect(LayerButtons[currentLayer].button, &rc);

    LayerButtons[currentLayer].deviceContext->BeginDraw();
    LayerButtons[currentLayer].deviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
    LayerButtons[currentLayer].deviceContext->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));

    // If you want to draw the actual layer content, you'd do something like:
    LayerButtons[currentLayer].deviceContext->DrawBitmap(layers[currentLayer].pBitmap.Get(), D2D1::RectF(rc.left, rc.top, rc.right, rc.bottom));

    HRESULT hr = LayerButtons[currentLayer].deviceContext->EndDraw();

    // Present the swap chain for this layer window
    LayerButtons[currentLayer].swapChain->Present(1, 0);
}