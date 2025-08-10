#pragma once

enum Tools
{
	TEraser,
	TBrush,
	TRectangle,
	TEllipse,
	TLine,
	TPaintBucket,
	TMove
};

extern void TEraserTool(int left, int top);
extern void TBrushTool(int left, int top, COLORREF hexColor, bool pixelMode, int pPixelSizeRatio);
extern void TRectangleTool(int left, int top, int right, int bottom, unsigned int hexColor);
extern void TEllipseTool(int left, int top, int right, int bottom, unsigned int hexColor);
extern void TLineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor);
extern void TPaintBucketTool(int mouseX, int mouseY, COLORREF fillColor, HWND hWnd);
extern void __stdcall TSelectTool(int xInitial, int yInitial);
extern void __stdcall TMoveTool(int xInitial, int yInitial, int x, int y);
extern void __stdcall TUnSelectTool();