#include "pch.h"
#include "Replay.h"
#include "Layers.h"
#include "Helpers.h"
#include "Tools.h"

void TCreateReplayFrame(int FrameIndex) {

    HWND replayFrame = HCreateTimelineFrameButton(FrameIndex);

    RenderData renderData = HCreateRenderDataHWND(replayFrame);  

    renderData.deviceContext->SetTarget(renderData.bitmap.Get());
    
    TimelineFrameButtons.push_back(TimelineFrameButton{
        layerIndex,
        FrameIndex,
        replayFrame,
        renderData.deviceContext,
        renderData.swapChain,
        renderData.bitmap
    });

    renderData.deviceContext->BeginDraw();
    renderData.deviceContext->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));

    float canvasW = width;
    float canvasH = height;
    float thumbW = renderData.size.width;
    float thumbH = renderData.size.height;

    float scale = min(thumbW / canvasW, thumbH / canvasH);

    renderData.deviceContext->SetTransform(
        D2D1::Matrix3x2F::Scale(scale, scale)
    );

    std::vector<ACTION> FilteredRedoActions;

    for (int i = 0; i < RedoActions.size(); i++) {
        if (RedoActions[i].Tool == TLayer) continue;
        FilteredRedoActions.push_back(RedoActions[i]);
    }

    int total = static_cast<int>(FilteredRedoActions.size());

    for (int i = 0; i < FrameIndex && i < total; i++) {
        int idx = total - 1 - i;
        HRenderAction(FilteredRedoActions[idx], renderData.deviceContext, COLOR_UNDEFINED);
    }
        
    renderData.deviceContext->EndDraw();

    renderData.swapChain->Present(1, 0);
}

void TReplayClearLayers() {
    for (const auto& layer : layerBitmaps) {
        if (layer.pBitmap) {
            pRenderTarget->BeginDraw();
            pRenderTarget->SetTarget(layer.pBitmap.Get());
            pRenderTarget->Clear(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.0f));
            pRenderTarget->EndDraw();
        }
    }
}

void TReplayRender() {

    std::vector<LayerOrder> sortedLayers = layersOrder;
    std::sort(sortedLayers.begin(), sortedLayers.end(),
    [](const LayerOrder& a, const LayerOrder& b) {
        return a.layerOrder < b.layerOrder; // higher order drawn first
    });

    Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
    HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(IDXGISurface), &backBuffer);
    if (FAILED(hr)) {
        OutputDebugStringW((L"TRenderLayers: Failed to get backbuffer, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return;
    }

    for (auto it = layers.begin(); it != layers.end(); ++it) {

        if (layerBitmaps.size() > 0) {
            if (it->has_value()) {
                const auto targetLayerID = it->value().LayerID;

                auto layer = std::find_if(
                    layerBitmaps.begin(),
                    layerBitmaps.end(),
                    [&](const Layer& l) {
                        return l.LayerID == targetLayerID;
                    }
                );

                if (layer != layerBitmaps.end()) {
                    continue;
                }
            }
        }

        Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
        D2D1_BITMAP_PROPERTIES1 targetBitmapProps = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            dpiX, dpiY
        );

        D2D1_SIZE_U size = D2D1::SizeU(logicalWidth, logicalHeight);
        HRESULT hr = pRenderTarget->CreateBitmap(size, nullptr, 0, &targetBitmapProps, &targetBitmap);

        if (SUCCEEDED(hr)) {
            pRenderTarget->BeginDraw();
            pRenderTarget->SetTarget(targetBitmap.Get());
            pRenderTarget->Clear(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.0f));
            pRenderTarget->EndDraw();

            layerBitmaps.push_back(Layer{
                 it->value().LayerID,
                 it->value().FrameIndex,
                 it->value().isActive,
                 it->value().isVisible,
                 targetBitmap
            });
        }
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

    hr = pRenderTarget->CreateBitmapFromDxgiSurface(backBuffer.Get(), &bitmapProps, pD2DTargetBitmap.ReleaseAndGetAddressOf());

    if (FAILED(hr)) {
        OutputDebugStringW((L"TRenderLayers: Failed to create bitmap from DXGI surface, HRESULT: 0x" + std::to_wstring(hr) + L"\n").c_str());
        return;
    }

    pRenderTarget->BeginDraw();

    for (int j = 0; j <= Actions.size() && j < Actions.size(); j++) {
        pRenderTarget->SetTarget(layerBitmaps[Actions[j].Layer].pBitmap.Get());
        
        (Actions[j], pRenderTarget, COLOR_UNDEFINED);
    }

    pRenderTarget->EndDraw();

    pRenderTarget->SetTarget(pD2DTargetBitmap.Get());

    pRenderTarget->BeginDraw();
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));

    for (const auto& lo : sortedLayers) {
        int index = lo.layerIndex;
        if (index < 0 || index >= layers.size()) continue;
        if (layerBitmaps[index].isActive) {
            pRenderTarget->DrawBitmap(layerBitmaps[index].pBitmap.Get());
        }
    }

    pRenderTarget->EndDraw();

    g_pSwapChain->Present(1, 0);
}

void TEditFromThisPoint() {

    std::string text = "Essa é uma ação destrutiva. Todas as ações após esse ponto serão excluídas. Tem certeza que quer continuar?";

    LPCSTR message = text.c_str();
    LPCSTR title = "Editar a partir daqui";

    int clicked = MessageBoxA(mainHWND, message, title, MB_YESNO);

    if (clicked == IDYES) {
        int ActionIndex = lastActiveReplayFrame;

        if (ActionIndex < Actions.size()) {
            Actions.erase(Actions.begin() + ActionIndex + 1, Actions.end());
        }

        RedoActions.clear();
    }
    else
    {
        std::string text = "Ação cancelada.";

        LPCSTR message = text.c_str();
        LPCSTR title = "Editar a partir daqui";

        int clicked = MessageBoxA(mainHWND, message, title, MB_OK);
    }
}

void TReplayBackwards() {
    if (Actions.size() > 0) {
        ACTION lastAction = Actions.back();
        RedoActions.push_back(lastAction);
        Actions.pop_back();

        lastActiveReplayFrame--;
        HOnScrollWheelTimeline(1);
        TReplayRender();
    }
}

void TReplayForward() {
    if (RedoActions.size() > 0) {
        ACTION action = RedoActions.back();
        Actions.push_back(action);
        RedoActions.pop_back();

        lastActiveReplayFrame++;
        HOnScrollWheelTimeline(-1);
        TReplayRender();
    }
}