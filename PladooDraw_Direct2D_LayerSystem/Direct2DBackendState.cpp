#include "pch.h"
#include "Direct2DBackendState.h"

Microsoft::WRL::ComPtr<ID2D1Bitmap1> pD2DTargetBitmap;
Microsoft::WRL::ComPtr<ID2D1Factory1> pD2DFactory;

Microsoft::WRL::ComPtr<ID2D1DeviceContext> pRenderTarget = nullptr;

Microsoft::WRL::ComPtr<ID3D11Device> g_pD3DDevice;
Microsoft::WRL::ComPtr<IDXGISwapChain1> g_pSwapChain;
Microsoft::WRL::ComPtr<ID2D1Device> g_pD2DDevice;
Microsoft::WRL::ComPtr<IDXGIFactory2> g_dxgiFactory;

Microsoft::WRL::ComPtr<IDWriteFactory> pDWriteFactory;
