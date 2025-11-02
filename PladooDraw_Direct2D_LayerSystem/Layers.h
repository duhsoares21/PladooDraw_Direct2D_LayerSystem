#pragma once
#include "Base.h"
#include "Structs.h"

extern Microsoft::WRL::ComPtr<ID2D1Bitmap1> CreateEmptyLayerBitmap();

extern int TLayersCount();
extern HRESULT TAddLayer(bool fromFile, int currentLayer, int currentFrame);
extern void TReorderLayers(bool isAddingLayer);
extern void TAddLayerButton(int LayerButtonID, bool visible);
extern void THideAllUnusedLayerButtons();
extern void TRemoveLayerButton(int currentLayer);
extern HRESULT TRemoveLayer(int currentLayer);
extern int TGetLayer();
extern void TSetLayer(int index);
extern void TShowCurrentLayerOnly();
extern void TReorderLayerUp();
extern void TReorderLayerDown();
extern void TUpdateLayers(int layerIndexTarget, int CurrentFrameIndexTarget);
extern void TRenderLayers();
extern void TDrawLayerPreview(int currentLayer);
extern void TSelectedLayerHighlight(int currentLayer);