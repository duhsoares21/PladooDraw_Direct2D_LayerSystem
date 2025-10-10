#pragma once
#include "Base.h"
#include "Structs.h"

extern void THandleMouseUp();
extern void TUndo();
extern void TRedo();
extern void TReplayBackwards();
extern void TReplayForward();
extern bool HitTestAction(const ACTION& action, float x, float y);
extern std::vector<COLORREF> CaptureCanvasPixels();
