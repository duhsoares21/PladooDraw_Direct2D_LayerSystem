#pragma once
#include "Base.h"
#include "Constants.h"
#include "pch.h"

extern void TRenderReplayAction(const ACTION& action, Microsoft::WRL::ComPtr<ID2D1DeviceContext> replayDeviceContext);
extern void TCreateReplayFrame(int FrameIndex);
extern void TSetReplayHighlight();
extern void TReplayRender();
extern void TEditFromThisPoint();