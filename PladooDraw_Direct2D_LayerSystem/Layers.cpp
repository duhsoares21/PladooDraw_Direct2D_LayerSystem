#include "pch.h"
#include "CoreBase.h"
#include "Constants.h"
#include "Helpers.h"
#include "Tools.h"
#include "Layers.h"
#include "Render.h"
#include "Main.h"
#include "Animation.h"

/* LAYERS */

int TLayersCount() {
    int total_layers = 0;

    for (size_t i = 0; i < layers.size(); ++i) {
        if (!layers[i].has_value()) continue;  // guard: skip nullopt slots
        if (layers[i].value().LayerID > total_layers) {
            total_layers = layers[i].value().LayerID;
        }
    }

    return total_layers + 1;
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

RenderData CreateEmptyLayerBitmap()
{
    // logicalWidth/Height are set by TInitializeDocument. If AddLayer is called
    // before that (or the window was 0-sized), we must not pass 0x0 to the
    // OpenGL backend - a 0x0 FBO is incomplete and CreateBitmapRenderData
    // returns {}, causing TAddLayer to silently drop the layer entirely.
    // Use a 1x1 fallback; the FBO will be recreated at the real size once
    // TInitializeDocument runs and the layer is replayed/re-rendered.
    float w = logicalWidth > 0.0f ? logicalWidth : 1.0f;
    float h = logicalHeight > 0.0f ? logicalHeight : 1.0f;
    return HCreateRenderData(
        SizeU{
            static_cast<uint32_t>(w),
            static_cast<uint32_t>(h)
        }
    );
}

HRESULT TAddLayer(bool fromFile = false, int currentLayer = -1, int currentFrame = -1) {
    RenderData renderData = CreateEmptyLayerBitmap();
    if (!renderData.surfaceHandle || !renderData.bitmapHandle) {
        HCreateLogData("error.log", "TAddLayer: CreateEmptyLayerBitmap returned empty RenderData - layer NOT added");
        return E_FAIL;
    }
    HCreateLogData("error.log", "TAddLayer: CreateEmptyLayerBitmap OK - layer will be added");

    renderData.surfaceHandle->BeginDraw();
    renderData.surfaceHandle->SetTransform(MakeIdentityMatrix3x2());
    renderData.surfaceHandle->Clear(ColorRGBA{ 1.0f, 1.0f, 1.0f, 0.0f });
    renderData.surfaceHandle->EndDraw();

    if (currentFrame == -1) {
        currentFrame = 0;
    }

    // Add to layers
    Layer layer = { currentLayer, currentFrame, true, true, renderData.surfaceHandle, renderData.bitmapHandle };
    layers.emplace_back(layer);

    if (!fromFile && currentFrame == 0) {
        LayerOrder layerOrder = { TLayersCount() - 1, TLayersCount() - 1 };
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

    int currentLayerSize = layers.size();

    for (size_t i = 0; i < currentLayerSize; ++i) {
        if (!layers[i].has_value()) continue;

        int frameindex = layers[i]->FrameIndex;

        // Verifica se já existe um layer com o mesmo ID e FrameIndex
        bool exists = std::any_of(
            layers.begin(),
            layers.end(),
            [currentLayer, frameindex](const std::optional<Layer>& optLayer) {
                return optLayer.has_value() &&
                    optLayer->LayerID == currentLayer &&
                    optLayer->FrameIndex == frameindex;
            }
        );

        if (!exists) {
            TAddLayer(false, currentLayer, frameindex);
        }
    }

    return S_OK;
}

void TResizeLayers() {
    // Called after TInitializeDocument sets logicalWidth/logicalHeight.
    // Any layer whose surface was created with the 0x0->1x1 fallback (because
    // AddLayer was called before the document size was known) needs its FBO
    // and texture replaced at the real canvas size.
    if (logicalWidth <= 0.0f || logicalHeight <= 0.0f) return;

    SizeU realSize = {
        static_cast<uint32_t>(logicalWidth),
        static_cast<uint32_t>(logicalHeight)
    };

    for (auto& optLayer : layers) {
        if (!optLayer.has_value()) continue;
        Layer& layer = optLayer.value();

        // Check if the surface size is wrong (e.g. the 1x1 fallback)
        if (layer.surfaceHandle) {
            SizeU current = layer.surfaceHandle->GetSize();
            if (current.width == realSize.width && current.height == realSize.height) continue;
        }

        // Recreate at the correct size
        RenderData rd = HCreateRenderData(realSize);
        if (!rd.surfaceHandle || !rd.bitmapHandle) continue;

        rd.surfaceHandle->BeginDraw();
        rd.surfaceHandle->SetTransform(MakeIdentityMatrix3x2());
        rd.surfaceHandle->Clear(ColorRGBA{ 1.0f, 1.0f, 1.0f, 0.0f });
        rd.surfaceHandle->EndDraw();

        layer.surfaceHandle = rd.surfaceHandle;
        layer.bitmapHandle = rd.bitmapHandle;
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

    RenderData renderData = HCreateRenderDataHWND(layerButton);

    LayerButtons.push_back(LayerButton{
        LayerButtonID,
        CurrentFrameIndex,
        layerButton,
        visible,
        renderData.surfaceHandle
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
    }
    else {
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
        CurrentFrameIndexTarget = CurrentFrameIndex;
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

    // 3. Renderizar todas as acoes dentro do layer encontrado
    auto& layer = itLayer->value();
    if (!layer.surfaceHandle) {
        return;
    }

    layer.surfaceHandle->BeginDraw();
    layer.surfaceHandle->SetTransform(MakeIdentityMatrix3x2());
    layer.surfaceHandle->Clear(ColorRGBA{ 1.0f, 1.0f, 1.0f, 0.0f });

    for (auto& action : validActions) {
        HRenderAction(action, *layer.surfaceHandle);
    }

    layer.surfaceHandle->EndDraw();
}

void TRenderLayers() {

    if (!gGraphicsBackend) {
        return;
    }

    RenderSurfacePtr documentSurface = gGraphicsBackend->GetDocumentSurface();
    if (!documentSurface) {
        return;
    }

    documentSurface->BeginDraw();
    documentSurface->Clear(ColorRGBA{ 1.0f, 1.0f, 1.0f, 1.0f });
    documentSurface->SetTransform(MakeIdentityMatrix3x2());

    std::vector<LayerOrder> sortedLayers = layersOrder;
    std::sort(sortedLayers.begin(), sortedLayers.end(),
        [](const LayerOrder& a, const LayerOrder& b) {
            return a.layerOrder < b.layerOrder;
        });

    for (const auto& lo : sortedLayers) {
        int index = lo.layerIndex;
        if (index < 0 || index >= layers.size()) continue;

        auto currentLayer = std::find_if(
            layers.begin(),
            layers.end(),
            [index](const std::optional<Layer>& optLayer) {
                return optLayer.has_value() &&
                    optLayer->LayerID == index &&
                    optLayer->FrameIndex == CurrentFrameIndex;
            }
        );

        if (currentLayer != layers.end() && currentLayer->has_value() && currentLayer->value().bitmapHandle) {
            documentSurface->DrawBitmap(*currentLayer->value().bitmapHandle);
        }

        if (CurrentFrameIndex > 0) {
            auto previousLayer = std::find_if(
                layers.begin(),
                layers.end(),
                [index](const std::optional<Layer>& optLayer) {
                    return optLayer.has_value() &&
                        optLayer->LayerID == index &&
                        optLayer->FrameIndex == CurrentFrameIndex - 1;
                }
            );

            if (previousLayer != layers.end() &&
                previousLayer->has_value() &&
                previousLayer->value().bitmapHandle &&
                isAnimationMode &&
                CurrentFrameIndex > 0 &&
                previousLayer->value().isVisible &&
                !isPlayingAnimation &&
                !hideShadow) {
                documentSurface->DrawBitmap(*previousLayer->value().bitmapHandle, nullptr, 0.2f);
            }
        }
    }

    documentSurface->EndDraw();
    documentSurface->Present(0);

    if (layerIndex >= 0 && layerIndex < layers.size() && layers[layerIndex].has_value()) {
        TDrawLayerPreview(layerIndex);
    }
}

void TSelectedLayerHighlight(int currentLayer) {
    if (LayerButtons.empty()) {
        return;
    }

    for (size_t i = 0; i < LayerButtons.size(); i++) {
        if (!LayerButtons[i].has_value()) {
            continue;
        }

        auto& btn = LayerButtons[i].value();
        if (!btn.surfaceHandle) {
            continue;
        }

        btn.surfaceHandle->BeginDraw();
        btn.surfaceHandle->SetTransform(MakeIdentityMatrix3x2());
        btn.surfaceHandle->Clear(ColorRGBA{ 1.0f, 1.0f, 1.0f, 1.0f });

        RECT rc;
        GetClientRect(btn.button, &rc);
        RectF drawRect = MakeRectF(0.0f, 0.0f, static_cast<float>(rc.right), static_cast<float>(rc.bottom));

        auto it = std::find_if(
            layers.begin(),
            layers.end(),
            [&](const std::optional<Layer>& optLayer) {
                return optLayer.has_value()
                    && optLayer->LayerID == btn.LayerID
                    && optLayer->FrameIndex == CurrentFrameIndex;
            }
        );

        if (it != layers.end() && it->has_value() && it->value().bitmapHandle) {
            btn.surfaceHandle->DrawBitmap(*it->value().bitmapHandle, &drawRect);
        }

        bool isActive = (static_cast<int>(i) == currentLayer);
        ColorRGBA borderColor;
        float borderWidth;

        if (isActive) {
            borderColor = ColorRGBA{ 0.05f, 0.63f, 1.0f, 0.3f };
            borderWidth = 3.0f;
        }
        else {
            borderColor = ColorRGBA{ 0.83f, 0.83f, 0.83f, 0.0f };
            borderWidth = 1.0f;
        }

        if (borderWidth > 0.0f) {
            btn.surfaceHandle->StrokeRect(drawRect, borderColor, borderWidth);
        }

        HRESULT hr = btn.surfaceHandle->EndDraw();
        if (SUCCEEDED(hr)) {
            btn.surfaceHandle->Present(1);
        }
    }
}
void TDrawLayerPreview(int currentLayer) {
    TSelectedLayerHighlight(currentLayer);
}