#pragma once
#include "Base.h"
#include "Structs.h"

extern int TLayersCount();
extern HRESULT TAddLayer(bool fromFile, int currentLayer);
extern void TAddLayerButton(HWND layerButton);
extern void TRemoveLayerButton();
extern HRESULT TRemoveLayer();
extern int TGetLayer();
extern void TSetLayer(int index);
extern void TReorderLayerUp();
extern void TReorderLayerDown();
extern void TRenderActionToTarget(const ACTION& action);
extern void TUpdateLayers(int layerIndexTarget);
extern void TRenderLayers();
extern void TDrawLayerPreview(int currentLayer);
