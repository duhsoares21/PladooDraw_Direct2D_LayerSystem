#pragma once
#include "Base.h"
#include "Structs.h"

extern Microsoft::WRL::ComPtr<ID2D1Bitmap1> CreateEmptyLayerBitmap();

extern int TLayersCount();
extern HRESULT TAddLayer(bool fromFile, int currentLayer);
extern void TReorderLayers(bool isAddingLayer);
extern void TAddLayerButton(int LayerButtonID, bool visible);
extern void TRemoveLayerButton(int currentLayer);
extern HRESULT TRemoveLayer(int currentLayer);
extern int TGetLayer();
extern void TSetLayer(int index);
extern void TReorderLayerUp();
extern void TReorderLayerDown();
extern void TRenderActionToTarget(const ACTION& action);
extern void TUpdateLayers(int layerIndexTarget);
extern void TRenderLayers();
extern void TDrawLayerPreview(int currentLayer);
extern void TSelectedLayerHighlight(int currentLayer);