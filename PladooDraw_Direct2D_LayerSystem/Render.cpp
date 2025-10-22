#include "pch.h"
#include "Base.h"
#include "Constants.h"
#include "Layers.h"
#include "Render.h"
#include "SurfaceDial.h"
#include "Transforms.h"
#include "Helpers.h"
#include "Tools.h"

HRESULT TInitialize(HWND pmainHWND) {
    mainHWND = pmainHWND;

    HRESULT hr = RoInitialize(RO_INIT_SINGLETHREADED);

    if (FAILED(hr)) {
        // Handle the error, for example, by logging or returning FALSE  
        MessageBox(NULL, L"Failed to initialize Windows Runtime", L"Error", MB_OK | MB_ICONERROR);
        return FALSE;
    }
    
    D2D1_FACTORY_OPTIONS options = { D2D1_DEBUG_LEVEL_INFORMATION };
    HRESULT factoryResult = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_MULTI_THREADED,
        __uuidof(ID2D1Factory1),
        &options,
        reinterpret_cast<void**>(pD2DFactory.GetAddressOf())
    );

    if (FAILED(factoryResult)) {
        MessageBox(pmainHWND, L"Erro ao criar Factory", L"Erro", MB_OK);
            
        return factoryResult;
    }

    return S_OK;
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

    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    #ifdef _DEBUG
        creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags,
        featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
        &g_pD3DDevice, &featureLevel, &g_pD3DContext
    );

    if (FAILED(hr)) {
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, creationFlags,
            featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
            &g_pD3DDevice, &featureLevel, &g_pD3DContext
        );
        if (FAILED(hr)) {
            MessageBox(hWnd, L"Failed to create D3D11 device", L"Error", MB_OK);
            return hr;
        }
    }

    // Get DXGI device
    Microsoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice;
    hr = g_pD3DDevice.As(&dxgiDevice);
    if (FAILED(hr)) {
        return hr;
    }

    // Create D2D1 device
    hr = pD2DFactory->CreateDevice(dxgiDevice.Get(), &g_pD2DDevice);
    if (FAILED(hr)) {
        return hr;
    }

    hr = g_pD2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &pRenderTarget);

    if (FAILED(hr)) {
        HPrintHResultError(hr);
        return hr;
    }

    // Create DXGI factory
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr)) {
        return hr;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;
    hr = adapter->GetParent(__uuidof(IDXGIFactory2), &dxgiFactory);

    if (FAILED(hr)) {
        return hr;
    }

    DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
    swapDesc.Width = width;
    swapDesc.Height = height;
    swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 2;
    swapDesc.Scaling = DXGI_SCALING_STRETCH;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapDesc.Flags = 0;

    //hr = dxgiFactory->CreateSwapChainForComposition()
    hr = dxgiFactory->CreateSwapChainForHwnd(g_pD3DDevice.Get(), hWnd, &swapDesc, nullptr, nullptr, &g_pSwapChain);
    
    if (FAILED(hr)) {
        return hr;
    }

    hr = dxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(hr)) {
        return hr;
    }

    if (!g_pSwapChain) {
        return E_FAIL;
    }

    CreateGlobalRenderBitmap();

    return S_OK;
}

HRESULT TInitializeWrite() {
    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(pDWriteFactory.GetAddressOf())
    );
    if (FAILED(hr)) {
        MessageBox(NULL, L"Failed to create DirectWrite factory", L"Error", MB_OK);
    }

    return S_OK;
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

HRESULT TInitializeReplay(HWND hWnd) {
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