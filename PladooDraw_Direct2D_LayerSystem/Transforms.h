#pragma once

extern float TGetZoomFactor();
extern void TSetZoomFactor(int pZoomFactor);
extern void TZoomIn(float zoomRatio);
extern void TZoomOut(float zoomRatio);
extern void TZoom();
extern void TIncreaseBrushSize_Default();
extern void TDecreaseBrushSize_Default();
extern void TIncreaseBrushSize(float sizeIncrement);
extern void TDecreaseBrushSize(float sizeIncrement);
