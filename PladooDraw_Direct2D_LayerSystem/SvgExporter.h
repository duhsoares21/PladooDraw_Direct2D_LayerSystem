#pragma once

#include <string>
#include <vector>
#include "Structs.h"

// Função para converter COLORREF para string hexadecimal SVG
std::string ColorRefToSvgHex(COLORREF color);

// Função para exportar Actions para SVG com upscaling
void ExportActionsToSvg(const std::vector<ACTION>& actions, const std::string& filename, float scaleFactor, int originalWidth, int originalHeight);


