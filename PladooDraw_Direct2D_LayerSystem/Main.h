#pragma once
#include "Base.h"
#include "Structs.h"

extern "C" __declspec(dllexport) void __stdcall Resize();

extern "C" __declspec(dllexport) HRESULT Initialize(HWND pmainHWND, HWND pdocHWND, int pWidth, int pHeight, int pPixelSizeRatio);
extern "C" __declspec(dllexport) HRESULT InitializeDocument(HWND hWnd, int pWidth, int pHeight, int pPixelSizeRatio);
extern "C" __declspec(dllexport) HRESULT InitializeLayerRenderPreview();
extern "C" __declspec(dllexport) HRESULT InitializeLayersButtons(HWND* buttonsHwnd);
extern "C" __declspec(dllexport) HRESULT InitializeLayers(HWND hWnd);

extern "C" __declspec(dllexport) void SetSelectedTool(int pselectedTool);
extern "C" __declspec(dllexport) void __stdcall ExportSVG();
extern "C" __declspec(dllexport) void __stdcall SetFont();

extern "C" __declspec(dllexport) void __stdcall SaveProjectDll(const char* pathAnsi);
extern "C" __declspec(dllexport) void __stdcall LoadProjectDll(LPCSTR apath);
extern "C" __declspec(dllexport) void __stdcall LoadProjectDllW(LPWSTR lppath);

extern "C" __declspec(dllexport) void CreateLogData(std::string fileName, std::string content);

extern "C" __declspec(dllexport) void handleMouseUp();
extern "C" __declspec(dllexport) void Undo();
extern "C" __declspec(dllexport) void Redo();
extern "C" __declspec(dllexport) float __stdcall GetZoomFactor();
extern "C" __declspec(dllexport) void ZoomIn_Default();
extern "C" __declspec(dllexport) void ZoomOut_Default();
extern "C" __declspec(dllexport) void ZoomIn(float zoomIncrement);
extern "C" __declspec(dllexport) void ZoomOut(float zoomIncrement);
extern "C" __declspec(dllexport) void Zoom();
extern "C" __declspec(dllexport) void IncreaseBrushSize_Default();
extern "C" __declspec(dllexport) void DecreaseBrushSize_Default();
extern "C" __declspec(dllexport) void IncreaseBrushSize(float sizeIncrement);
extern "C" __declspec(dllexport) void DecreaseBrushSize(float sizeIncrement);

extern "C" __declspec(dllexport) void ReorderLayers(int isAddingLayer);
extern "C" __declspec(dllexport) void AddLayerButton(int LayerButtonID);
extern "C" __declspec(dllexport) void RemoveLayerButton();
extern "C" __declspec(dllexport) void AddLayer(bool fromFile, int currentLayer);
extern "C" __declspec(dllexport) HRESULT RemoveLayer();
extern "C" __declspec(dllexport) int GetLayer();
extern "C" __declspec(dllexport) void SetLayer(int index);
extern "C" __declspec(dllexport) int __stdcall IsLayerActive(int layer, int* isActive);
extern "C" __declspec(dllexport) int GetActiveLayersCount();
extern "C" __declspec(dllexport) void ReorderLayerUp();
extern "C" __declspec(dllexport) void ReorderLayerDown();
extern "C" __declspec(dllexport) int LayersCount();
extern "C" __declspec(dllexport) void UpdateLayers(int layerIndexTarget);
extern "C" __declspec(dllexport) void RenderLayers();
extern "C" __declspec(dllexport) void DrawLayerPreview(int currentLayer);
extern "C" __declspec(dllexport) void __stdcall SelectTool(int xInitial, int yInitial);
extern "C" __declspec(dllexport) void __stdcall UnSelectTool();
extern "C" __declspec(dllexport) void __stdcall MoveTool(int xInitial, int yInitial, int x, int y);
extern "C" __declspec(dllexport) void EraserTool(int left, int top);
extern "C" __declspec(dllexport) void BrushTool(int left, int top, COLORREF hexColor, bool pixelMode, int pPixelSizeRatio);
extern "C" __declspec(dllexport) void RectangleTool(int left, int top, int right, int bottom, unsigned int hexColor);
extern "C" __declspec(dllexport) void EllipseTool(int left, int top, int right, int bottom, unsigned int hexColor);
extern "C" __declspec(dllexport) void LineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor);
extern "C" __declspec(dllexport) void PaintBucketTool(int xInitial, int yInitial, COLORREF hexFillColor, HWND hWnd);
extern "C" __declspec(dllexport) void WriteTool(int x, int y);
extern "C" __declspec(dllexport) void Cleanup();

extern "C" __declspec(dllexport) void OnScrollWheelLayers(int wParam);