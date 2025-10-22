#pragma once
#include "Base.h"
#include "Constants.h"
#include "Helpers.h"

extern void TSetAnimationFrame(int AnimationFrame);
extern void TCreateAnimationFrame(int LayerIndex, int FrameIndex);
extern void TReorganizeFrames();
extern void TRemoveAnimationFrame();
extern void TUpdateAnimation();
extern void TRenderAnimation();

extern void TAnimationForward();
extern void TAnimationBackward();

extern void TPlayAnimation();
extern void TPauseAnimation();