#pragma once
#include "Base.h"

extern HRESULT TInitialize(HWND pmainHWND, HWND pdocHWND, int pWidth, int pHeight, int pPixelSizeRatio);
extern HRESULT TInitializeDocument(HWND hWnd, int pWidth, int pHeight, int pPixelSizeRatio);
extern HRESULT TInitializeWrite();
extern HRESULT TInitializeLayerRenderPreview();
extern HRESULT TInitializeLayers(HWND hWnd);
extern HRESULT TInitializeTools(HWND hWnd);
extern HRESULT TInitializeReplay(HWND hWnd);
extern HRESULT TInitializeLayersButtons(HWND* buttonsHwnd);
