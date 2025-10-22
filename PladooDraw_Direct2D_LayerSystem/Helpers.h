#pragma once
#include "Structs.h"

extern int HGetActiveLayersCount();
extern int HGetMaxFrameIndex();
extern void HSetAnimationMode(int pAnimationMode);
extern void HSetReplayMode(int pReplayMode);
extern void HSetSelectedTool(int pselectedTool);
extern void HCreateLogData(std::string fileName, std::string content);
extern D2D1_COLOR_F HGetRGBColor(COLORREF color);
extern std::vector<std::string> HSplit(std::string s, std::string delimiter);
extern RECT HScalePointsToButton(int x, int y, int drawingAreaWidth, int drawingAreaHeight, int buttonWidth, int buttonHeight, bool pixelMode, int pPixelSizeRatio);
extern void HPrintHResultError(HRESULT hr);
extern void HCleanup();

extern void HRenderAction(const ACTION& action, Microsoft::WRL::ComPtr<ID2D1DeviceContext> deviceContext, D2D1_COLOR_F customColor);
extern RenderData HCreateRenderData(D2D1_SIZE_U size);
extern RenderData HCreateRenderDataHWND(HWND hWnd);
extern void CreateGlobalRenderBitmap();

extern void HOnScrollWheelLayers(int wParam);
extern void HOnScrollWheelTimeline(int wParam);
extern void HSetScrollPosition(int index);

extern void HSetTimelineHighlight();
extern HWND HCreateTimelineFrameButton(int FrameIndex);
extern void HCreateHighlightFrame();