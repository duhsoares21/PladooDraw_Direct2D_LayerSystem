#include "pch.h"
#include "CoreBase.h"
#include "Constants.h"
#include "BackendSelector.h"
#include "Layers.h"
#include "Render.h"
#include "SurfaceDial.h"
#include "Transforms.h"
#include "Helpers.h"
#include "Tools.h"

HRESULT TInitialize(HWND pmainHWND) {
    mainHWND = pmainHWND;

    if (!gGraphicsBackend) {
        gGraphicsBackend = CreateGraphicsBackend();
    }

    return gGraphicsBackend ? gGraphicsBackend->InitializeMainWindow(pmainHWND) : E_FAIL;
}

HRESULT TInitializeDocument(HWND hWnd, int pWidth, int pHeight, int pPixelSizeRatio, int pBtnWidth, int pBtnHeight) {
    docHWND = hWnd;

    RECT rc;
    GetClientRect(hWnd, &rc);

    width = rc.right - rc.left;
    height = rc.bottom - rc.top;

    if (pWidth != -1 && pHeight != -1) {
        width = pWidth;
        height = pHeight;
    }
    
    btnWidth = pBtnWidth;
    btnHeight = pBtnHeight;

    GetClientRect(mainHWND, &rc);

    int mainWidth = (rc.right - rc.left) - 500;

    if (mainWidth < width) {
        zoomFactor = (float)mainWidth / (float)width;
    }

    logicalWidth = static_cast<float>(width);
    logicalHeight = static_cast<float>(height);

    if (pPixelSizeRatio != -1) {
        pixelSizeRatio = pPixelSizeRatio;
    }

    if (!IsWindow(hWnd)) {
        return E_INVALIDARG;
    }

    if (width <= 0 || height <= 0) {
        return E_INVALIDARG;
    }

    if (!gGraphicsBackend) {
        gGraphicsBackend = CreateGraphicsBackend();
    }

    return gGraphicsBackend ? gGraphicsBackend->InitializeDocument(hWnd, pWidth, pHeight, pPixelSizeRatio, pBtnWidth, pBtnHeight) : E_FAIL;
}

HRESULT TInitializeWrite() {
    return gGraphicsBackend ? gGraphicsBackend->InitializeText() : E_FAIL;
}

HRESULT TInitializeLayerRenderPreview() {
    TRenderLayers();
    TZoom();
    return S_OK;
}

HRESULT TInitializeLayers(HWND pLayerWindow, HWND pLayers, HWND pControlButtons) {

    layerWindowHWND = pLayerWindow;
    layersHWND = pLayers;
    layersControlButtonsGroupHWND = pControlButtons;

    return S_OK;
}

HRESULT TInitializeTools(HWND hWnd) {
    toolsHWND = hWnd;

    return S_OK;
}

HRESULT TInitializeTimeline(HWND hWnd) {
    timelineHWND = hWnd;

    return S_OK;
}

HRESULT TInitializeLayersButtons(HWND* buttonsHwnd) {

    buttonUp = buttonsHwnd[0];
    buttonDown = buttonsHwnd[1];
    buttonPlus = buttonsHwnd[2];
    buttonMinus = buttonsHwnd[3];

    return S_OK;
}

