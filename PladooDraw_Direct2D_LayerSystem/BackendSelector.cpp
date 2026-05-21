#include "pch.h"
#include "BackendSelector.h"
#include "Direct2DBackend.h"

std::unique_ptr<IGraphicsBackend> CreateGraphicsBackend() {
   return std::make_unique<Direct2DBackend>();
}
