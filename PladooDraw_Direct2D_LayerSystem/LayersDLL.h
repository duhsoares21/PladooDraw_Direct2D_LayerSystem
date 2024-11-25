#pragma once
#include <windows.h>
#include <d2d1.h>

extern "C" __declspec(dllexport) HRESULT Initialize(HWND hWnd);
extern "C" __declspec(dllexport) void ResetParameters();
extern "C" __declspec(dllexport) HRESULT AddLayer();
extern "C" __declspec(dllexport) void SetLayer(int index);
extern "C" __declspec(dllexport) void RenderLayers();
extern "C" __declspec(dllexport) int LayersCount();
extern "C" __declspec(dllexport) void EllipseTool(HDC hdc, int left, int top, int right, int bottom, unsigned int hexColor);
extern "C" __declspec(dllexport) void RectangleTool(HDC hdc, int left, int top, int right, int bottom, unsigned int hexColor);
extern "C" __declspec(dllexport) void BrushTool(HDC hdc, int left, int top, unsigned int hexColor, int brushSize);
extern "C" __declspec(dllexport) void EraserTool(HDC hdc, int left, int top, int brushSize);
extern "C" __declspec(dllexport) void Cleanup();
template <class T> void SafeRelease(T** ppT);