#pragma once
#include "Structs.h"

extern int HGetActiveLayersCount();
extern int HGetMaxFrameIndex();
extern void HSetAnimationMode(int pAnimationMode);
extern int GetActionStepCount(const ACTION& a);
extern ACTION MakeStepAction(const ACTION& orig, int stepIndex);
extern void HSetReplayMode(int pReplayMode);
extern void HSetSelectedTool(int pselectedTool);
extern void HCreateLogData(std::string fileName, std::string content);
extern ColorRGBA HGetRGBAColor(COLORREF color);
extern std::vector<std::string> HSplit(std::string s, std::string delimiter);
extern RECT HScalePointsToButton(int x, int y, int drawingAreaWidth, int drawingAreaHeight, int buttonWidth, int buttonHeight, bool pixelMode, int pPixelSizeRatio);
extern void HPrintHResultError(HRESULT hr);
extern void HCleanup();

extern void HRenderAction(ACTION& action, IRenderSurface& renderSurface, std::optional<ColorRGBA> customColor = std::nullopt);
extern RenderData HCreateRenderData(const SizeU& size);
extern RenderData HCreateRenderDataHWND(HWND hWnd);

extern void HOnScrollWheelLayers(int wParam);
extern void HOnScrollWheelTimeline(int wParam);
extern void HSetScrollPosition(int index);

extern void HSetTimelineHighlight();
extern HWND HCreateTimelineFrameButton(int FrameIndex);
extern void HCreateHighlightFrame();
