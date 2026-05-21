#pragma once
#include "GraphicsFont.h"
#include "GraphicsTypes.h"

class IBitmapSurface {
public:
    virtual ~IBitmapSurface() = default;
    virtual SizeU GetSize() const = 0;
};

class IRenderSurface {
public:
    virtual ~IRenderSurface() = default;

    virtual SizeU GetSize() const = 0;
    virtual void BeginDraw() = 0;
    virtual HRESULT EndDraw() = 0;
    virtual void Clear(const ColorRGBA& color) = 0;
    virtual void SetTransform(const Matrix3x2& transform) = 0;
    virtual void PushClip(const RectF& clipRect) = 0;
    virtual void PopClip() = 0;
    virtual void DrawBitmap(IBitmapSurface& bitmap, const RectF* destination = nullptr, float opacity = 1.0f) = 0;
    virtual void FillRect(const RectF& rect, const ColorRGBA& color) = 0;
    virtual void StrokeRect(const RectF& rect, const ColorRGBA& color, float strokeWidth) = 0;
    virtual void FillEllipse(const EllipseF& ellipse, const ColorRGBA& color) = 0;
    virtual void StrokeEllipse(const EllipseF& ellipse, const ColorRGBA& color, float strokeWidth) = 0;
    virtual void DrawLine(const PointF& start, const PointF& end, const ColorRGBA& color, float strokeWidth) = 0;
    virtual void DrawText(const std::wstring& text, const FontDesc& font, const RectF& layoutRect, const ColorRGBA& color) = 0;
    virtual void SetPrimitiveBlendCopy(bool enabled) = 0;
    virtual void SetAliased(bool enabled) = 0;
    virtual void Present(UINT syncInterval) = 0;
};

using BitmapSurfacePtr = std::shared_ptr<IBitmapSurface>;
using RenderSurfacePtr = std::shared_ptr<IRenderSurface>;

struct RenderData {
    SizeU size{};
    RenderSurfacePtr surfaceHandle;
    BitmapSurfacePtr bitmapHandle;
};

class IGraphicsBackend {
public:
    virtual ~IGraphicsBackend() = default;

    virtual HRESULT InitializeMainWindow(HWND mainWindow) = 0;
    virtual HRESULT InitializeDocument(HWND documentWindow, int width, int height, int pixelSizeRatio, int buttonWidth, int buttonHeight) = 0;
    virtual HRESULT InitializeText() = 0;
    virtual void ResizeDocument(int width, int height, float zoomFactor) = 0;
    virtual RenderData CreateWindowRenderData(HWND windowHandle) = 0;
    virtual RenderData CreateBitmapRenderData(const SizeU& size) = 0;
    virtual RenderSurfacePtr GetDocumentSurface() = 0;
    virtual std::vector<COLORREF> ReadDocumentPixels(int logicalWidth, int logicalHeight) = 0;
    virtual SizeF MeasureText(const std::wstring& text, const FontDesc& font, const RectF& layoutRect) = 0;
    virtual void PresentDocument(UINT syncInterval) = 0;
    virtual void Cleanup() = 0;
};
