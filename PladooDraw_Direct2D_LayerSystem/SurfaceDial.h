// SurfaceDial.h
#pragma once

#include <windows.h>

extern void TInitializeSurfaceDial(HWND docHWND);

void CleanupSurfaceDial();

void OnDialRotation(int menuIndex, int deltaTicks, float rotationDegrees);

void OnDialButtonClick(int menuIndex);