#include "pch.h"
#include "Base.h"
#include "Constants.h"
#include "Helpers.h"
#include "Tools.h"
#include "Layers.h"
#include "Render.h"
#include "Main.h"
#include "Animation.h"

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

HRESULT TAddLayer(bool fromFile = false, int currentLayer = -1, int currentFrame = -1) {
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> pBitmap = CreateEmptyLayerBitmap();
   
    pRenderTarget->SetTarget(pBitmap.Get());
    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(1, 1, 1, 0));
    pRenderTarget->EndDraw();

    if (currentFrame == -1) {
        currentFrame = CurrentFrameIndex;
    }

    // Add to layers
    Layer layer = { currentLayer, currentFrame, true, true, pBitmap };
    layers.emplace_back(layer);

    if (!fromFile) {
        LayerOrder layerOrder = { static_cast<int>(layers.size()) - 1, static_cast<int>(layers.size()) - 1 };
        layersOrder.emplace_back(layerOrder);
    }

    if (!fromFile) {
        ACTION action;
        action.Tool = TLayer;
        action.Layer = currentLayer;
        action.FrameIndex = currentFrame;
        action.isLayerVisible = 1;

        Actions.push_back(action);
    }

    return S_OK;
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

    DXGI_SWAP_CHAIN_DESC1 swapDesc = TSetSwapChainDescription(size.width, size.height, DXGI_ALPHA_MODE_IGNORE);

    Microsoft::WRL::ComPtr<ID2D1DeviceContext> layerDeviceContext;
    HRESULT hr = g_pD2DDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        layerDeviceContext.GetAddressOf()
    );

    Microsoft::WRL::ComPtr<IDXGISwapChain1> layerSwapChain;
    g_dxgiFactory->CreateSwapChainForHwnd(
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
        CurrentFrameIndex,
        layerButton,
        layerDeviceContext,
        layerSwapChain,
        visible
    });

    TDrawLayerPreview(LayerButtonID);
}

void THideAllUnusedLayerButtons() {
    for (auto it = LayerButtons.begin(); it != LayerButtons.end(); ++it) {
        if (!it->has_value()) continue;
        if (it->value().FrameIndex != CurrentFrameIndex) {
            ShowWindow(it->value().button, SW_HIDE);
        }
    }

    //REDRAW BUTTONS
    for (auto it = LayerButtons.begin(); it != LayerButtons.end(); ++it) {
        if (!it->has_value()) continue;
        if (it->value().isActive) {
            ShowWindow(it->value().button, SW_SHOW);
        }
        else {
            ShowWindow(it->value().button, SW_HIDE);
        }
    }
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

bool isShowingCurrentLayerOnly = false;
void TShowCurrentLayerOnly() {
    if (isShowingCurrentLayerOnly) {
        for (auto& layer : layers) {
            if (!layer.has_value()) continue;
            layer.value().isVisible = true;
        }
        isShowingCurrentLayerOnly = false;
    } else {
        for (auto& layer : layers) {
            if (!layer.has_value()) continue;
            layer.value().isVisible = (layer.value().LayerID == layerIndex);
        }
        isShowingCurrentLayerOnly = true;
    }
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

void TUpdateLayers(int layerIndexTarget = -1, int CurrentFrameIndexTarget = -1) {

    if (layerIndex < 0 || layerIndex >= layers.size()) {
        layerIndex = 0;
    }

    if (layerIndexTarget == -1) {
        layerIndexTarget = layerIndex;
    }

    if (CurrentFrameIndexTarget == -1) {
        CurrentFrameIndexTarget == CurrentFrameIndex;
    }

    //GetCurrent Frame
    auto itLayer = std::find_if(
        layers.begin(),
        layers.end(),
        [layerIndexTarget, CurrentFrameIndexTarget](const std::optional<Layer>& optLayer) {
            return optLayer.has_value() &&
                optLayer->LayerID == layerIndexTarget &&
                optLayer->FrameIndex == CurrentFrameIndexTarget;
        }
    );

    // Se não achar, sai
    if (itLayer == layers.end() || !itLayer->has_value())
        return;

    // 2. Coletar todas as actions que pertencem a esse mesmo layer/frame
    std::vector<ACTION> validActions;
    std::copy_if(
        Actions.begin(),
        Actions.end(),
        std::back_inserter(validActions),
        [layerIndexTarget, CurrentFrameIndexTarget](const ACTION& act) {
            return act.Layer == layerIndexTarget && act.FrameIndex == CurrentFrameIndexTarget;
        }
    );

    // 3. Renderizar todas as ações dentro do layer encontrado
    auto& layer = itLayer->value(); // referência para evitar cópia
    pRenderTarget->SetTarget(layer.pBitmap.Get());
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.0f));

    for (const auto& action : validActions) {
        HRenderAction(action, pRenderTarget, COLOR_UNDEFINED);
    }

    pRenderTarget->EndDraw();
}

void TRenderLayers() {

    pRenderTarget->SetTarget(pD2DTargetBitmap.Get());
    pRenderTarget->BeginDraw();
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());  
        
    std::vector<LayerOrder> sortedLayers = layersOrder;
    std::sort(sortedLayers.begin(), sortedLayers.end(),
        [](const LayerOrder& a, const LayerOrder& b) {
            return a.layerOrder < b.layerOrder; // higher order drawn first
        });

    for (const auto& lo : sortedLayers) {
        int index = lo.layerIndex;
        if (index < 0 || index >= layers.size()) continue;
        if (!layers[index].has_value()) continue;
        if (layers[index].value().FrameIndex == CurrentFrameIndex && layers[index].value().isVisible) {
            pRenderTarget->DrawBitmap(layers[index].value().pBitmap.Get());
        }

        if (isAnimationMode && CurrentFrameIndex > 0 && layers[index].value().isVisible && !isPlayingAnimation && !hideShadow) {
            if (layers[index].value().FrameIndex == CurrentFrameIndex - 1) {
                pRenderTarget->DrawBitmap(
                    layers[index].value().pBitmap.Get(),
                    nullptr,                       // desenha em toda a área
                    0.2f,                          // 20% de opacidade
                    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR
                );
            }
        }
    }

    pRenderTarget->EndDraw();

    g_pSwapChain->Present(0, 0);
  
    if (layers[layerIndex].has_value()) {
        TDrawLayerPreview(layerIndex);
    }
}
    
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

        auto it = std::find_if(
            layers.begin(),
            layers.end(),
            [&](const std::optional<Layer>& optLayer) {
                return optLayer.has_value()
                    && optLayer->LayerID == btn.LayerID     // compara com LayerID do botão
                    && optLayer->FrameIndex == CurrentFrameIndex;
            }
        );

        if (it != layers.end() && it->has_value()) {
            // desenha o bitmap do layer encontrado (miniatura do frame atual)
            deviceContext->DrawBitmap(it->value().pBitmap.Get(), drawRect);
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