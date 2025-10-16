#pragma once
#include "Base.h"

extern HRESULT TInitialize(HWND pmainHWND);
extern HRESULT TInitializeDocument(HWND hWnd, int pWidth, int pHeight, int pPixelSizeRatio, int pBtnWidth, int pBtnHeight);
extern HRESULT TInitializeWrite();
extern HRESULT TInitializeLayerRenderPreview();
extern HRESULT TInitializeLayers(HWND pLayerWindow, HWND pLayers, HWND pControlButtons);
extern HRESULT TInitializeTools(HWND hWnd);
extern HRESULT TInitializeReplay(HWND hWnd);
extern HRESULT TInitializeLayersButtons(HWND* buttonsHwnd);
