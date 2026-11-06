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
    float thumbW = static_cast<float>(renderData.size.width);
    float thumbH = static_cast<float>(renderData.size.height);

    float scale = min(thumbW / canvasW, thumbH / canvasH);

    renderData.deviceContext->SetTransform(
        D2D1::Matrix3x2F::Scale(scale, scale)
    );

    std::vector<ACTION> FilteredRedoActions;

    for (int i = 0; i < (int)RedoActions.size(); i++) {
        if (RedoActions[i].Tool == TLayer) continue;
        FilteredRedoActions.push_back(RedoActions[i]);
    }

    int total = static_cast<int>(FilteredRedoActions.size());

    // Cada botão representa um ACTION inteiro (não steps)
    for (int i = 0; i < FrameIndex && i < total; i++) {
        int idx = total - 1 - i;
        HRenderAction(FilteredRedoActions[idx], renderData.deviceContext, COLOR_UNDEFINED);
    }

    renderData.deviceContext->EndDraw();

    if (renderData.swapChain) {
        renderData.swapChain->Present(1, 0);
    }
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

    pRenderTarget->SetTarget(pD2DTargetBitmap.Get());

    pRenderTarget->BeginDraw();
    pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));

    for (int j = 0; j < (int)Actions.size(); j++) {
        HRenderAction(Actions[j], pRenderTarget, COLOR_UNDEFINED);
    }

    pRenderTarget->EndDraw();

    g_pSwapChain->Present(0, 0);
}

void TEditFromThisPoint() {

    std::string text = "Essa é uma ação destrutiva. Todas as ações após esse ponto serão excluídas. Tem certeza que quer continuar?";

    LPCSTR message = text.c_str();
    LPCSTR title = "Editar a partir daqui";

    int clicked = MessageBoxA(mainHWND, message, title, MB_YESNO);

    if (clicked == IDYES) {
        int ActionIndex = lastActiveReplayFrame;

        if (ActionIndex < (int)Actions.size()) {
            Actions.erase(Actions.begin() + ActionIndex + 1, Actions.end());
        }

        RedoActions.clear();
        replayPartialStepCount = 0;
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
    if (!RedoActions.empty() && replayPartialStepCount > 0) {
        ACTION& pendingAction = RedoActions.back();
        if (replayPartialStepCount > 1) {
            replayPartialStepCount--;
            Actions.back() = MakeStepAction(pendingAction, replayPartialStepCount);
        }
        else {
            // Volta do primeiro step para "nenhum step" desta action.
            Actions.pop_back();
            replayPartialStepCount = 0;
        }

        TReplayRender();
        return;
    }

    if (!Actions.empty()) {
        // Remove a última action completa e coloca novamente no topo do redo.
        ACTION lastAction = Actions.back();
        Actions.pop_back();
        RedoActions.push_back(lastAction);

        int totalSteps = GetActionStepCount(lastAction);

        if (lastActiveReplayFrame > 0) {
            lastActiveReplayFrame--;
            HOnScrollWheelTimeline(1);
        }

        if (totalSteps > 1) {
            replayPartialStepCount = totalSteps - 1;
            Actions.push_back(MakeStepAction(lastAction, replayPartialStepCount));
        }
        else {
            replayPartialStepCount = 0;
        }

        TReplayRender();
    }
}

void TReplayForward() {
    if (!RedoActions.empty()) {
        ACTION& pendingAction = RedoActions.back();
        int totalSteps = GetActionStepCount(pendingAction);

        if (replayPartialStepCount == 0) {
            replayPartialStepCount = 1;
            Actions.push_back(MakeStepAction(pendingAction, replayPartialStepCount));
        }
        else {
            replayPartialStepCount++;
            Actions.back() = MakeStepAction(pendingAction, replayPartialStepCount);
        }

        if (replayPartialStepCount >= totalSteps) {
            replayPartialStepCount = 0;
            RedoActions.pop_back();

            if (lastActiveReplayFrame < (int)TimelineFrameButtons.size() - 1) {
                lastActiveReplayFrame++;
            }
            HOnScrollWheelTimeline(-1);
        }

        TReplayRender();
    }
}
