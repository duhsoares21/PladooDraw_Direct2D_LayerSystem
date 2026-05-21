#include "pch.h"
#include "Direct2DBackend.h"

#include "Constants.h"
#include "Direct2DBackendState.h"

namespace {
    D2D1_COLOR_F ToD2DColor(const ColorRGBA& color) {
        return D2D1::ColorF(color.r, color.g, color.b, color.a);
    }

    D2D1_RECT_F ToD2DRect(const RectF& rect) {
        return D2D1::RectF(rect.left, rect.top, rect.right, rect.bottom);
    }

    D2D1_ELLIPSE ToD2DEllipse(const EllipseF& ellipse) {
        return D2D1::Ellipse(
            D2D1::Point2F(ellipse.point.x, ellipse.point.y),
            ellipse.radiusX,
            ellipse.radiusY
        );
    }

    D2D1_MATRIX_3X2_F ToD2DMatrix(const Matrix3x2& transform) {
        return D2D1::Matrix3x2F(
            transform.m11,
            transform.m12,
            transform.m21,
            transform.m22,
            transform.dx,
            transform.dy
        );
    }

    bool IsRoInitializeSuccessful(HRESULT hr) {
        return hr == S_OK || hr == S_FALSE;
    }

    HRESULT CreateTextFormatForFont(const FontDesc& font, IDWriteTextFormat** textFormat) {
        if (!pDWriteFactory || !textFormat) {
            return E_INVALIDARG;
        }

        return pDWriteFactory->CreateTextFormat(
            font.family.empty() ? L"Arial" : font.family.c_str(),
            nullptr,
            static_cast<DWRITE_FONT_WEIGHT>(font.weight == 0 ? DWRITE_FONT_WEIGHT_NORMAL : font.weight),
            font.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            static_cast<FLOAT>(font.size <= 0 ? 12 : font.size) / 10.0f,
            L"en-us",
            textFormat
        );
    }

    HRESULT CreateDeviceContext(Microsoft::WRL::ComPtr<ID2D1DeviceContext>& deviceContext) {
        if (!g_pD2DDevice) {
            return E_FAIL;
        }

        return g_pD2DDevice->CreateDeviceContext(
            D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
            deviceContext.GetAddressOf()
        );
    }

    HRESULT CreateSwapChainTargetBitmap(
        ID2D1DeviceContext* deviceContext,
        IDXGISwapChain1* swapChain,
        D2D1_ALPHA_MODE alphaMode,
        ID2D1Bitmap1** targetBitmap
    ) {
        if (!deviceContext || !swapChain || !targetBitmap) {
            return E_INVALIDARG;
        }

        Microsoft::WRL::ComPtr<IDXGISurface> backBuffer;
        HRESULT hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (FAILED(hr)) {
            return hr;
        }

        FLOAT dpiX = 96.0f;
        FLOAT dpiY = 96.0f;
        deviceContext->GetDpi(&dpiX, &dpiY);

        D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, alphaMode),
            dpiX,
            dpiY
        );

        return deviceContext->CreateBitmapFromDxgiSurface(
            backBuffer.Get(),
            &bitmapProperties,
            targetBitmap
        );
    }
}

Direct2DBitmapSurface::Direct2DBitmapSurface(SizeU surfaceSize, Microsoft::WRL::ComPtr<ID2D1Bitmap1> surfaceBitmap)
    : size_(surfaceSize), bitmap_(std::move(surfaceBitmap)) {
}

SizeU Direct2DBitmapSurface::GetSize() const {
    return size_;
}

ID2D1Bitmap1* Direct2DBitmapSurface::GetBitmap() const {
    return bitmap_.Get();
}

Direct2DRenderSurface::Direct2DRenderSurface(
    SizeU surfaceSize,
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> surfaceContext,
    Microsoft::WRL::ComPtr<IDXGISwapChain1> surfaceSwapChain,
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap
)
    : size_(surfaceSize),
      context_(std::move(surfaceContext)),
      swapChain_(std::move(surfaceSwapChain)),
      targetBitmap_(std::move(targetBitmap)) {
    if (context_ && targetBitmap_) {
        context_->SetTarget(targetBitmap_.Get());
    }
}

SizeU Direct2DRenderSurface::GetSize() const {
    return size_;
}

