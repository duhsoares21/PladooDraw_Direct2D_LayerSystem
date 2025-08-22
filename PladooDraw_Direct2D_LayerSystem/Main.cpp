// LayersDLL.cpp
#include "pch.h"
#include "Constants.h"
#include "Helpers.h"
#include "Main.h"
#include "Layers.h"
#include "Render.h"
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

HRESULT Initialize(HWND pmainHWND, HWND pdocHWND, int pWidth, int pHeight, int pPixelSizeRatio) {
    return TInitialize(pmainHWND, pdocHWND, pWidth, pHeight, pPixelSizeRatio);
}

HRESULT InitializeDocument(HWND hWnd, int pWidth, int pHeight, int pPixelSizeRatio) {
    return TInitializeDocument(hWnd, pWidth, pHeight, pPixelSizeRatio);
}

HRESULT InitializeLayerRenderPreview() {
    return TInitializeLayerRenderPreview();
}

HRESULT InitializeLayers(HWND hWnd) {
    return TInitializeLayers(hWnd);
}

/* HELPERS */

void CreateLogData(std::string fileName, std::string content) {
    HCreateLogData(fileName, content);
}

void __stdcall SaveProjectDll(const char* pathAnsi) {
    int wlen = MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, nullptr, 0);
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_ACP, 0, pathAnsi, -1, &wpath[0], wlen);

    SaveBinaryProject(wpath);
}

void __stdcall LoadProjectDll(LPCWSTR wpath, HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int* layerID, const wchar_t* szButtonClass, const wchar_t* msgText) {
    LoadBinaryProject(wpath, hWndLayer, hLayerInstance, btnWidth, btnHeight, hLayerButtons, *layerID, L"Button", msgText);
}

/* ACTIONS */

void handleMouseUp() {
    THandleMouseUp();
}

void Undo() {
    TUndo();
}

void Redo() {
    TRedo();
}

/* TRANSFORM */

float __stdcall GetZoomFactor() {
    return TGetZoomFactor();
}

void ZoomIn_Default() {
    TZoomIn(0.5f);
}

void ZoomOut_Default() {
    TZoomOut(0.5f);
}

void ZoomIn(float zoomRatio) {
    TZoomIn(zoomRatio);
}

void ZoomOut(float zoomRatio) {
    TZoomOut(zoomRatio);
}

void Zoom() {
    TZoom();
}

void IncreaseBrushSize_Default() {
    TIncreaseBrushSize(0.5f);
}
void DecreaseBrushSize_Default() {
    TDecreaseBrushSize(0.5f);
}

void IncreaseBrushSize(float sizeRatio) {
    TIncreaseBrushSize(sizeRatio);
}
void DecreaseBrushSize(float sizeRatio) {
    TDecreaseBrushSize(sizeRatio);
}

/* LAYERS */

HRESULT AddLayer(bool fromFile=false) {
    return TAddLayer(fromFile);
}

int LayersCount() {
    return TLayersCount();
}

void AddLayerButton(HWND layerButton) {
    TAddLayerButton(layerButton);
}

HRESULT RemoveLayer() {
    return TRemoveLayer();
}

HRESULT __stdcall RecreateLayers(HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int& layerID, const wchar_t* szButtonClass, const wchar_t* msgText) {
    return TRecreateLayers(hWndLayer, hLayerInstance, btnWidth, btnHeight, hLayerButtons, layerID, szButtonClass, msgText);
}

int GetLayer() {
    return TGetLayer();
}

void SetLayer(int index) {
    TSetLayer(index);
}

void ReorderLayerUp() {
    TReorderLayerUp();
}

void ReorderLayerDown() {
    TReorderLayerDown();
}

void RenderActionToTarget(const ACTION& action, ID2D1RenderTarget* target) {
    TRenderActionToTarget(action, target);
}

void UpdateLayers(int layerIndexTarget = -1) {
    TUpdateLayers(layerIndexTarget);
}

void RenderLayers() {
    TRenderLayers();
}

void DrawLayerPreview(int currentLayer) {
    TDrawLayerPreview(currentLayer);
}

/* TOOLS */

void SetSelectedTool(int pselectedTool) {
    HSetSelectedTool(pselectedTool);
}

void EraserTool(int left, int top) {
    TEraserTool(left, top);
}

void BrushTool(int left, int top, COLORREF hexColor, bool pixelMode, int pPixelSizeRatio) {
    TBrushTool(left, top, hexColor, pixelMode, pPixelSizeRatio);
}

void RectangleTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    TRectangleTool(left, top, right, bottom, hexColor);
}

void EllipseTool(int left, int top, int right, int bottom, unsigned int hexColor) {
    TEllipseTool(left, top, right, bottom, hexColor);
}

void LineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor) {
    TLineTool(xInitial, yInitial, x, y, hexColor);
}

void PaintBucketTool(int mouseX, int mouseY, COLORREF fillColor, HWND hWnd) {
    TPaintBucketTool(mouseX, mouseY, fillColor, hWnd);
}

void __stdcall SelectTool(int xInitial, int yInitial) {
    TSelectTool(xInitial, yInitial);
}

void __stdcall MoveTool(int xInitial, int yInitial, int x, int y) {
    TMoveTool(xInitial, yInitial, x, y);
}

void __stdcall UnSelectTool() {
    TUnSelectTool();
}

void Cleanup() {
    HCleanup();
}