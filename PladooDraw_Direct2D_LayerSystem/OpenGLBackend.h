#pragma once

#include "OpenGLBackendState.h"
#include "GraphicsBackend.h"
#include "Constants.h"

#include <vector>
#include <unordered_map>
#include <string>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")

// -----------------------------------------------------------------------
// OpenGLBitmapSurface
//   Wraps an OpenGL texture.  Analogous to Direct2DBitmapSurface.
// -----------------------------------------------------------------------
class OpenGLBitmapSurface final : public IBitmapSurface {
public:
    OpenGLBitmapSurface(SizeU surfaceSize, GLuint textureID);
    ~OpenGLBitmapSurface() override;

    SizeU   GetSize()      const override;
    GLuint  GetTextureID() const;

private:
    SizeU  size_;
    GLuint textureID_;
};

// -----------------------------------------------------------------------
// OpenGLRenderSurface
//   Wraps an OpenGL FBO (or the default framebuffer for a window).
//   Analogous to Direct2DRenderSurface.
// -----------------------------------------------------------------------
class OpenGLRenderSurface final : public IRenderSurface {
public:
    // fboID == 0  → render directly to the window (default framebuffer)
    // fboID != 0  → render to an off-screen FBO / texture
    OpenGLRenderSurface(SizeU surfaceSize,
        GLuint fboID,
        GLuint colorTextureID,  // 0 if default framebuffer
        HDC    deviceContext,   // nullptr if off-screen
        HWND   windowHandle);   // nullptr if off-screen
    ~OpenGLRenderSurface() override;

    // --- IRenderSurface ---
    SizeU   GetSize()                                               const override;
    void    BeginDraw()                                                   override;
    HRESULT EndDraw()                                                     override;
    void    Clear(const ColorRGBA& color)                                 override;
    void    SetTransform(const Matrix3x2& transform)                      override;
    void    PushClip(const RectF& clipRect)                               override;
    void    PopClip()                                                     override;
    void    DrawBitmap(IBitmapSurface& bitmap,
        const RectF* destination,
        float opacity)                                     override;
    void    FillRect(const RectF& rect, const ColorRGBA& color)          override;
    void    StrokeRect(const RectF& rect, const ColorRGBA& color,
        float strokeWidth)                                 override;
    void    FillEllipse(const EllipseF& ellipse, const ColorRGBA& color) override;
    void    StrokeEllipse(const EllipseF& ellipse, const ColorRGBA& color,
        float strokeWidth)                              override;
    void    DrawLine(const PointF& start, const PointF& end,
        const ColorRGBA& color, float strokeWidth)          override;
    void    DrawText(const std::wstring& text, const FontDesc& font,
        const RectF& layoutRect, const ColorRGBA& color)    override;
    void    SetPrimitiveBlendCopy(bool enabled)                           override;
    void    SetAliased(bool enabled)                                      override;
    void    Present(UINT syncInterval)                                    override;

    // --- Accessors ---
    GLuint  GetFBO()            const;
    GLuint  GetColorTexture()   const;

private:
    // --- Internal helpers ---
    void    BindAndSetViewport();
    void    DrawQuad(GLuint shader, float x, float y, float w, float h,
        const ColorRGBA& color, float opacity = 1.0f);
    void    DrawTexturedQuad(GLuint textureID,
        float sx, float sy, float sw, float sh,
        float dx, float dy, float dw, float dh,
        float opacity);
    void    DrawEllipseImmediate(const EllipseF& ellipse, const ColorRGBA& color,
        bool fill, float strokeWidth);

    // Map a 2-D point through the current transform then to NDC
    // (returns clip-space x, y ready for glVertex2f in legacy mode –
    //  in our shader path we pass the transform as a uniform instead)
    void    SetColorShaderUniforms(const ColorRGBA& color, float opacity = 1.0f);
    void    BuildOrthoMatrix(float mat[16]) const;

    SizeU  size_;
    GLuint fboID_;
    GLuint colorTextureID_;
    HDC    hdc_;
    HWND   hwnd_;

    // Transform stack
    Matrix3x2              currentTransform_;
    std::vector<Matrix3x2> transformStack_;

    // Clip-rect stack (scissor test)
    struct ClipRect { int x, y, w, h; };
    std::vector<ClipRect> clipStack_;

    // Blend mode: false = src-over, true = copy (no blending)
    bool blendCopy_ = false;
    bool isDocumentSurface_ = false;
    bool antialiased_ = true;

    friend class OpenGLBackend;
};

// -----------------------------------------------------------------------
// OpenGLBackend
//   Implements IGraphicsBackend for OpenGL / WGL.
//   Analogous to Direct2DBackend.
// -----------------------------------------------------------------------
class OpenGLBackend final : public IGraphicsBackend {
public:
    HRESULT InitializeMainWindow(HWND mainWindow)                                          override;
    HRESULT InitializeDocument(HWND documentWindow,
        int width, int height,
        int pixelSizeRatio,
        int buttonWidth, int buttonHeight)                          override;
    HRESULT InitializeText()                                                               override;
    void    ResizeDocument(int width, int height, float zoomFactor)                       override;
    RenderData CreateWindowRenderData(HWND windowHandle)                                  override;
    RenderData CreateBitmapRenderData(const SizeU& size)                                  override;
    RenderSurfacePtr GetDocumentSurface()                                                  override;
    std::vector<COLORREF> ReadDocumentPixels(int logicalWidth, int logicalHeight)         override;
    SizeF   MeasureText(const std::wstring& text, const FontDesc& font,
        const RectF& layoutRect)                                           override;
    void    PresentDocument(UINT syncInterval)                                             override;
    void    Cleanup()                                                                      override;

private:
    bool    EnsureContext();
    HRESULT LoadExtensions();
    HRESULT BuildShaders();
    HRESULT BuildQuadGeometry();
    HRESULT CreateDocumentFBO(int pixelWidth, int pixelHeight);

    void    RebuildDocumentSurface();
    void    BlitDocumentToWindow();   // copies off-screen FBO → window back buffer

    // Document FBO resources
    GLuint g_docFBO_ = 0;
    GLuint g_docColorTex_ = 0;
    GLuint g_docDepthRBO_ = 0;

    int    docPixelWidth_ = 0;
    int    docPixelHeight_ = 0;

    // Window whose DC is current (document window)
    HWND   docHwnd_ = nullptr;
    HDC    docHdc_ = nullptr;

    RenderSurfacePtr documentSurface_;
};