void Direct2DRenderSurface::BeginDraw() {
    if (!context_) return;
    if (targetBitmap_) {
        context_->SetTarget(targetBitmap_.Get());
    }
    context_->BeginDraw();
}

HRESULT Direct2DRenderSurface::EndDraw() {
    if (!context_) return E_FAIL;
    return context_->EndDraw();
}

void Direct2DRenderSurface::Clear(const ColorRGBA& color) {
    if (!context_) return;
    context_->Clear(ToD2DColor(color));
}

void Direct2DRenderSurface::SetTransform(const Matrix3x2& transform) {
    if (!context_) return;
    context_->SetTransform(ToD2DMatrix(transform));
}

void Direct2DRenderSurface::PushClip(const RectF& clipRect) {
    if (!context_) return;
    context_->PushAxisAlignedClip(ToD2DRect(clipRect), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
}

void Direct2DRenderSurface::PopClip() {
    if (!context_) return;
    context_->PopAxisAlignedClip();
}

void Direct2DRenderSurface::DrawBitmap(IBitmapSurface& bitmap, const RectF* destination, float opacity) {
    if (!context_) return;
    auto* d2dBitmap = dynamic_cast<Direct2DBitmapSurface*>(&bitmap);
    if (!d2dBitmap) return;

    if (destination) {
        context_->DrawBitmap(d2dBitmap->GetBitmap(), ToD2DRect(*destination), opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        return;
    }

    context_->DrawBitmap(d2dBitmap->GetBitmap(), nullptr, opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

void Direct2DRenderSurface::FillRect(const RectF& rect, const ColorRGBA& color) {
    if (!context_) return;
    auto brush = CreateBrush(color);
    context_->FillRectangle(ToD2DRect(rect), brush.Get());
}

void Direct2DRenderSurface::StrokeRect(const RectF& rect, const ColorRGBA& color, float strokeWidth) {
    if (!context_) return;
    auto brush = CreateBrush(color);
    context_->DrawRectangle(ToD2DRect(rect), brush.Get(), strokeWidth);
}

void Direct2DRenderSurface::FillEllipse(const EllipseF& ellipse, const ColorRGBA& color) {
    if (!context_) return;
    auto brush = CreateBrush(color);
    context_->FillEllipse(ToD2DEllipse(ellipse), brush.Get());
}

void Direct2DRenderSurface::StrokeEllipse(const EllipseF& ellipse, const ColorRGBA& color, float strokeWidth) {
    if (!context_) return;
    auto brush = CreateBrush(color);
    context_->DrawEllipse(ToD2DEllipse(ellipse), brush.Get(), strokeWidth);
}

void Direct2DRenderSurface::DrawLine(const PointF& start, const PointF& end, const ColorRGBA& color, float strokeWidth) {
    if (!context_) return;
    auto brush = CreateBrush(color);
    context_->DrawLine(
        D2D1::Point2F(start.x, start.y),
        D2D1::Point2F(end.x, end.y),
        brush.Get(),
        strokeWidth,
        nullptr
    );
}

void Direct2DRenderSurface::DrawText(const std::wstring& text, const FontDesc& font, const RectF& layoutRect, const ColorRGBA& color) {
    if (!context_ || !pDWriteFactory || text.empty()) return;

    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    HRESULT hr = CreateTextFormatForFont(font, format.GetAddressOf());
    if (FAILED(hr)) return;

    auto brush = CreateBrush(color);
    context_->DrawTextW(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        format.Get(),
        ToD2DRect(layoutRect),
        brush.Get()
    );
}

void Direct2DRenderSurface::SetPrimitiveBlendCopy(bool enabled) {
    if (!context_) return;
    context_->SetPrimitiveBlend(enabled ? D2D1_PRIMITIVE_BLEND_COPY : D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
}

void Direct2DRenderSurface::SetAliased(bool enabled) {
    if (!context_) return;
    context_->SetAntialiasMode(enabled ? D2D1_ANTIALIAS_MODE_ALIASED : D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
}

void Direct2DRenderSurface::Present(UINT syncInterval) {
    if (swapChain_) {
        swapChain_->Present(syncInterval, 0);
    }
}

Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> Direct2DRenderSurface::CreateBrush(const ColorRGBA& color) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    if (context_) {
        context_->CreateSolidColorBrush(ToD2DColor(color), &brush);
    }
    return brush;
}

HRESULT Direct2DBackend::InitializeMainWindow(HWND mainWindow) {
    HRESULT hr = RoInitialize(RO_INIT_SINGLETHREADED);
    if (!IsRoInitializeSuccessful(hr)) {
        MessageBox(nullptr, L"Failed to initialize Windows Runtime", L"Error", MB_OK | MB_ICONERROR);
        return hr;
    }

    if (pD2DFactory) {
        return S_OK;
    }

    D2D1_FACTORY_OPTIONS options = { D2D1_DEBUG_LEVEL_INFORMATION };
    hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_MULTI_THREADED,
        __uuidof(ID2D1Factory1),
        &options,
        reinterpret_cast<void**>(pD2DFactory.GetAddressOf())
    );
    if (FAILED(hr)) {
        MessageBox(mainWindow, L"Erro ao criar Factory", L"Erro", MB_OK);
    }

    return hr;
}

HRESULT Direct2DBackend::InitializeDocument(HWND documentWindow, int, int, int, int, int) {
    if (!IsWindow(documentWindow)) {
        return E_INVALIDARG;
    }

    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags,
        featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
        &g_pD3DDevice, nullptr, nullptr
    );

    if (FAILED(hr)) {
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, creationFlags,
            featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION,
            &g_pD3DDevice, nullptr, nullptr
        );
        if (FAILED(hr)) {
            MessageBox(documentWindow, L"Failed to create D3D11 device", L"Error", MB_OK);
            return hr;
        }
    }

    hr = CreateDeviceResources();
    if (FAILED(hr)) {
        return hr;
    }

    DXGI_SWAP_CHAIN_DESC1 swapDesc = MakeSwapChainDescription(width, height);
    hr = g_dxgiFactory->CreateSwapChainForHwnd(g_pD3DDevice.Get(), documentWindow, &swapDesc, nullptr, nullptr, &g_pSwapChain);
    if (FAILED(hr)) {
        return hr;
    }

    hr = g_dxgiFactory->MakeWindowAssociation(documentWindow, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(hr)) {
        return hr;
    }

    hr = RebuildDocumentTargetBitmap();
    if (FAILED(hr)) {
        return hr;
    }

    RefreshDocumentSurface();
    return S_OK;
}

HRESULT Direct2DBackend::InitializeText() {
    if (pDWriteFactory) {
        return S_OK;
    }

    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(pDWriteFactory.GetAddressOf())
    );
    if (FAILED(hr)) {
        MessageBox(nullptr, L"Failed to create DirectWrite factory", L"Error", MB_OK);
    }

    return hr;
}

void Direct2DBackend::ResizeDocument(int documentWidth, int documentHeight, float documentZoomFactor) {
    if (!g_pSwapChain || !pRenderTarget) {
        return;
    }

    pRenderTarget->SetTarget(nullptr);
    pD2DTargetBitmap.Reset();

    HRESULT hr = g_pSwapChain->ResizeBuffers(
        0,
        static_cast<UINT>(documentWidth * documentZoomFactor),
        static_cast<UINT>(documentHeight * documentZoomFactor),
        DXGI_FORMAT_B8G8R8A8_UNORM,
        0
    );
    if (FAILED(hr)) {
        return;
    }

    if (FAILED(RebuildDocumentTargetBitmap())) {
        return;
    }

    RefreshDocumentSurface();
}

RenderData Direct2DBackend::CreateWindowRenderData(HWND windowHandle) {
    RECT rc{};
    GetClientRect(windowHandle, &rc);
    SizeU size{
        static_cast<uint32_t>(rc.right - rc.left),
        static_cast<uint32_t>(rc.bottom - rc.top)
    };

    Microsoft::WRL::ComPtr<ID2D1DeviceContext> deviceContext;
    HRESULT hr = CreateDeviceContext(deviceContext);
    if (FAILED(hr)) {
        return {};
    }

    DXGI_SWAP_CHAIN_DESC1 swapDesc = MakeSwapChainDescription(static_cast<int>(size.width), static_cast<int>(size.height));

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    hr = g_dxgiFactory->CreateSwapChainForHwnd(
        g_pD3DDevice.Get(),
        windowHandle,
        &swapDesc,
        nullptr,
        nullptr,
        &swapChain
    );
    if (FAILED(hr)) {
        return {};
    }

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
    hr = CreateSwapChainTargetBitmap(
        deviceContext.Get(),
        swapChain.Get(),
        D2D1_ALPHA_MODE_IGNORE,
        targetBitmap.GetAddressOf()
    );
    if (FAILED(hr)) {
        return {};
    }

    RenderData renderData;
    renderData.size = size;
    renderData.surfaceHandle = std::make_shared<Direct2DRenderSurface>(size, deviceContext, swapChain, targetBitmap);
    renderData.bitmapHandle = std::make_shared<Direct2DBitmapSurface>(size, targetBitmap);
    return renderData;
}

RenderData Direct2DBackend::CreateBitmapRenderData(const SizeU& size) {
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> deviceContext;
    HRESULT hr = CreateDeviceContext(deviceContext);
    if (FAILED(hr)) {
        return {};
    }

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap;
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    hr = deviceContext->CreateBitmap(D2D1::SizeU(size.width, size.height), nullptr, 0, &bitmapProperties, &targetBitmap);
    if (FAILED(hr)) {
        return {};
    }

    RenderData renderData;
    renderData.size = size;
    renderData.surfaceHandle = std::make_shared<Direct2DRenderSurface>(size, deviceContext, nullptr, targetBitmap);
    renderData.bitmapHandle = std::make_shared<Direct2DBitmapSurface>(size, targetBitmap);
    return renderData;
}

RenderSurfacePtr Direct2DBackend::GetDocumentSurface() {
    if (!documentSurface_ && pRenderTarget && pD2DTargetBitmap) {
        RefreshDocumentSurface();
    }
    return documentSurface_;
}

std::vector<COLORREF> Direct2DBackend::ReadDocumentPixels(int logicalWidth, int logicalHeight) {
    if (logicalWidth <= 0 || logicalHeight <= 0) {
        return {};
    }

    D2D1_SIZE_U size = D2D1::SizeU(static_cast<UINT32>(logicalWidth), static_cast<UINT32>(logicalHeight));
    D2D1_BITMAP_PROPERTIES1 renderBitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f
    );

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> renderBitmap;
    HRESULT hr = pRenderTarget->CreateBitmap(size, nullptr, 0, &renderBitmapProps, &renderBitmap);
    if (FAILED(hr)) {
        return {};
    }

    Microsoft::WRL::ComPtr<ID2D1DeviceContext> captureContext;
    hr = CreateDeviceContext(captureContext);
    if (FAILED(hr)) {
        return {};
    }

    captureContext->SetTarget(renderBitmap.Get());
    captureContext->BeginDraw();
    captureContext->Clear(D2D1::ColorF(D2D1::ColorF::White, 1.0f));

    D2D1_RECT_F destRect = D2D1::RectF(0, 0, static_cast<FLOAT>(logicalWidth), static_cast<FLOAT>(logicalHeight));

    auto it = std::find_if(
        layers.begin(),
        layers.end(),
        [](const std::optional<Layer>& optLayer) {
            return optLayer.has_value() && optLayer->LayerID == layerIndex && optLayer->FrameIndex == CurrentFrameIndex;
        }
    );

    if (it != layers.end() && it->has_value()) {
        auto* d2dBitmap = dynamic_cast<Direct2DBitmapSurface*>(it->value().bitmapHandle.get());
        if (d2dBitmap) {
            captureContext->DrawBitmap(d2dBitmap->GetBitmap(), destRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        }
    }

    hr = captureContext->EndDraw();
    if (FAILED(hr)) {
        return {};
    }

    D2D1_BITMAP_PROPERTIES1 cpuBitmapProps = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_CPU_READ | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f
    );

    Microsoft::WRL::ComPtr<ID2D1Bitmap1> cpuBitmap;
    hr = pRenderTarget->CreateBitmap(size, nullptr, 0, &cpuBitmapProps, &cpuBitmap);
    if (FAILED(hr)) {
        return {};
    }

    hr = cpuBitmap->CopyFromBitmap(nullptr, renderBitmap.Get(), nullptr);
    if (FAILED(hr)) {
        return {};
    }

    D2D1_MAPPED_RECT mappedRect;
    hr = cpuBitmap->Map(D2D1_MAP_OPTIONS_READ, &mappedRect);
    if (FAILED(hr)) {
        return {};
    }

    std::vector<COLORREF> pixels(static_cast<size_t>(logicalWidth) * static_cast<size_t>(logicalHeight));
    BYTE* src = mappedRect.bits;
    for (size_t i = 0; i < pixels.size(); ++i) {
        BYTE b = src[i * 4 + 0];
        BYTE g = src[i * 4 + 1];
        BYTE r = src[i * 4 + 2];
        pixels[i] = RGB(r, g, b);
    }

    cpuBitmap->Unmap();
    return pixels;
}

