#pragma once

extern void HSetSelectedTool(int pselectedTool);
extern void HCreateLogData(std::string fileName, std::string content);
extern D2D1_COLOR_F HGetRGBColor(COLORREF color);
extern RECT HScalePointsToButton(int x, int y, int drawingAreaWidth, int drawingAreaHeight, int buttonWidth, int buttonHeight, bool pixelMode, int pPixelSizeRatio);
extern void HCleanup();