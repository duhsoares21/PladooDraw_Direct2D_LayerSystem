#pragma once
#include "Base.h"

enum ActionType
{
	TAction,
	TRedoAction
};


extern void SaveBinaryProject(const std::wstring& filename);
extern void LoadBinaryProject(const std::wstring& filename);