SizeF Direct2DBackend::MeasureText(const std::wstring& text, const FontDesc& font, const RectF& layoutRect) {
    if (!pDWriteFactory || text.empty()) {
        return {};
    }

    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    HRESULT hr = CreateTextFormatForFont(font, format.GetAddressOf());
    if (FAILED(hr)) {
        return {};
    }

    Microsoft::WRL::ComPtr<IDWriteTextLayout> textLayout;
    hr = pDWriteFactory->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        format.Get(),
        layoutRect.right - layoutRect.left,
        layoutRect.bottom - layoutRect.top,
        &textLayout
    );
    if (FAILED(hr)) {
        return {};
    }

    if (font.underline) {
        textLayout->SetUnderline(TRUE, DWRITE_TEXT_RANGE{ 0, static_cast<UINT32>(text.size()) });
    }
    if (font.strike) {
        textLayout->SetStrikethrough(TRUE, DWRITE_TEXT_RANGE{ 0, static_cast<UINT32>(text.size()) });
    }

    DWRITE_TEXT_METRICS metrics{};
    hr = textLayout->GetMetrics(&metrics);
    if (FAILED(hr)) {
        return {};
    }

    return SizeF{ metrics.width, metrics.height };
}

void Direct2DBackend::PresentDocument(UINT syncInterval) {
    if (g_pSwapChain) {
        g_pSwapChain->Present(syncInterval, 0);
    }
}

