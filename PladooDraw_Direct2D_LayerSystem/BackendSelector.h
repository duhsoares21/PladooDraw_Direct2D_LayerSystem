#pragma once
#include "GraphicsBackend.h"

std::unique_ptr<IGraphicsBackend> CreateGraphicsBackend();
