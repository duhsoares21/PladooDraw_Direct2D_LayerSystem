#pragma once
#include "Direct2DBase.h"
#include "GraphicsBackend.h"

class Direct2DBitmapSurface final : public IBitmapSurface {
public:
    Direct2DBitmapSurface(SizeU surfaceSize, Microsoft::WRL::ComPtr<ID2D1Bitmap1> surfaceBitmap);

    SizeU GetSize() const override;
    ID2D1Bitmap1* GetBitmap() const;

private:
    SizeU size_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> bitmap_;
};

class Direct2DRenderSurface final : public IRenderSurface {
public:
    Direct2DRenderSurface(
        SizeU surfaceSize,
        Microsoft::WRL::ComPtr<ID2D1DeviceContext> surfaceContext,
        Microsoft::WRL::ComPtr<IDXGISwapChain1> surfaceSwapChain,
        Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap
    );

    SizeU GetSize() const override;
    void BeginDraw() override;
    HRESULT EndDraw() override;
    void Clear(const ColorRGBA& color) override;
    void SetTransform(const Matrix3x2& transform) override;
    void PushClip(const RectF& clipRect) override;
    void PopClip() override;
    void DrawBitmap(IBitmapSurface& bitmap, const RectF* destination, float opacity) override;
    void FillRect(const RectF& rect, const ColorRGBA& color) override;
    void StrokeRect(const RectF& rect, const ColorRGBA& color, float strokeWidth) override;
    void FillEllipse(const EllipseF& ellipse, const ColorRGBA& color) override;
    void StrokeEllipse(const EllipseF& ellipse, const ColorRGBA& color, float strokeWidth) override;
    void DrawLine(const PointF& start, const PointF& end, const ColorRGBA& color, float strokeWidth) override;
    void DrawText(const std::wstring& text, const FontDesc& font, const RectF& layoutRect, const ColorRGBA& color) override;
    void SetPrimitiveBlendCopy(bool enabled) override;
    void SetAliased(bool enabled) override;
    void Present(UINT syncInterval) override;

private:
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> CreateBrush(const ColorRGBA& color);

    SizeU size_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> targetBitmap_;
};

class Direct2DBackend final : public IGraphicsBackend {
public:
    HRESULT InitializeMainWindow(HWND mainWindow) override;
    HRESULT InitializeDocument(HWND documentWindow, int width, int height, int pixelSizeRatio, int buttonWidth, int buttonHeight) override;
    HRESULT InitializeText() override;
    void ResizeDocument(int width, int height, float zoomFactor) override;
    RenderData CreateWindowRenderData(HWND windowHandle) override;
    RenderData CreateBitmapRenderData(const SizeU& size) override;
    RenderSurfacePtr GetDocumentSurface() override;
    std::vector<COLORREF> ReadDocumentPixels(int logicalWidth, int logicalHeight) override;
    SizeF MeasureText(const std::wstring& text, const FontDesc& font, const RectF& layoutRect) override;
    void PresentDocument(UINT syncInterval) override;
    void Cleanup() override;

private:
    HRESULT CreateDeviceResources();
    HRESULT RebuildDocumentTargetBitmap();
    void RefreshDocumentSurface();
    static DXGI_SWAP_CHAIN_DESC1 MakeSwapChainDescription(int width, int height);

    RenderSurfacePtr documentSurface_;
};
