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
extern HRESULT TCreateRender();
extern DXGI_SWAP_CHAIN_DESC1 TSetSwapChainDescription(int sizeW, int sizeH, DXGI_ALPHA_MODE AlphaMode);
