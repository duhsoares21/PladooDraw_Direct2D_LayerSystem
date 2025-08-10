#pragma once

extern "C" float TGetZoomFactor();
extern "C" void TZoomIn_Default();
extern "C" void TZoomOut_Default();
extern "C" void TZoomIn(float zoomRatio);
extern "C" void TZoomOut(float zoomRatio);
extern "C" void TZoom();
extern "C" void TIncreaseBrushSize_Default();
extern "C" void TDecreaseBrushSize_Default();
extern "C" void TIncreaseBrushSize(float sizeIncrement);
extern "C" void TDecreaseBrushSize(float sizeIncrement);
