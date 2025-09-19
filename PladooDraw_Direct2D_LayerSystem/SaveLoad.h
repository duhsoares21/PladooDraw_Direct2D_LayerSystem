#pragma once
#include "Base.h"

extern void SaveBinaryProject(const std::wstring& filename);
extern void LoadBinaryProject(const std::wstring& filename, HWND hWndLayer, HINSTANCE hLayerInstance, int btnWidth, int btnHeight, HWND* hLayerButtons, int layerID, const wchar_t* szButtonClass, const wchar_t* msgText);