#include "pch.h"
#include "Animation.h"
#include "Tools.h"
#include "Layers.h"

void TSetAnimationFrame(int AnimationFrame) {
    CurrentFrameIndex = AnimationFrame;
}

void TReorganizeFrames() {
    //Re-organize Frames

    RECT rcParent;
    GetClientRect(timelineHWND, &rcParent);

    int timelineParentWidth = rcParent.right - rcParent.left;

    std::vector<std::optional<TimelineFrameButton>> validTimelineFrames;
    std::copy_if(
        TimelineFrameButtons.begin(),
        TimelineFrameButtons.end(),
        std::back_inserter(validTimelineFrames),
        [](const std::optional<TimelineFrameButton>& optLayer) {
            return optLayer.has_value();
        }
    );

    for (size_t i = 0; i < validTimelineFrames.size(); ++i) {
        if (!validTimelineFrames[i].has_value()) continue;
        MoveWindow(validTimelineFrames[i].value().frame, (timelineParentWidth / 2) - (btnWidth / 2) + (btnWidth * i), 10, btnWidth, btnHeight, true);
    }
}

void TCreateAnimationFrame(int LayerIndex, int FrameIndex) {
    HWND animationFrame = HCreateTimelineFrameButton(FrameIndex);
    RenderData renderData = HCreateRenderDataHWND(animationFrame);

    renderData.deviceContext->BeginDraw();
    renderData.deviceContext->SetTarget(renderData.bitmap.Get());
    renderData.deviceContext->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));
    renderData.deviceContext->EndDraw();

    TimelineFrameButtons.push_back(TimelineFrameButton{
        layerIndex,
        FrameIndex,
        animationFrame,
        renderData.deviceContext,
        renderData.swapChain,
        renderData.bitmap
    });

    auto it = std::find_if(
        layers.begin(),
        layers.end(),
        [](const std::optional<Layer>& optLayer) {
            return optLayer.has_value() && optLayer->LayerID == layerIndex && optLayer->FrameIndex == CurrentFrameIndex;
        }
    );

    int currentLayerSize = layers.size();

    for(size_t i = 0; i < currentLayerSize; ++i) {
        if (!layers[i].has_value()) continue;

        int layerId = layers[i]->LayerID;

        // Verifica se já existe um layer com o mesmo ID e FrameIndex
        bool exists = std::any_of(
            layers.begin(),
            layers.end(),
            [layerId, FrameIndex](const std::optional<Layer>& optLayer) {
                return optLayer.has_value() &&
                    optLayer->LayerID == layerId &&
                    optLayer->FrameIndex == FrameIndex;
            }
        );

        if (!exists) {
            TAddLayer(false, layerId, FrameIndex);
        }
    }

    renderData.swapChain->Present(1, 0);

    TReorganizeFrames();
}

void TRemoveAnimationFrame() {

    if (layerIndex < 0 || layerIndex >= layers.size()) {
        layerIndex = 0;
    }

    std::for_each(
        layers.begin(),
        layers.end(),
        [](std::optional<Layer>& optLayer) {
            if (optLayer.has_value() && optLayer->FrameIndex == CurrentFrameIndex) {
                optLayer.reset();
            }
        }
    );

    auto lt_btn = std::find_if(
        TimelineFrameButtons.begin(),
        TimelineFrameButtons.end(),
        [](const std::optional<TimelineFrameButton>& optLayer) {
            if (optLayer.has_value()) {
                return optLayer->FrameIndex == CurrentFrameIndex;
            }
            return false;
        }
    );

    if (lt_btn == TimelineFrameButtons.end() || !lt_btn->has_value()) return;

    DestroyWindow(lt_btn->value().frame);
    lt_btn->reset();
        
    for (auto it = Actions.begin(); it != Actions.end();) {
        if (it->FrameIndex == CurrentFrameIndex) {
            it = Actions.erase(it); // erase returns iterator to next element
        }
        else {
            ++it; // only increment if no erase
        }
    }

    auto first_valid_frame = std::find_if(
        TimelineFrameButtons.begin(),
        TimelineFrameButtons.end(),
        [](const std::optional<TimelineFrameButton>& optLayer) {
            return optLayer.has_value();
        }
    );

    if (first_valid_frame == TimelineFrameButtons.end() || !first_valid_frame->has_value()) return;

    int frameIndex = first_valid_frame->value().FrameIndex;
    TSetAnimationFrame(frameIndex);

    TReorganizeFrames();
}