void Direct2DBackend::Cleanup() {
    documentSurface_.reset();
    pRenderTarget.Reset();
    pD2DFactory.Reset();
    pD2DTargetBitmap.Reset();
    pDWriteFactory.Reset();
    g_pSwapChain.Reset();
    g_pD2DDevice.Reset();
    g_pD3DDevice.Reset();
    g_dxgiFactory.Reset();
}

HRESULT Direct2DBackend::CreateDeviceResources() {
    Microsoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice;
    HRESULT hr = g_pD3DDevice.As(&dxgiDevice);
    if (FAILED(hr)) {
        return hr;
    }

    hr = pD2DFactory->CreateDevice(dxgiDevice.Get(), &g_pD2DDevice);
    if (FAILED(hr)) {
        return hr;
    }

    hr = CreateDeviceContext(pRenderTarget);
    if (FAILED(hr)) {
        return hr;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr)) {
        return hr;
    }

    return adapter->GetParent(IID_PPV_ARGS(&g_dxgiFactory));
}

HRESULT Direct2DBackend::RebuildDocumentTargetBitmap() {
    HRESULT hr = CreateSwapChainTargetBitmap(
        pRenderTarget.Get(),
        g_pSwapChain.Get(),
        D2D1_ALPHA_MODE_PREMULTIPLIED,
        pD2DTargetBitmap.ReleaseAndGetAddressOf()
    );
    if (FAILED(hr)) {
        return hr;
    }

    pRenderTarget->SetTarget(pD2DTargetBitmap.Get());
    return S_OK;
}

void Direct2DBackend::RefreshDocumentSurface() {
    if (!pRenderTarget || !pD2DTargetBitmap) {
        documentSurface_.reset();
        return;
    }

    SizeU documentSize{
        static_cast<uint32_t>(width > 0 ? width : 0),
        static_cast<uint32_t>(height > 0 ? height : 0)
    };
    documentSurface_ = std::make_shared<Direct2DRenderSurface>(documentSize, pRenderTarget, g_pSwapChain, pD2DTargetBitmap);
}

DXGI_SWAP_CHAIN_DESC1 Direct2DBackend::MakeSwapChainDescription(int width, int height) {
    DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
    swapDesc.Width = width;
    swapDesc.Height = height;
    swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 2;
    swapDesc.Scaling = DXGI_SCALING_STRETCH;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapDesc.Flags = 0;
    return swapDesc;
}
