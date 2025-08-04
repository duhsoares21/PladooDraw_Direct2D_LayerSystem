// SurfaceDial.h
#pragma once

#include <windows.h>

void InitializeSurfaceDial(HWND docHWND);

void CleanupSurfaceDial();

void OnDialRotation(int menuIndex, int deltaTicks, float rotationDegrees);

void OnDialButtonClick(int menuIndex);