void TRenderFrameThumbnail(int frameIndex, ComPtr<ID2D1DeviceContext> dc) {
    dc->SetTransform(D2D1::Matrix3x2F::Identity());
    dc->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));

    float canvasW = width;
    float canvasH = height;
    float thumbW = BTN_WIDTH_DEFAULT;
    float thumbH = BTN_HEIGHT_DEFAULT;
    float scale = min(thumbW / canvasW, thumbH / canvasH);

    D2D1_MATRIX_3X2_F scaleMatrix = D2D1::Matrix3x2F::Scale(scale, scale);
    dc->SetTransform(scaleMatrix);

    for (const auto& action : Actions) {
        if (action.FrameIndex == frameIndex && action.Tool != TLayer) {
            HRenderAction(action, dc, COLOR_UNDEFINED);
        }
    }
}

void TUpdateAnimation() {
    if (TimelineFrameButtons.empty()) return;

    // percorre todos os frames existentes
    for (size_t frameIndex = 0; frameIndex < TimelineFrameButtons.size(); ++frameIndex) {
        auto& frameOpt = TimelineFrameButtons[frameIndex];
        if (!frameOpt.has_value()) continue;

        auto& frame = frameOpt.value();

        // prepara o contexto do botão do frame
        frame.deviceContext->BeginDraw();
        frame.deviceContext->SetTarget(frame.bitmap.Get());
        frame.deviceContext->SetTransform(D2D1::Matrix3x2F::Identity());
        frame.deviceContext->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));

        // desenha as layers do frame atual
        TRenderFrameThumbnail(static_cast<int>(frameIndex), frame.deviceContext);

        frame.deviceContext->EndDraw();
        frame.swapChain->Present(1, 0);
    }
}

void TRenderAnimation() {
    for (auto& frameOpt : TimelineFrameButtons) {
        if (!frameOpt.has_value()) continue;
        auto& frame = frameOpt.value();

        frame.deviceContext->BeginDraw();
        frame.deviceContext->SetTarget(frame.bitmap.Get());
        frame.deviceContext->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));
        TRenderFrameThumbnail(frame.FrameIndex, frame.deviceContext);
        frame.deviceContext->EndDraw();
        frame.swapChain->Present(1, 0);
    }
}

int counter = 0;
void TAnimationForward() {
    
    int validFramesSize = static_cast<int>(
        std::count_if(
            TimelineFrameButtons.begin(),
            TimelineFrameButtons.end(),
            [](const std::optional<TimelineFrameButton>& optLayer) {
                return optLayer.has_value();
            }
        )
    ) - 1;

    if (counter >= validFramesSize) return;

    int frameIndex = CurrentFrameIndex + 1;
    TSetAnimationFrame(frameIndex);
    TRenderLayers();
    TRenderAnimation();
    HOnScrollWheelTimeline(-1);
    counter++;
}

void TAnimationBackward() {
    if (counter <= 0) return;

    int frameIndex = CurrentFrameIndex - 1;
    TSetAnimationFrame(frameIndex);
    TRenderLayers();
    TRenderAnimation();
    HOnScrollWheelTimeline(1);

    counter--;
}

void TPlayAnimation() {
    if (!isPlayingAnimation) {
        counter = 0;
    }

    isPlayingAnimation = true;

    if (counter < HGetActiveLayersCount())
    {
        TSetAnimationFrame(counter);

        TRenderLayers();

        HSetScrollPosition(counter);

        counter++;
    }
    else {
        g_scrollOffsetTimeline = 0;
        isPlayingAnimation = false;
    }

}

void TPauseAnimation() {
    counter = 0;
    HSetScrollPosition(counter);
    TSetAnimationFrame(counter);
    g_scrollOffsetTimeline = 0;
    isPlayingAnimation = false;
}