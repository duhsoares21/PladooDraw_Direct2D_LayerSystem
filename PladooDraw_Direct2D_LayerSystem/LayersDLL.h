#pragma once
#include <windows.h>
#include <d2d1.h>

extern "C" __declspec(dllexport) HRESULT Initialize(HWND hWnd);
extern "C" __declspec(dllexport) void handleMouseUp();
extern "C" __declspec(dllexport) HRESULT AddLayer();
extern "C" __declspec(dllexport) void SetLayer(int index);
extern "C" __declspec(dllexport) void RenderLayers();
extern "C" __declspec(dllexport) int LayersCount();
extern "C" __declspec(dllexport) void BrushTool(int left, int top, unsigned int hexColor, int brushSize);
extern "C" __declspec(dllexport) void RectangleTool(int left, int top, int right, int bottom, unsigned int hexColor);
extern "C" __declspec(dllexport) void EllipseTool(int left, int top, int right, int bottom, unsigned int hexColor);
extern "C" __declspec(dllexport) void LineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor, int brushSize);
extern "C" __declspec(dllexport) void PaintBucketTool(int xInitial, int yInitial, unsigned int hexFillColor);
extern "C" __declspec(dllexport) void EraserTool(int left, int top, int brushSize);
extern "C" __declspec(dllexport) void Cleanup();

ID2D1SolidColorBrush* D2_CreateSolidBrush(unsigned int hexColor);
template <class T> void SafeRelease(T** ppT);