#include "pch.h"
#include "Base.h"
#include "Constants.h"
#include "Layers.h"
#include "Render.h"
#include "SurfaceDial.h"

HRESULT TInitialize(HWND pmainHWND, HWND pdocHWND, int pWidth, int pHeight, int pPixelSizeRatio) {
    mainHWND = pmainHWND;
    docHWND = pdocHWND;

    D2D1_FACTORY_OPTIONS options = { D2D1_DEBUG_LEVEL_INFORMATION };
    HRESULT factoryResult = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_MULTI_THREADED,
        __uuidof(ID2D1Factory1),
        &options,
        reinterpret_cast<void**>(pD2DFactory.GetAddressOf())
    );

    if (FAILED(factoryResult)) {
        MessageBox(pdocHWND, L"Erro ao criar Factory", L"Erro", MB_OK);

        return factoryResult;
    }

    TInitializeDocument(pdocHWND, pWidth, pHeight, pPixelSizeRatio);
    InitializeSurfaceDial(pmainHWND);

    return S_OK;
}

HRESULT TInitializeDocument(HWND hWnd, int pWidth, int pHeight, int pPixelSizeRatio) {

    RECT rc;
    GetClientRect(hWnd, &rc);

    width = rc.right - rc.left;
    height = rc.bottom - rc.top;

    if (pWidth != -1 && pHeight != -1) {
        width = pWidth;
        height = pHeight;
    }

    if (pPixelSizeRatio != -1) {
        pixelSizeRatio = pPixelSizeRatio;
    }

    D2D1_SIZE_U size = D2D1::SizeU(width, height);
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(hWnd, size);

    D2D1_RENDER_TARGET_PROPERTIES attempts[] = {
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_HARDWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            0, 0,
            D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE
        ),
        D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            0, 0,
            D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE
        )
    };

    HRESULT renderResult = E_FAIL;
    for (const auto& props : attempts) {
        renderResult = pD2DFactory->CreateHwndRenderTarget(props, hwndProps, &pRenderTarget);
        if (SUCCEEDED(renderResult)) {
            break;
        }
    }

    if (FAILED(renderResult)) {
        wchar_t errorMessage[512];
        FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            renderResult,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            errorMessage,
            (sizeof(errorMessage) / sizeof(wchar_t)),
            nullptr
        );

        wchar_t finalMessage[1024];
        swprintf_s(finalMessage, L"Erro ao criar hWndRenderTarget\nCódigo: 0x%08X\nMensagem: %s", renderResult, errorMessage);
        MessageBox(hWnd, finalMessage, L"Erro", MB_OK | MB_ICONERROR);
        return renderResult;
    }

    return S_OK;
}

HRESULT TInitializeLayerRenderPreview() {
    TRenderLayers();

    TDrawLayerPreview(layerIndex);

    return S_OK;
}

HRESULT TInitializeLayers(HWND hWnd) {
    layersHWND = hWnd;

    return S_OK;
}