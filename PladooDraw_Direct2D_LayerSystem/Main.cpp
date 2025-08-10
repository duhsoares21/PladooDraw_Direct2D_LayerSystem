// LayersDLL.cpp
#include "pch.h"
#include "Constants.h"
#include "Helpers.h"
#include "Main.h"
#include "Layers.h"
#include "SaveLoad.h"
#include "SurfaceDial.h"
#include "Tools.h"
#include "ToolsAux.h"
#include "Transforms.h"

/* MAIN */

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {  
    HRESULT hr; // Variable to store the return value of RoInitialize  

    switch (fdwReason) {  
        case DLL_PROCESS_ATTACH:  
            hr = RoInitialize(RO_INIT_SINGLETHREADED);  
            if (FAILED(hr)) {  
                // Handle the error, for example, by logging or returning FALSE  
                MessageBox(NULL, L"Failed to initialize Windows Runtime", L"Error", MB_OK | MB_ICONERROR);  
                return FALSE;  
            }  
            break;  

        case DLL_PROCESS_DETACH:  
            CleanupSurfaceDial();  
            HCleanup();  
            break;  

        case DLL_THREAD_ATTACH:  
        case DLL_THREAD_DETACH:  
            break;  
    }  

    return TRUE;  
}

extern "C" __declspec(dllexport) HRESULT Initialize(HWND pmainHWND, HWND pdocHWND, int pWidth, int pHeight, int pPixelSizeRatio) {
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

    InitializeDocument(pdocHWND, pWidth, pHeight, pPixelSizeRatio);
    InitializeSurfaceDial(pmainHWND);
        
    return S_OK;
}

HRESULT InitializeDocument(HWND hWnd, int pWidth, int pHeight, int pPixelSizeRatio) {

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

extern "C" __declspec(dllexport) HRESULT InitializeLayerRenderPreview() {
    RenderLayers();

    DrawLayerPreview(layerIndex);

    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT InitializeLayers(HWND hWnd) {
    layersHWND = hWnd;

    return S_OK;
}

/* HELPERS */

void CreateLogData(std::string fileName, std::string content) {
    HCreateLogData(fileName, content);
}

extern "C" __declspec(dllexport) void __stdcall SaveProjectDll(const char* pathAnsi) {
    int wlen = MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, &wpath[0], wlen);

    SaveBinaryProject(wpath);
}

extern "C" __declspec(dllexport) void __stdcall LoadProjectDll(const char* pathAnsi, HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int* layerID, const wchar_t* szButtonClass, const wchar_t* msgText) {
    int wlen = MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, &wpath[0], wlen);

    LoadBinaryProject(wpath, hWndLayer, hLayerInstance, btnWidth, btnHeight, hLayerButtons, *layerID, L"Button", msgText);
}

/* ACTIONS */

extern "C" __declspec(dllexport) void handleMouseUp() {
    THandleMouseUp();
}

extern "C" __declspec(dllexport) void Undo() {
    TUndo();
}

extern "C" __declspec(dllexport) void Redo() {
    TRedo();
}

/* TRANSFORM */

extern "C" __declspec(dllexport) float __stdcall GetZoomFactor() {
    return TGetZoomFactor();
}

extern "C" __declspec(dllexport) void ZoomIn_Default() {
    TZoomIn(0.5f);
}

extern "C" __declspec(dllexport) void ZoomOut_Default() {
    TZoomOut(0.5f);
}

extern "C" __declspec(dllexport) void ZoomIn(float zoomRatio) {
    TZoomIn(zoomRatio);
}

extern "C" __declspec(dllexport) void ZoomOut(float zoomRatio) {
    TZoomOut(zoomRatio);
}

extern "C" __declspec(dllexport) void Zoom() {
    TZoom();
}

extern "C" __declspec(dllexport) void IncreaseBrushSize_Default() {
    TIncreaseBrushSize(0.5f);
}
extern "C" __declspec(dllexport) void DecreaseBrushSize_Default() {
    TDecreaseBrushSize(0.5f);
}

extern "C" __declspec(dllexport) void IncreaseBrushSize(float sizeRatio) {
    TIncreaseBrushSize(sizeRatio);
}
extern "C" __declspec(dllexport) void DecreaseBrushSize(float sizeRatio) {
    TDecreaseBrushSize(sizeRatio);
}

/* LAYERS */

extern "C" __declspec(dllexport) HRESULT AddLayer(bool fromFile=false) {
    return TAddLayer(fromFile);
}

extern "C" __declspec(dllexport) int LayersCount() {
    return TLayersCount();
}

extern "C" __declspec(dllexport) void AddLayerButton(HWND layerButton) {
    TAddLayerButton(layerButton);
}

extern "C" __declspec(dllexport) HRESULT RemoveLayer() {
    return TRemoveLayer();
}

extern "C" __declspec(dllexport) HRESULT __stdcall RecreateLayers(HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int& layerID, const wchar_t* szButtonClass, const wchar_t* msgText) {
    return TRecreateLayers(hWndLayer, hLayerInstance, btnWidth, btnHeight, hLayerButtons, layerID, szButtonClass, msgText);
}

extern "C" __declspec(dllexport) int GetLayer() {
    return TGetLayer();
}

extern "C" __declspec(dllexport) void SetLayer(int index) {
    TSetLayer(index);
}

extern "C" __declspec(dllexport) void ReorderLayerUp() {
    TReorderLayerUp();
}

extern "C" __declspec(dllexport) void ReorderLayerDown() {
    TReorderLayerDown();
}

void RenderActionToTarget(const ACTION& action, ID2D1RenderTarget* target) {
    TRenderActionToTarget(action, target);
}

void UpdateLayers(int layerIndexTarget = -1) {
    TUpdateLayers(layerIndexTarget);
}

extern "C" __declspec(dllexport) void RenderLayers() {
    TRenderLayers();
}

extern "C" __declspec(dllexport) void DrawLayerPreview(int currentLayer) {
    TDrawLayerPreview(currentLayer);
}

/* TOOLS */

extern "C" __declspec(dllexport) void SetSelectedTool(int pselectedTool) {
    HSetSelectedTool(pselectedTool);
}

extern "C" __declspec(dllexport) void EraserTool(int left, int top) {
    TEraserTool(left, top);
}

extern "C" __declspec(dllexport) void BrushTool(int left, int top, COLORREF hexColor, bool pixelMode, int pPixelSizeRatio) {
    TBrushTool(left, top, hexColor, pixelMode, pPixelSizeRatio);
}

extern "C" __declspec(dllexport) void RectangleTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    TRectangleTool(left, top, right, bottom, hexColor);
}

extern "C" __declspec(dllexport) void EllipseTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    TEllipseTool(left, top, right, bottom, hexColor);
}

extern "C" __declspec(dllexport) void LineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor) {
    TLineTool(xInitial, yInitial, x, y, hexColor);
}

extern "C" __declspec(dllexport) void PaintBucketTool(int mouseX, int mouseY, COLORREF fillColor, HWND hWnd) {
    TPaintBucketTool(mouseX, mouseY, fillColor, hWnd);
}

extern "C" __declspec(dllexport) void __stdcall SelectTool(int xInitial, int yInitial) {
    TSelectTool(xInitial, yInitial);
}

extern "C" __declspec(dllexport) void __stdcall MoveTool(int xInitial, int yInitial, int x, int y) {
    TMoveTool(xInitial, yInitial, x, y);
}

extern "C" __declspec(dllexport) void __stdcall UnSelectTool() {
    TUnSelectTool();
}

extern "C" __declspec(dllexport) void Cleanup() {
    HCleanup();
}