#pragma once

#include "Direct2DBase.h"

extern Microsoft::WRL::ComPtr<ID2D1Bitmap1> pD2DTargetBitmap;
extern Microsoft::WRL::ComPtr<ID2D1Factory1> pD2DFactory;
extern Microsoft::WRL::ComPtr<ID2D1DeviceContext> pRenderTarget;

extern Microsoft::WRL::ComPtr<ID3D11Device> g_pD3DDevice;
extern Microsoft::WRL::ComPtr<IDXGISwapChain1> g_pSwapChain;
extern Microsoft::WRL::ComPtr<ID2D1Device> g_pD2DDevice;
extern Microsoft::WRL::ComPtr<IDXGIFactory2> g_dxgiFactory;

extern Microsoft::WRL::ComPtr<IDWriteFactory> pDWriteFactory;
