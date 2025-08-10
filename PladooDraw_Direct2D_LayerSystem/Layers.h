#pragma once
#include "Base.h"
#include "Structs.h"

extern int TLayersCount();
extern HRESULT TAddLayer(bool fromFile);
extern void TAddLayerButton(HWND layerButton);
extern HRESULT TRemoveLayer();
extern HRESULT __stdcall TRecreateLayers(HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int& layerID, const wchar_t* szButtonClass, const wchar_t* msgText);
extern int TGetLayer();
extern void TSetLayer(int index);
extern void TReorderLayerUp();
extern void TReorderLayerDown();
extern void TRenderActionToTarget(const ACTION& action, ID2D1RenderTarget* target);
extern void TUpdateLayers(int layerIndexTarget);
extern void TRenderLayers();
extern void TDrawLayerPreview(int currentLayer);
