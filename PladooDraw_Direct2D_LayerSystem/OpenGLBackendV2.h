#pragma once

#include "OpenGLBackendV2State.h"
#include "GraphicsBackend.h"

class OpenGLBitmapSurfaceV2 final : public IBitmapSurface {
public:
    OpenGLBitmapSurfaceV2(SizeU surfaceSize, GLuint textureId);
    ~OpenGLBitmapSurfaceV2() override;

    SizeU GetSize() const override;
    GLuint GetTextureId() const;

private:
    SizeU size_;
    GLuint textureId_ = 0;
};

class OpenGLRenderSurfaceV2 final : public IRenderSurface {
public:
    OpenGLRenderSurfaceV2(SizeU surfaceSize, GLuint fbo, GLuint colorTexture, GLuint depthRbo, HDC hdc, HWND hwnd, bool releaseHdc);
    ~OpenGLRenderSurfaceV2() override;

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

    GLuint GetFbo() const;
    GLuint GetColorTexture() const;
    HDC GetHdc() const;

private:
    void BindTarget();
    void ApplyClip();
    void UseColorProgram(const ColorRGBA& color, float opacity);
    void UseTexProgram(const ColorRGBA& color, float opacity, bool glyphMode);
    void UploadQuad(const RectF& rect, bool withUv, float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f);
    void DrawEllipseInternal(const EllipseF& ellipse, const ColorRGBA& color, bool fill, float strokeWidth);
    PointF TransformPoint(const PointF& point) const;

    SizeU size_{};
    GLuint fbo_ = 0;
    GLuint colorTexture_ = 0;
    GLuint depthRbo_ = 0;
    HDC hdc_ = nullptr;
    HWND hwnd_ = nullptr;
    bool releaseHdc_ = false;
    bool blendCopy_ = false;
    bool antialiased_ = true;

    Matrix3x2 currentTransform_{};
    std::vector<Matrix3x2> transformStack_;

    struct ClipRect { int x, y, w, h; };
    std::vector<ClipRect> clipStack_;
};

class OpenGLBackendV2 final : public IGraphicsBackend {
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
    bool EnsureContext(HDC targetDc);
    bool EnsurePixelFormat(HDC dc);
    bool LoadExtensions();
    bool BuildShaders();
    bool BuildGeometry();
    bool CreateDocumentSurface(HWND documentWindow);

    RenderSurfacePtr documentSurface_;
    HWND documentHwnd_ = nullptr;
    HDC documentHdc_ = nullptr;
};
