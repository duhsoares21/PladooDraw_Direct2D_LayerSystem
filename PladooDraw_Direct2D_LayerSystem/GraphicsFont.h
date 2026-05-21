#pragma once
#include "CoreBase.h"

struct FontDesc {
    std::wstring family;
    int size = 0;
    int weight = 0;
    bool italic = false;
    bool underline = false;
    bool strike = false;
};
