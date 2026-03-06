#pragma once
#include "Animation.h"
#include "Base.h"
#include "Constants.h"
#include "Helpers.h"
#include "Layers.h"
#include "Render.h"
#include "Replay.h"
#include "Structs.h"
#include "Tools.h"

extern void THandleMouseUp();
extern void TUpdatePaint(float deltaX, float deltaY);
extern void TUndo();
extern void TRedo();
extern DELTA CalculateMovementDelta(int x, int y, ACTION* targetAction, ACTION* selected);
extern bool HitTestAction(const ACTION& action, float x, float y);
extern std::vector<COLORREF> CaptureCanvasPixels();
extern void AuxCopyAction();
extern void AuxPasteAction();
extern void AuxDeleteAction();
