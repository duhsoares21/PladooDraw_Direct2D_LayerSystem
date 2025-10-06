#pragma once

extern int HGetActiveLayersCount();
extern void HSetSelectedTool(int pselectedTool);
extern void HCreateLogData(std::string fileName, std::string content);
extern D2D1_COLOR_F HGetRGBColor(COLORREF color);
extern std::vector<std::string> HSplit(std::string s, std::string delimiter);
extern RECT HScalePointsToButton(int x, int y, int drawingAreaWidth, int drawingAreaHeight, int buttonWidth, int buttonHeight, bool pixelMode, int pPixelSizeRatio);
extern void HPrintHResultError(HRESULT hr);
extern void HCleanup();

extern void HOnScrollWheelLayers(int wParam);