#pragma once
#include "CoreBase.h"
#include "Constants.h"
#include "pch.h"

extern void TReplayClearLayers();
extern void TCreateReplayFrame(int FrameIndex);
extern void TReplayRender();
extern void TEditFromThisPoint();
extern void TReplayBackwards();
extern void TReplayForward();
extern bool TIsReplayAtEnd();
