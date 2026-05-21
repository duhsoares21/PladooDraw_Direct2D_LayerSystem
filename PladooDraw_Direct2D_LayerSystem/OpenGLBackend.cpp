// OpenGLBackend.cpp
// Full implementation of IGraphicsBackend using OpenGL 3.3 Core Profile + WGL.
// Text rendering uses GDI as a fallback glyph rasteriser (no external deps),
// with results uploaded as an RGBA texture atlas per draw call.
//
// Build notes:
//   • Link: opengl32.lib  gdi32.lib
//   • Windows SDK required (WGL, GDI).
//   • No other third-party libraries needed.

#include "OpenGLBackend.h"
#include "OpenGLBackendState.h"
#include "GraphicsTypes.h"
#include "GraphicsBackend.h"   // FontDesc lives in GraphicsFont.h, included via GraphicsBackend.h
#include "Helpers.h"
#include "Constants.h"
#include "Layers.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

// ============================================================
// Internal helper macros / constants
// ============================================================

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define GL_FRAMEBUFFER                  0x8D40
#define GL_FRAMEBUFFER_BINDING          0x8CA6
#define GL_RENDERBUFFER                 0x8D41
#define GL_COLOR_ATTACHMENT0            0x8CE0
#define GL_DEPTH_ATTACHMENT             0x8D00
#define GL_DEPTH_COMPONENT24            0x81A6
#define GL_FRAMEBUFFER_COMPLETE         0x8CD5
#define GL_ARRAY_BUFFER                 0x8892
#define GL_STATIC_DRAW                  0x88B4
#define GL_DYNAMIC_DRAW                 0x88E8
#define GL_FRAGMENT_SHADER              0x8B30
#define GL_VERTEX_SHADER                0x8B31
#define GL_COMPILE_STATUS               0x8B81
#define GL_LINK_STATUS                  0x8B82
#define GL_CLAMP_TO_EDGE                0x812F
#define GL_RED                          0x1903
#define GL_RGBA8                        0x8058
#define GL_BGRA                         0x80E1
#define GL_UNPACK_ALIGNMENT             0x0CF5

// WGL_ARB_create_context attribute tokens
#define WGL_CONTEXT_MAJOR_VERSION_ARB   0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB   0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB    0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

// ============================================================
// GLSL shader sources
// ============================================================

static const char* kColorVertSrc = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;

uniform mat4 uOrtho;
uniform mat3x2 uTransform;   // 2-D affine transform (column-major 3x2)

void main() {
    // Apply 2-D affine: [m11 m21 dx; m12 m22 dy] * [x; y; 1]
    vec2 transformed = mat2(uTransform[0].xy, uTransform[1].xy) * aPos
                       + vec2(uTransform[2].x, uTransform[2].y);
    // Hmm – GLSL mat3x2 is 3 columns × 2 rows.
    // We store our Matrix3x2 as: col0=(m11,m12), col1=(m21,m22), col2=(dx,dy).
    gl_Position = uOrtho * vec4(transformed, 0.0, 1.0);
}
)glsl";

static const char* kColorFragSrc = R"glsl(
#version 330 core
out vec4 FragColor;
uniform vec4  uColor;
uniform float uOpacity;
void main() {
    FragColor = vec4(uColor.rgb, uColor.a * uOpacity);
}
)glsl";

static const char* kTexVertSrc = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

uniform mat4 uOrtho;
uniform mat3x2 uTransform;

out vec2 vUV;
void main() {
    vec2 transformed = mat2(uTransform[0].xy, uTransform[1].xy) * aPos
                       + vec2(uTransform[2].x, uTransform[2].y);
    gl_Position = uOrtho * vec4(transformed, 0.0, 1.0);
    vUV = aUV;
}
)glsl";

static const char* kTexFragSrc = R"glsl(
#version 330 core
in  vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
uniform vec4      uColor;   // tint (text color); for bitmap, pass (1,1,1,1)
uniform float     uOpacity;
uniform int       uGlyphMode; // 1 = use red channel as alpha mask (text), 0 = normal RGBA
void main() {
    if (uGlyphMode == 1) {
        float alpha = texture(uTex, vUV).r;
        FragColor = vec4(uColor.rgb, uColor.a * alpha * uOpacity);
    } else {
        vec4 c = texture(uTex, vUV);
        FragColor = vec4(c.rgb * uColor.rgb, c.a * uOpacity);
    }
}
)glsl";

// ============================================================
// Anonymous namespace – internal helpers
// ============================================================
namespace {

// ----------------------------------------------------------
// Extension loader helper
// ----------------------------------------------------------
template<typename T>
static bool LoadProc(T& fnPtr, const char* name) {
    // Two-step cast required by MSVC: PROC -> void* -> target function pointer type
    PROC raw = wglGetProcAddress(name);
    if (!raw) {
        // Some core functions (pre-GL 1.2) aren't returned by wglGetProcAddress.
        // Fall back to GetProcAddress on opengl32.dll.
        static HMODULE hGL = GetModuleHandleA("opengl32.dll");
        if (hGL) raw = reinterpret_cast<PROC>(GetProcAddress(hGL, name));
    }
    fnPtr = reinterpret_cast<T>(reinterpret_cast<void*>(raw));
    return fnPtr != nullptr;
}

// ----------------------------------------------------------
// Shader compilation helper
// ----------------------------------------------------------
static GLuint CompileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512] = {};
        glGetShaderInfoLog(shader, 512, nullptr, log);
        OutputDebugStringA("OpenGL shader compile error: ");
        OutputDebugStringA(log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint LinkProgram(const char* vertSrc, const char* fragSrc) {
    GLuint vs = CompileShader(GL_VERTEX_SHADER,   vertSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!vs || !fs) return 0;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

// ----------------------------------------------------------
// Build a 4x4 orthographic matrix (column-major, as GL expects)
// Maps [0..w] × [0..h] → NDC, y-down (matching D2D convention).
// ----------------------------------------------------------
static void MakeOrtho(float out[16], float w, float h) {
    // left=0, right=w, bottom=h, top=0, near=-1, far=1
    std::memset(out, 0, 64);
    out[0]  =  2.0f / w;
    out[5]  = -2.0f / h;
    out[10] = -1.0f;
    out[12] = -1.0f;
    out[13] =  1.0f;
    out[15] =  1.0f;
}

// ----------------------------------------------------------
// Pack Matrix3x2 into the column-major 3x2 float array
// that GLSL mat3x2 expects (3 columns, 2 rows).
//   col0 = (m11, m12)
//   col1 = (m21, m22)
//   col2 = (dx,  dy)
// ----------------------------------------------------------
static void PackTransform(float out[6], const Matrix3x2& t) {
    out[0] = t.m11; out[1] = t.m12;
    out[2] = t.m21; out[3] = t.m22;
    out[4] = t.dx;  out[5] = t.dy;
}

// ----------------------------------------------------------
// Upload a quad (2 triangles) to g_glQuadVAO / g_glQuadVBO.
// Vertices: pos(x,y) + uv(u,v) = 4 floats per vertex, 6 vertices.
// ----------------------------------------------------------
static void UploadQuad(float x, float y, float w, float h,
                       float u0 = 0.0f, float v0 = 0.0f,
                       float u1 = 1.0f, float v1 = 1.0f)
{
    // Caller is responsible for binding the VAO before calling this.
    // We only update the VBO data here.
    if (!g_glQuadVBO) return;

    float verts[] = {
        // pos          uv
        x,     y,      u0, v0,
        x+w,   y,      u1, v0,
        x+w,   y+h,    u1, v1,

        x,     y,      u0, v0,
        x+w,   y+h,    u1, v1,
        x,     y+h,    u0, v1,
    };

    // glBindBuffer is safe to call without VAO; it just sets the current binding.
    // The VAO remembers which VBO to use via the VertexAttribPointer call made
    // in BuildQuadGeometry – we only need to push new data into that VBO.
    glBindBuffer(GL_ARRAY_BUFFER, g_glQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(verts)), verts, GL_DYNAMIC_DRAW);
}

// ----------------------------------------------------------
// Rasterize a single Unicode codepoint using GDI into a 1-channel
// bitmap, returning the glyph metrics and pixel data.
// Used as a lightweight text backend that requires no extra libs.
// ----------------------------------------------------------
struct GlyphInfo {
    int width  = 0;
    int height = 0;
    int bearingX = 0;
    int bearingY = 0;
    int advance  = 0;
    std::vector<uint8_t> bitmap; // 1 byte per pixel, white-on-black
};

static GlyphInfo RasterizeGlyphGDI(wchar_t ch, const FontDesc& font) {
    GlyphInfo info;

    HDC hdc = CreateCompatibleDC(nullptr);
    if (!hdc) return info;

    LOGFONTW lf = {};
    lf.lfHeight         = static_cast<LONG>(font.size <= 0 ? 120 : font.size) / 10;
    lf.lfWeight         = font.weight == 0 ? FW_NORMAL : font.weight;
    lf.lfItalic         = font.italic ? TRUE : FALSE;
    lf.lfUnderline      = font.underline ? TRUE : FALSE;
    lf.lfStrikeOut      = font.strike ? TRUE : FALSE;
    lf.lfCharSet        = DEFAULT_CHARSET;
    lf.lfQuality        = CLEARTYPE_QUALITY;
    wcscpy_s(lf.lfFaceName, font.family.empty() ? L"Arial" : font.family.c_str());

    HFONT hFont = CreateFontIndirectW(&lf);
    if (!hFont) { DeleteDC(hdc); return info; }
    HFONT hOld = static_cast<HFONT>(SelectObject(hdc, hFont));

    // Measure
    SIZE sz = {};
    wchar_t str[2] = { ch, 0 };
    GetTextExtentPoint32W(hdc, str, 1, &sz);
    if (sz.cx <= 0 || sz.cy <= 0) {
        SelectObject(hdc, hOld);
        DeleteObject(hFont);
        DeleteDC(hdc);
        return info;
    }

    info.width   = sz.cx;
    info.height  = sz.cy;
    info.advance = sz.cx;

    // Create monochrome bitmap
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = sz.cx;
    bmi.bmiHeader.biHeight      = -sz.cy; // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hBmp) {
        SelectObject(hdc, hOld);
        DeleteObject(hFont);
        DeleteDC(hdc);
        return info;
    }

    HBITMAP hOldBmp = static_cast<HBITMAP>(SelectObject(hdc, hBmp));
    SetBkColor(hdc, RGB(0, 0, 0));
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT rc = { 0, 0, sz.cx, sz.cy };
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &rc, str, 1, nullptr);

    // Extract red channel as the alpha/coverage
    info.bitmap.resize(static_cast<size_t>(sz.cx) * sz.cy);
    const uint32_t* src = static_cast<const uint32_t*>(pBits);
    for (int i = 0; i < sz.cx * sz.cy; ++i) {
        info.bitmap[i] = static_cast<uint8_t>(src[i] & 0xFF); // R channel
    }

    SelectObject(hdc, hOldBmp);
    SelectObject(hdc, hOld);
    DeleteObject(hBmp);
    DeleteObject(hFont);
    DeleteDC(hdc);
    return info;
}

} // namespace

// ============================================================
// OpenGLBitmapSurface
// ============================================================

OpenGLBitmapSurface::OpenGLBitmapSurface(SizeU surfaceSize, GLuint textureID)
    : size_(surfaceSize), textureID_(textureID) {
}

OpenGLBitmapSurface::~OpenGLBitmapSurface() {
    if (textureID_) {
        glDeleteTextures(1, &textureID_);
    }
}

SizeU OpenGLBitmapSurface::GetSize() const {
    return size_;
}

GLuint OpenGLBitmapSurface::GetTextureID() const {
    return textureID_;
}

// ============================================================
// OpenGLRenderSurface
// ============================================================

OpenGLRenderSurface::OpenGLRenderSurface(SizeU surfaceSize,
                                         GLuint fboID,
                                         GLuint colorTextureID,
                                         HDC    deviceContext,
                                         HWND   windowHandle)
    : size_(surfaceSize),
      fboID_(fboID),
      colorTextureID_(colorTextureID),
      hdc_(deviceContext),
      hwnd_(windowHandle),
      currentTransform_(MakeIdentityMatrix3x2()) {
}

OpenGLRenderSurface::~OpenGLRenderSurface() {
    // FBO and texture ownership stay with the backend (or the caller that
    // created them); only the surface created by CreateBitmapRenderData
    // owns its FBO.  For simplicity we always delete here – callers that
    // share resources must zero out fboID_ / colorTextureID_ after hand-off.
    if (fboID_) {
        glDeleteFramebuffers(1, &fboID_);
    }
    // colorTextureID_ is also wrapped by an OpenGLBitmapSurface elsewhere
    // when created by CreateBitmapRenderData; we don't double-delete.
}

SizeU OpenGLRenderSurface::GetSize() const {
    return size_;
}

static void EnsureContextCurrent(HDC hdc) {
    if (wglGetCurrentContext() != g_glContext && hdc && g_glContext) {
        wglMakeCurrent(hdc, g_glContext);
    }
}

void OpenGLRenderSurface::BindAndSetViewport() {
    // Ensure the shared GL context is current on this thread.
    // In a single-threaded app this is almost always already true,
    // but it can lapse after a window resize / focus change.
    if (wglGetCurrentContext() != g_glContext) {
        HDC dc = hdc_ ? hdc_ : g_glMainDC;
        if (dc && g_glContext) {
            wglMakeCurrent(dc, g_glContext);
        }
    }

    if (glBindFramebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, fboID_);
    }
    glViewport(0, 0,
               static_cast<GLsizei>(size_.width),
               static_cast<GLsizei>(size_.height));
}

void OpenGLRenderSurface::BeginDraw() {
    BindAndSetViewport();

    // Re-apply blend state
    if (blendCopy_) {
        glDisable(GL_BLEND);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}

HRESULT OpenGLRenderSurface::EndDraw() {
    // Nothing special to flush in OpenGL – caller calls Present or SwapBuffers
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return S_OK;
}

void OpenGLRenderSurface::Clear(const ColorRGBA& color) {
    BindAndSetViewport();
    // glClear writes directly to all channels regardless of blend state,
    // but we must ensure the color mask is fully open so alpha is written too.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderSurface::SetTransform(const Matrix3x2& transform) {
    currentTransform_ = transform;
}

void OpenGLRenderSurface::PushClip(const RectF& clipRect) {
    // Our ortho matrix maps Y=0->top, Y=height->bottom (y-down convention).
    // GL scissor origin is always bottom-left regardless of FBO or default framebuffer.
    // So we must always flip Y: scissor_y = height - logical_y - scissor_h.
    int x = static_cast<int>(clipRect.left);
    int w = static_cast<int>(clipRect.right  - clipRect.left);
    int h = static_cast<int>(clipRect.bottom - clipRect.top);
    int y = static_cast<int>(size_.height) - static_cast<int>(clipRect.top) - h;

    clipStack_.push_back({ x, y, w, h });
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, w, h);
}

void OpenGLRenderSurface::PopClip() {
    if (clipStack_.empty()) return;
    clipStack_.pop_back();

    if (clipStack_.empty()) {
        glDisable(GL_SCISSOR_TEST);
    } else {
        const auto& top = clipStack_.back();
        glScissor(top.x, top.y, top.w, top.h);
    }
}

void OpenGLRenderSurface::BuildOrthoMatrix(float mat[16]) const {
    MakeOrtho(mat,
              static_cast<float>(size_.width),
              static_cast<float>(size_.height));
}

void OpenGLRenderSurface::SetColorShaderUniforms(const ColorRGBA& color, float opacity) {
    if (!g_glShaderColor) return;
    if (!glUniformMatrix4fv || !glUniformMatrix3x2fv || !glUniform4f || !glUniform1f) return;

    float ortho[16];
    BuildOrthoMatrix(ortho);
    float xform[6];
    PackTransform(xform, currentTransform_);

    GLint locOrtho     = glGetUniformLocation(g_glShaderColor, "uOrtho");
    GLint locTransform = glGetUniformLocation(g_glShaderColor, "uTransform");
    GLint locColor     = glGetUniformLocation(g_glShaderColor, "uColor");
    GLint locOpacity   = glGetUniformLocation(g_glShaderColor, "uOpacity");

    if (locOrtho     >= 0) glUniformMatrix4fv(locOrtho,       1, GL_FALSE, ortho);
    if (locTransform >= 0) glUniformMatrix3x2fv(locTransform, 1, GL_FALSE, xform);
    if (locColor     >= 0) glUniform4f(locColor, color.r, color.g, color.b, color.a);
    if (locOpacity   >= 0) glUniform1f(locOpacity, opacity);
}

void OpenGLRenderSurface::DrawQuad(GLuint shader,
                                   float x, float y, float w, float h,
                                   const ColorRGBA& color, float opacity) {
    if (!shader || !g_glQuadVAO || !g_glQuadVBO) return;
    if (!glUseProgram || !glBindVertexArray || !glDrawArrays) return;

    // Bind VAO first so the VBO upload in UploadQuad is recorded in its state.
    glBindVertexArray(g_glQuadVAO);
    UploadQuad(x, y, w, h);

    glUseProgram(shader);
    SetColorShaderUniforms(color, opacity);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Unbind to avoid accidental state leakage
    glUseProgram(0);
    glBindVertexArray(0);
}

void OpenGLRenderSurface::DrawTexturedQuad(GLuint textureID,
                                           float sx, float sy, float sw, float sh,
                                           float dx, float dy, float dw, float dh,
                                           float opacity) {
    // UV coords from source dimensions (full texture)
    float u0 = sx / sw;
    float v0 = sy / sh;
    float u1 = (sx + dw) / sw;
    float v1 = (sy + dh) / sh;
    {
        char buf[192];
        sprintf_s(buf, "DrawTexturedQuad this=%p VAO=%u VBO=%u shader=%u mat4=%p tex=%u",
            (void*)this, g_glQuadVAO, g_glQuadVBO, g_glShaderTex,
            (void*)glUniformMatrix4fv, textureID);
        HCreateLogData("draw.log", buf);
    }
    if (!g_glQuadVAO || !g_glQuadVBO) return;
    if (!g_glShaderTex || !glUniformMatrix4fv || !glUniformMatrix3x2fv) return;

    // Bind VAO first, then upload data into its VBO
    glBindVertexArray(g_glQuadVAO);
    UploadQuad(dx, dy, dw, dh, u0, v0, u1, v1);

    float ortho[16];
    BuildOrthoMatrix(ortho);
    float xform[6];
    PackTransform(xform, currentTransform_);

    glUseProgram(g_glShaderTex);

    {
        GLint l;
        l = glGetUniformLocation(g_glShaderTex, "uOrtho");
        if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, ortho);
        l = glGetUniformLocation(g_glShaderTex, "uTransform");
        if (l >= 0) glUniformMatrix3x2fv(l, 1, GL_FALSE, xform);
        l = glGetUniformLocation(g_glShaderTex, "uOpacity");
        if (l >= 0) glUniform1f(l, opacity);
        l = glGetUniformLocation(g_glShaderTex, "uColor");
        if (l >= 0) glUniform4f(l, 1.0f, 1.0f, 1.0f, 1.0f);
        l = glGetUniformLocation(g_glShaderTex, "uGlyphMode");
        if (l >= 0) glUniform1i(l, 0);
    }

    // VAO already bound above
    if (glActiveTexture) glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    {
        GLint l = glGetUniformLocation(g_glShaderTex, "uTex");
        if (l >= 0) glUniform1i(l, 0);
    }

    // Diagnostic: check GL error and confirm draw happened
    {
        GLenum err = glGetError();
        GLint boundFBO = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &boundFBO);
        char buf[128];
        sprintf_s(buf, "DrawTexturedQuad: tex=%u fbo_bound=%d shader=%u err=0x%x u0=%.2f v0=%.2f u1=%.2f v1=%.2f",
            textureID, boundFBO, g_glShaderTex, err,
            sx/sw, sy/sh, (sx+dw)/sw, (sy+dh)/sh);
        HCreateLogData("draw.log", buf);
    }

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void OpenGLRenderSurface::FillRect(const RectF& rect, const ColorRGBA& color) {
    BindAndSetViewport();
    float x = rect.left;
    float y = rect.top;
    float w = rect.right  - rect.left;
    float h = rect.bottom - rect.top;

    {
        char buf[128];
        sprintf_s(buf, "FillRect this=%p fbo=%u size=%ux%u x=%.1f y=%.1f w=%.1f h=%.1f ctx=%p VAO=%u shader=%u",
            (void*)this, fboID_, size_.width, size_.height, x, y, w, h,
            (void*)wglGetCurrentContext(), g_glQuadVAO, g_glShaderColor);
        HCreateLogData("draw.log", buf);
    }

    DrawQuad(g_glShaderColor, x, y, w, h, color);
}

void OpenGLRenderSurface::StrokeRect(const RectF& rect, const ColorRGBA& color, float strokeWidth) {
    BindAndSetViewport();
    float l = rect.left,   t = rect.top;
    float r = rect.right,  b = rect.bottom;
    float sw = strokeWidth;

    // Four edge quads
    struct { float x, y, w, h; } edges[] = {
        { l,      t,      r-l, sw   },  // top
        { l,      b-sw,   r-l, sw   },  // bottom
        { l,      t,      sw,  b-t  },  // left
        { r-sw,   t,      sw,  b-t  },  // right
    };
    for (const auto& e : edges) {
        DrawQuad(g_glShaderColor, e.x, e.y, e.w, e.h, color);
    }
}

void OpenGLRenderSurface::DrawEllipseImmediate(const EllipseF& ellipse,
                                               const ColorRGBA& color,
                                               bool fill,
                                               float strokeWidth)
{
    BindAndSetViewport();

    const int segments = antialiased_ ? 64 : 32;
    const float cx = ellipse.point.x;
    const float cy = ellipse.point.y;
    const float rx = ellipse.radiusX;
    const float ry = ellipse.radiusY;

    // Build vertex strip manually via GL_TRIANGLE_FAN (legacy) or
    // upload into the VBO and draw as GL_TRIANGLE_FAN / GL_LINE_LOOP.
    // We use the VBO here for GL 3.3 core compatibility.

    std::vector<float> verts;

    if (fill) {
        // centre + circumference
        verts.reserve((segments + 2) * 4);
        // No UV needed for color shader – pad with zeros
        verts.push_back(cx); verts.push_back(cy); verts.push_back(0); verts.push_back(0);
        for (int i = 0; i <= segments; ++i) {
            float angle = static_cast<float>(2.0 * M_PI * i / segments);
            verts.push_back(cx + rx * std::cos(angle));
            verts.push_back(cy + ry * std::sin(angle));
            verts.push_back(0); verts.push_back(0);
        }
    } else {
        // Stroke: two rings
        float rx0 = rx - strokeWidth * 0.5f;
        float ry0 = ry - strokeWidth * 0.5f;
        float rx1 = rx + strokeWidth * 0.5f;
        float ry1 = ry + strokeWidth * 0.5f;
        verts.reserve((segments + 1) * 8);
        for (int i = 0; i <= segments; ++i) {
            float angle = static_cast<float>(2.0 * M_PI * i / segments);
            float c = std::cos(angle);
            float s = std::sin(angle);
            // inner
            verts.push_back(cx + rx0 * c); verts.push_back(cy + ry0 * s);
            verts.push_back(0); verts.push_back(0);
            // outer
            verts.push_back(cx + rx1 * c); verts.push_back(cy + ry1 * s);
            verts.push_back(0); verts.push_back(0);
        }
    }

    if (!g_glQuadVAO || !g_glQuadVBO) return;

    glUseProgram(g_glShaderColor);
    float ortho[16]; BuildOrthoMatrix(ortho);
    float xform[6];  PackTransform(xform, currentTransform_);
    if (!glUniformMatrix4fv || !glUniformMatrix3x2fv) return;
    {
        GLint l;
        l = glGetUniformLocation(g_glShaderColor, "uOrtho");
        if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, ortho);
        l = glGetUniformLocation(g_glShaderColor, "uTransform");
        if (l >= 0) glUniformMatrix3x2fv(l, 1, GL_FALSE, xform);
        l = glGetUniformLocation(g_glShaderColor, "uColor");
        if (l >= 0) glUniform4f(l, color.r, color.g, color.b, color.a);
        l = glGetUniformLocation(g_glShaderColor, "uOpacity");
        if (l >= 0) glUniform1f(l, 1.0f);
    }

    // Bind VAO so draw state is correct, then upload vertices
    glBindVertexArray(g_glQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_glQuadVBO);

    const GLsizeiptr byteSize = static_cast<GLsizeiptr>(verts.size() * sizeof(float));
    glBufferData(GL_ARRAY_BUFFER, byteSize, verts.data(), GL_DYNAMIC_DRAW);

    // GL_TRIANGLE_FAN is removed in Core Profile; convert fan to triangle list.
    // For fill: centre + N rim points → (N) triangles = N*3 verts
    // For stroke: already a TRIANGLE_STRIP → draw as-is using TRIANGLE_STRIP
    if (fill) {
        // Build explicit triangle list from the fan data already in verts:
        // verts layout: [centre, p0, p1, p2, ..., pN]  (each = 4 floats)
        // Triangle i = centre, p[i], p[i+1]
        int rimCount = static_cast<int>(verts.size() / 4) - 1; // exclude centre
        std::vector<float> tris;
        tris.reserve(static_cast<size_t>(rimCount) * 3 * 4);
        for (int i = 0; i < rimCount; ++i) {
            // centre
            tris.push_back(verts[0]); tris.push_back(verts[1]);
            tris.push_back(verts[2]); tris.push_back(verts[3]);
            // p[i]
            int base = (i + 1) * 4;
            tris.push_back(verts[base]);     tris.push_back(verts[base + 1]);
            tris.push_back(verts[base + 2]); tris.push_back(verts[base + 3]);
            // p[i+1]  (wraps: last rim point connects back to p[0])
            int next = (i + 2 <= rimCount) ? (i + 2) : 1;
            int nbase = next * 4;
            tris.push_back(verts[nbase]);     tris.push_back(verts[nbase + 1]);
            tris.push_back(verts[nbase + 2]); tris.push_back(verts[nbase + 3]);
        }
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(tris.size() * sizeof(float)),
                     tris.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(tris.size() / 4));
    } else {
        glDrawArrays(GL_TRIANGLE_STRIP, 0, static_cast<GLsizei>(verts.size() / 4));
    }
}

void OpenGLRenderSurface::FillEllipse(const EllipseF& ellipse, const ColorRGBA& color) {
    DrawEllipseImmediate(ellipse, color, true, 0.0f);
}

void OpenGLRenderSurface::StrokeEllipse(const EllipseF& ellipse, const ColorRGBA& color, float strokeWidth) {
    DrawEllipseImmediate(ellipse, color, false, strokeWidth);
}

void OpenGLRenderSurface::DrawLine(const PointF& start, const PointF& end,
                                   const ColorRGBA& color, float strokeWidth)
{
    BindAndSetViewport();

    // Build a quad aligned to the line direction
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float len = std::sqrt(dx*dx + dy*dy);
    if (len < 0.001f) return;

    float nx = -dy / len * (strokeWidth * 0.5f);
    float ny =  dx / len * (strokeWidth * 0.5f);

    float verts[] = {
        start.x + nx, start.y + ny, 0.f, 0.f,
        start.x - nx, start.y - ny, 0.f, 1.f,
        end.x   + nx, end.y   + ny, 1.f, 0.f,

        start.x - nx, start.y - ny, 0.f, 1.f,
        end.x   - nx, end.y   - ny, 1.f, 1.f,
        end.x   + nx, end.y   + ny, 1.f, 0.f,
    };

    if (!g_glQuadVAO || !g_glQuadVBO) return;

    glUseProgram(g_glShaderColor);
    float ortho[16]; BuildOrthoMatrix(ortho);
    float xform[6];  PackTransform(xform, currentTransform_);
    if (!glUniformMatrix4fv || !glUniformMatrix3x2fv) return;
    {
        GLint l;
        l = glGetUniformLocation(g_glShaderColor, "uOrtho");
        if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, ortho);
        l = glGetUniformLocation(g_glShaderColor, "uTransform");
        if (l >= 0) glUniformMatrix3x2fv(l, 1, GL_FALSE, xform);
        l = glGetUniformLocation(g_glShaderColor, "uColor");
        if (l >= 0) glUniform4f(l, color.r, color.g, color.b, color.a);
        l = glGetUniformLocation(g_glShaderColor, "uOpacity");
        if (l >= 0) glUniform1f(l, 1.0f);
    }

    glBindVertexArray(g_glQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_glQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(verts)), verts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void OpenGLRenderSurface::DrawBitmap(IBitmapSurface& bitmap,
                                     const RectF* destination,
                                     float opacity)
{
    BindAndSetViewport();

    auto* glBitmap = dynamic_cast<OpenGLBitmapSurface*>(&bitmap);
    if (!glBitmap) {
        HCreateLogData("draw.log", "DrawBitmap: dynamic_cast failed - not OpenGLBitmapSurface");
        return;
    }

    {
        char buf[128];
        sprintf_s(buf, "DrawBitmap: dstFBO=%u srcTex=%u bmpSize=%ux%u opacity=%.2f",
            fboID_, glBitmap->GetTextureID(),
            glBitmap->GetSize().width, glBitmap->GetSize().height, opacity);
        HCreateLogData("draw.log", buf);
    }

    SizeU bmpSize = glBitmap->GetSize();
    float dw = destination ? (destination->right  - destination->left) : static_cast<float>(size_.width);
    float dh = destination ? (destination->bottom - destination->top)  : static_cast<float>(size_.height);
    float dx = destination ? destination->left : 0.0f;
    float dy = destination ? destination->top  : 0.0f;

    // Always sample the full source texture (UV 0..1).
    // The destination rect controls where it lands on the target FBO.
    if (glActiveTexture) glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glBitmap->GetTextureID());

    glBindVertexArray(g_glQuadVAO);
    UploadQuad(dx, dy, dw, dh, 0.f, 0.f, 1.f, 1.f);

    float ortho[16]; BuildOrthoMatrix(ortho);
    float xform[6];  PackTransform(xform, currentTransform_);

    glUseProgram(g_glShaderTex);
    {
        GLint l;
        l = glGetUniformLocation(g_glShaderTex, "uOrtho");     if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, ortho);
        l = glGetUniformLocation(g_glShaderTex, "uTransform"); if (l >= 0) glUniformMatrix3x2fv(l, 1, GL_FALSE, xform);
        l = glGetUniformLocation(g_glShaderTex, "uOpacity");   if (l >= 0) glUniform1f(l, opacity);
        l = glGetUniformLocation(g_glShaderTex, "uColor");     if (l >= 0) glUniform4f(l, 1,1,1,1);
        l = glGetUniformLocation(g_glShaderTex, "uGlyphMode"); if (l >= 0) glUniform1i(l, 0);
        l = glGetUniformLocation(g_glShaderTex, "uTex");       if (l >= 0) glUniform1i(l, 0);
    }
    glDrawArrays(GL_TRIANGLES, 0, 6);
    {
        GLenum e = glGetError();
        if (e != 0) {
            char buf[64]; sprintf_s(buf, "DrawBitmap glDrawArrays err=0x%x", e);
            HCreateLogData("draw.log", buf);
        }
    }
    glUseProgram(0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLRenderSurface::DrawText(const std::wstring& text, const FontDesc& font,
                                   const RectF& layoutRect, const ColorRGBA& color)
{
    if (text.empty()) return;
    BindAndSetViewport();

    float cursorX = layoutRect.left;
    float cursorY = layoutRect.top;

    for (wchar_t ch : text) {
        if (ch == L'\n') {
            cursorX = layoutRect.left;
            float lineH = static_cast<float>(font.size <= 0 ? 12 : font.size) / 10.0f + 2.0f;
            cursorY += lineH;
            continue;
        }

        GlyphInfo glyph = RasterizeGlyphGDI(ch, font);
        if (glyph.bitmap.empty()) {
            cursorX += static_cast<float>(glyph.advance > 0 ? glyph.advance : 4);
            continue;
        }

        // Upload glyph as a single-channel texture
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                     glyph.width, glyph.height,
                     0, GL_RED, GL_UNSIGNED_BYTE,
                     glyph.bitmap.data());

        // Draw glyph quad
        float gx = cursorX;
        float gy = cursorY;
        float gw = static_cast<float>(glyph.width);
        float gh = static_cast<float>(glyph.height);

        if (!g_glShaderTex || !g_glQuadVAO || !g_glQuadVBO) { glDeleteTextures(1, &tex); continue; }
        if (!glUniformMatrix4fv || !glUniformMatrix3x2fv)    { glDeleteTextures(1, &tex); continue; }

        // Bind VAO before uploading so VBO state is correct for draw
        glBindVertexArray(g_glQuadVAO);
        UploadQuad(gx, gy, gw, gh, 0.f, 0.f, 1.f, 1.f);

        float ortho[16]; BuildOrthoMatrix(ortho);
        float xform[6];  PackTransform(xform, currentTransform_);

        glUseProgram(g_glShaderTex);
        {
            GLint l;
            l = glGetUniformLocation(g_glShaderTex, "uOrtho");
            if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, ortho);
            l = glGetUniformLocation(g_glShaderTex, "uTransform");
            if (l >= 0) glUniformMatrix3x2fv(l, 1, GL_FALSE, xform);
            l = glGetUniformLocation(g_glShaderTex, "uColor");
            if (l >= 0) glUniform4f(l, color.r, color.g, color.b, color.a);
            l = glGetUniformLocation(g_glShaderTex, "uOpacity");
            if (l >= 0) glUniform1f(l, 1.0f);
            l = glGetUniformLocation(g_glShaderTex, "uGlyphMode");
            if (l >= 0) glUniform1i(l, 1);
        }
        if (glActiveTexture) glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        {
            GLint l = glGetUniformLocation(g_glShaderTex, "uTex");
            if (l >= 0) glUniform1i(l, 0);
        }

        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDeleteTextures(1, &tex);

        cursorX += static_cast<float>(glyph.advance);

        // Wrap at layout boundary
        if (cursorX > layoutRect.right) break;
    }
}

void OpenGLRenderSurface::SetPrimitiveBlendCopy(bool enabled) {
    blendCopy_ = enabled;
    if (enabled) {
        glDisable(GL_BLEND);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}

void OpenGLRenderSurface::SetAliased(bool enabled) {
    antialiased_ = !enabled;
    // Line/point smooth (informational for ellipse tessellation)
    if (!enabled) {
        glEnable(GL_LINE_SMOOTH);
    } else {
        glDisable(GL_LINE_SMOOTH);
    }
}

void OpenGLRenderSurface::Present(UINT /*syncInterval*/) {
    // All surfaces use the same path: read the FBO pixels back to CPU,
    // then blit to the target window via GDI (SetDIBitsToDevice).
    //
    // This avoids all wglMakeCurrent-per-window complexity: the single shared
    // GL context renders everything into off-screen FBOs, and GDI handles the
    // final copy to each Win32 window — exactly how a software renderer works.
    // D2D used a DXGI swap chain that was compositor-aware; we replicate that
    // behaviour with a CPU readback instead.

    if (!hwnd_ || !fboID_) return;

    EnsureContextCurrent(g_glMainDC);

    int w = static_cast<int>(size_.width);
    int h = static_cast<int>(size_.height);
    if (w <= 0 || h <= 0) return;

    {
        RECT rc2{}; GetClientRect(hwnd_, &rc2);
        char buf[128];
        sprintf_s(buf, "Present: fbo=%u hwnd=%p size=%dx%d winSize=%dx%d",
            fboID_, (void*)hwnd_, w, h, rc2.right-rc2.left, rc2.bottom-rc2.top);
        HCreateLogData("draw.log", buf);
    }

    // Read pixels from the FBO.
    // Our ortho matrix maps Y=0 to the top of the image and Y=h to the bottom,
    // which means row 0 in the FBO is physically at the GL bottom (origin=bottom-left).
    // glReadPixels therefore returns the image upside-down relative to screen coords.
    // We flip rows while converting RGBA->BGRA for GDI.
    std::vector<uint8_t> pixels(static_cast<size_t>(w * h * 4));
    glBindFramebuffer(GL_FRAMEBUFFER, fboID_);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Log top-left and bottom-left pixels to determine actual Y orientation
    {
        // row 0 in glReadPixels = GL bottom; row h-1 = GL top
        uint8_t* botLeft = &pixels[0];                          // GL row 0 = physical bottom
        uint8_t* topLeft = &pixels[(h-1) * w * 4];             // GL row h-1 = physical top
        char buf[128];
        sprintf_s(buf, "Present pixels: GL_bottom_left=(%d,%d,%d,%d) GL_top_left=(%d,%d,%d,%d)",
            botLeft[0],botLeft[1],botLeft[2],botLeft[3],
            topLeft[0],topLeft[1],topLeft[2],topLeft[3]);
        HCreateLogData("draw.log", buf);
    }

    // Flip rows: GL origin is bottom-left, so row 0 = visual bottom, row h-1 = visual top.
    // We flip to get top-down order for GDI, and convert RGBA->BGRA.
    std::vector<uint8_t> flipped(static_cast<size_t>(w * h * 4));
    for (int row = 0; row < h; ++row) {
        const uint8_t* s = &pixels[static_cast<size_t>((h - 1 - row) * w * 4)];
        uint8_t*       d = &flipped[static_cast<size_t>(row * w * 4)];
        for (int x = 0; x < w; ++x, s += 4, d += 4) {
            d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = s[3];
        }
    }

    BITMAPINFO bmi        = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = h; // positive = bottom-up, paired with row-flip above
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(hwnd_);
    if (hdc) {
        // Scale to fit the actual window client area (handles zoom / resize)
        RECT rc{};
        GetClientRect(hwnd_, &rc);
        int dstW = rc.right  - rc.left;
        int dstH = rc.bottom - rc.top;
        if (dstW > 0 && dstH > 0) {
            StretchDIBits(hdc,
                0, 0, dstW, dstH,   // destination
                0, 0, w,    h,      // source
                flipped.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
        }
        ReleaseDC(hwnd_, hdc);
    }
}

GLuint OpenGLRenderSurface::GetFBO() const {
    return fboID_;
}

GLuint OpenGLRenderSurface::GetColorTexture() const {
    return colorTextureID_;
}

// ============================================================
// OpenGLBackend – private helpers
// ============================================================

// Ensures the shared GL context is current on this thread.
// Must be called at the top of any method that issues GL commands
// outside of a BeginDraw/EndDraw pair.
bool OpenGLBackend::EnsureContext() {
    if (!g_glContext) {
        HCreateLogData("error.log", "EnsureContext: g_glContext is null");
        return false;
    }
    if (wglGetCurrentContext() == g_glContext) return true;

    char buf[256];
    sprintf_s(buf, "EnsureContext: context lost. g_glPermaDC=%p docHdc_=%p g_glMainDC=%p",
              (void*)g_glPermaDC, (void*)docHdc_, (void*)g_glMainDC);
    HCreateLogData("error.log", buf);

    // Use g_glMainDC as primary - it's the DC where the context was originally
    // created and SetPixelFormat was called. docHdc_ can be invalidated by
    // Win32 message processing (WM_NCCREATE etc.) during CreateWindowEx calls
    // for other windows in the same thread.
    HDC dc = g_glMainDC ? g_glMainDC : docHdc_;
    if (!dc) {
        HCreateLogData("error.log", "EnsureContext: no DC available");
        return false;
    }

    BOOL ok = wglMakeCurrent(dc, g_glContext);
    if (!ok) {
        sprintf_s(buf, "EnsureContext: wglMakeCurrent failed GLE=%lu dc=%p ctx=%p",
                  GetLastError(), (void*)dc, (void*)g_glContext);
        HCreateLogData("error.log", buf);
    }
    return ok == TRUE;
}

HRESULT OpenGLBackend::LoadExtensions() {
#define LOAD(fn, T) if (!LoadProc(fn, #fn)) { HCreateLogData("error.log", "LoadExtensions FAILED: " #fn); return E_FAIL; }

    LOAD(glGenFramebuffers,          PFNGLGENFRAMEBUFFERSPROC)
    LOAD(glBindFramebuffer,          PFNGLBINDFRAMEBUFFERPROC)
    LOAD(glFramebufferTexture2D,     PFNGLFRAMEBUFFERTEXTURE2DPROC)
    LOAD(glCheckFramebufferStatus,   PFNGLCHECKFRAMEBUFFERSTATUSPROC)
    LOAD(glDeleteFramebuffers,       PFNGLDELETEFRAMEBUFFERSPROC)
    LOAD(glGenRenderbuffers,         PFNGLGENRENDERBUFFERSPROC)
    LOAD(glBindRenderbuffer,         PFNGLBINDRENDERBUFFERPROC)
    LOAD(glRenderbufferStorage,      PFNGLRENDERBUFFERSTORAGEPROC)
    LOAD(glFramebufferRenderbuffer,  PFNGLFRAMEBUFFERRENDERBUFFERPROC)
    LOAD(glDeleteRenderbuffers,      PFNGLDELETERENDERBUFFERSPROC)

    LOAD(glCreateShader,             PFNGLCREATESHADERPROC)
    LOAD(glShaderSource,             PFNGLSHADERSOURCEPROC)
    LOAD(glCompileShader,            PFNGLCOMPILESHADERPROC)
    LOAD(glCreateProgram,            PFNGLCREATEPROGRAMPROC)
    LOAD(glAttachShader,             PFNGLATTACHSHADERPROC)
    LOAD(glLinkProgram,              PFNGLLINKPROGRAMPROC)
    LOAD(glUseProgram,               PFNGLUSEPROGRAMPROC)
    LOAD(glDeleteShader,             PFNGLDELETESHADERPROC)
    LOAD(glDeleteProgram,            PFNGLDELETEPROGRAMPROC)
    if (!LoadProc(gl_GetUniformLocation, "glGetUniformLocation")) { HCreateLogData("error.log", "LoadExtensions FAILED: glGetUniformLocation"); return E_FAIL; }
    LOAD(glUniform1i,                PFNGLUNIFORM1IPROC)
    LOAD(glUniform1f,                PFNGLUNIFORM1FPROC)
    LOAD(glUniform2f,                PFNGLUNIFORM2FPROC)
    LOAD(glUniform4f,                PFNGLUNIFORM4FPROC)
    LOAD(glUniformMatrix3x2fv,       PFNGLUNIFORMMATRIX3X2FVPROC)
    LOAD(glUniformMatrix4fv,         PFNGLUNIFORMMATRIX4FVPROC)
    LOAD(glGetShaderiv,              PFNGLGETSHADERIVPROC)
    LOAD(glGetProgramiv,             PFNGLGETPROGRAMIVPROC)
    LOAD(glGetShaderInfoLog,         PFNGLGETSHADERINFOLOGPROC)

    LOAD(glGenVertexArrays,          PFNGLGENVERTEXARRAYSPROC)
    LOAD(glBindVertexArray,          PFNGLBINDVERTEXARRAYPROC)
    LOAD(glDeleteVertexArrays,       PFNGLDELETEVERTEXARRAYSPROC)
    LOAD(glGenBuffers,               PFNGLGENBUFFERSPROC)
    LOAD(glBindBuffer,               PFNGLBINDBUFFERPROC)
    LOAD(glBufferData,               PFNGLBUFFERDATAPROC)
    LOAD(glBufferSubData,            PFNGLBUFFERSUBDATAPROC)
    LOAD(glDeleteBuffers,            PFNGLDELETEBUFFERSPROC)
    LOAD(glEnableVertexAttribArray,  PFNGLENABLEVERTEXATTRIBARRAYPROC)
    LOAD(glVertexAttribPointer,      PFNGLVERTEXATTRIBPOINTERPROC)

    // Optional WGL extensions (don't hard-fail if missing)
    LoadProc(wglSwapIntervalEXT, "wglSwapIntervalEXT");
    LoadProc(glActiveTexture,    "glActiveTexture");
#undef LOAD
    return S_OK;
}

HRESULT OpenGLBackend::BuildShaders() {
    g_glShaderColor = LinkProgram(kColorVertSrc, kColorFragSrc);
    if (!g_glShaderColor) return E_FAIL;

    g_glShaderTex = LinkProgram(kTexVertSrc, kTexFragSrc);
    if (!g_glShaderTex) return E_FAIL;

    {
        char buf[128];
        sprintf_s(buf, "BuildShaders OK: color=%u tex=%u VAO=%u VBO=%u", g_glShaderColor, g_glShaderTex, g_glQuadVAO, g_glQuadVBO);
        HCreateLogData("draw.log", buf);
    }
    return S_OK;
}

HRESULT OpenGLBackend::BuildQuadGeometry() {
    // Allocate a VAO + VBO large enough for our largest vertex upload
    // (ellipse strips can have ~65 * 2 * 4 = 520 floats; we allocate 2048)
    glGenVertexArrays(1, &g_glQuadVAO);
    glBindVertexArray(g_glQuadVAO);

    glGenBuffers(1, &g_glQuadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, g_glQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(2048 * sizeof(float)), nullptr, GL_DYNAMIC_DRAW);

    // layout(location=0): vec2 position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));

    // layout(location=1): vec2 uv
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

    glBindVertexArray(0);
    {
        char buf[128];
        sprintf_s(buf, "BuildQuadGeometry OK: VAO=%u VBO=%u", g_glQuadVAO, g_glQuadVBO);
        HCreateLogData("draw.log", buf);
    }
    return S_OK;
}

HRESULT OpenGLBackend::CreateDocumentFBO(int pixelWidth, int pixelHeight) {
    {
        char buf[64];
        sprintf_s(buf, "CreateDocumentFBO: %dx%d", pixelWidth, pixelHeight);
        HCreateLogData("draw.log", buf);
    }
    if (!EnsureContext()) { HCreateLogData("draw.log", "CreateDocumentFBO: EnsureContext FAILED"); return E_FAIL; }

    // Delete previous resources if any
    if (g_docFBO_)      { glDeleteFramebuffers(1, &g_docFBO_);      g_docFBO_      = 0; }
    if (g_docColorTex_) { glDeleteTextures(1, &g_docColorTex_);     g_docColorTex_ = 0; }
    if (g_docDepthRBO_) { glDeleteRenderbuffers(1, &g_docDepthRBO_);g_docDepthRBO_ = 0; }

    // Color texture
    glGenTextures(1, &g_docColorTex_);
    glBindTexture(GL_TEXTURE_2D, g_docColorTex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pixelWidth, pixelHeight,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // Depth renderbuffer
    glGenRenderbuffers(1, &g_docDepthRBO_);
    glBindRenderbuffer(GL_RENDERBUFFER, g_docDepthRBO_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, pixelWidth, pixelHeight);

    // FBO
    glGenFramebuffers(1, &g_docFBO_);
    glBindFramebuffer(GL_FRAMEBUFFER, g_docFBO_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, g_docColorTex_, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, g_docDepthRBO_);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        return E_FAIL;
    }

    docPixelWidth_  = pixelWidth;
    docPixelHeight_ = pixelHeight;
    return S_OK;
}

void OpenGLBackend::RebuildDocumentSurface() {
    SizeU sz{ static_cast<uint32_t>(docPixelWidth_), static_cast<uint32_t>(docPixelHeight_) };
    {
        char buf[128];
        sprintf_s(buf, "RebuildDocumentSurface: %dx%d fbo=%u", docPixelWidth_, docPixelHeight_, g_docFBO_);
        HCreateLogData("draw.log", buf);
    }
    // The document surface renders into the FBO; it does NOT own it (we pass 0
    // as fboID so the destructor skips deletion – we manage it ourselves).
    // We pass fboID_ directly so BeginDraw binds it correctly.
    {
        auto* s = new OpenGLRenderSurface(sz, g_docFBO_, g_docColorTex_, docHdc_, docHwnd_);
        s->isDocumentSurface_ = true;
        documentSurface_.reset(s);
    }
}

// ============================================================
// OpenGLBackend – IGraphicsBackend implementation
// ============================================================

HRESULT OpenGLBackend::InitializeMainWindow(HWND mainWindow) {
    if (g_glContext) return S_OK; // already done

    g_glMainHwnd = mainWindow;
    g_glMainDC   = GetDC(mainWindow);
    if (!g_glMainDC) return E_FAIL;

    // Set a basic pixel format so we can create a legacy context to
    // query wglCreateContextAttribsARB
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = 32;
    pfd.cDepthBits   = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType   = PFD_MAIN_PLANE;

    int fmt = ChoosePixelFormat(g_glMainDC, &pfd);
    if (!SetPixelFormat(g_glMainDC, fmt, &pfd)) return E_FAIL;

    // Bootstrap legacy context to load wglCreateContextAttribsARB
    HGLRC tempCtx = wglCreateContext(g_glMainDC);
    if (!tempCtx) return E_FAIL;
    wglMakeCurrent(g_glMainDC, tempCtx);

    LoadProc(wglCreateContextAttribsARB, "wglCreateContextAttribsARB");

    if (wglCreateContextAttribsARB) {
        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        HGLRC coreCtx = wglCreateContextAttribsARB(g_glMainDC, nullptr, attribs);
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(tempCtx);
        if (!coreCtx) return E_FAIL;
        wglMakeCurrent(g_glMainDC, coreCtx);
        g_glContext = coreCtx;
    } else {
        // Fall back to whatever the driver gave us
        g_glContext = tempCtx;
    }

    {
        char buf[128];
        sprintf_s(buf, "InitializeMainWindow: g_glContext=%p current=%p", (void*)g_glContext, (void*)wglGetCurrentContext());
        HCreateLogData("error.log", buf);
    }

    HRESULT hr = LoadExtensions();
    if (FAILED(hr)) return hr;

    hr = BuildShaders();
    if (FAILED(hr)) return hr;

    return BuildQuadGeometry();
    // Context intentionally left current on this thread after init.
}

HRESULT OpenGLBackend::InitializeDocument(HWND documentWindow,
                                          int width, int height,
                                          int pixelSizeRatio,
                                          int /*buttonWidth*/, int /*buttonHeight*/)
{
    if (!IsWindow(documentWindow)) return E_INVALIDARG;

    docHwnd_ = documentWindow;
    docHdc_  = nullptr;

    // The host passes -1 for width/height/ratio when it wants the backend to
    // determine the size from the window. Match D2D behaviour: always use the
    // actual client rect of the document window as the FBO dimensions.
    RECT rc{};
    GetClientRect(documentWindow, &rc);
    int pixelW = rc.right  - rc.left;
    int pixelH = rc.bottom - rc.top;
    if (pixelW <= 0) pixelW = (width  > 0) ? width  : 512;
    if (pixelH <= 0) pixelH = (height > 0) ? height : 512;

    {
        char buf[128];
        sprintf_s(buf, "InitializeDocument: hwnd=%p clientRect=%dx%d (params w=%d h=%d ratio=%d)",
            (void*)documentWindow, pixelW, pixelH, width, height, pixelSizeRatio);
        HCreateLogData("draw.log", buf);
    }

    HRESULT hr = CreateDocumentFBO(pixelW, pixelH);
    if (FAILED(hr)) return hr;

    RebuildDocumentSurface();
    return S_OK;
}

HRESULT OpenGLBackend::InitializeText() {
    // Text is rasterized through GDI per glyph in DrawText.
    // Nothing to initialise globally.
    return S_OK;
}

void OpenGLBackend::ResizeDocument(int width, int height, float zoomFactor) {
    if (!EnsureContext()) return;
    int pixelW = static_cast<int>(std::ceil(width  * zoomFactor));
    int pixelH = static_cast<int>(std::ceil(height * zoomFactor));
    if (pixelW <= 0 || pixelH <= 0) return;

    documentSurface_.reset();
    if (FAILED(CreateDocumentFBO(pixelW, pixelH))) return;
    RebuildDocumentSurface();
}

RenderData OpenGLBackend::CreateWindowRenderData(HWND windowHandle) {
    // Window surfaces (layer buttons, etc.) use an off-screen FBO exactly like
    // bitmap surfaces. On Present(), the FBO is blitted to the window via GDI
    // (glReadPixels -> SetDIBitsToDevice). This avoids the need for each window
    // to have its own GL pixel format and wglMakeCurrent, which is unreliable
    // with a shared single context.
    if (!EnsureContext()) return {};

    RECT rc{};
    GetClientRect(windowHandle, &rc);
    SizeU size{
        static_cast<uint32_t>(rc.right  - rc.left ? rc.right  - rc.left : 1),
        static_cast<uint32_t>(rc.bottom - rc.top  ? rc.bottom - rc.top  : 1)
    };

    RenderData rd = CreateBitmapRenderData(size);
    if (!rd.surfaceHandle) return {};

    // Store the window handle so Present() can blit to it
    auto* surf = static_cast<OpenGLRenderSurface*>(rd.surfaceHandle.get());
    surf->hwnd_ = windowHandle;
    // hdc_ stays null - we use GDI blit in Present() instead of SwapBuffers
    return rd;
}

RenderData OpenGLBackend::CreateBitmapRenderData(const SizeU& size) {
    // Ensure GL context is current - callers may come from any thread/moment
    if (!EnsureContext()) {
        HCreateLogData("error.log", "CreateBitmapRenderData: EnsureContext() failed");
        return {};
    }

    {
        char buf[128];
        sprintf_s(buf, "CreateBitmapRenderData: size=%ux%u g_glContext=%p current=%p", size.width, size.height, (void*)g_glContext, (void*)wglGetCurrentContext());
        HCreateLogData("error.log", buf);
    }

    // Off-screen FBO + texture
    GLuint fbo = 0, tex = 0, rbo = 0;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 static_cast<GLsizei>(size.width), static_cast<GLsizei>(size.height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                          static_cast<GLsizei>(size.width), static_cast<GLsizei>(size.height));

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    {
        char buf[128];
        sprintf_s(buf, "CreateBitmapRenderData: tex=%u fbo=%u rbo=%u status=0x%X", tex, fbo, rbo, status);
        HCreateLogData("error.log", buf);
    }

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        HCreateLogData("error.log", "CreateBitmapRenderData: framebuffer incomplete - returning empty");
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);
        glDeleteRenderbuffers(1, &rbo);
        return {};
    }

    RenderData rd;
    rd.size          = size;
    rd.surfaceHandle = std::make_shared<OpenGLRenderSurface>(size, fbo, tex, nullptr, nullptr);
    rd.bitmapHandle  = std::make_shared<OpenGLBitmapSurface>(size, tex);
    return rd;
}

RenderSurfacePtr OpenGLBackend::GetDocumentSurface() {
    if (!documentSurface_ && g_docFBO_) {
        RebuildDocumentSurface();
    }
    return documentSurface_;
}

std::vector<COLORREF> OpenGLBackend::ReadDocumentPixels(int logicalWidth, int logicalHeight) {
    if (logicalWidth <= 0 || logicalHeight <= 0) return {};

    // Match D2D behaviour: read from the current layer's FBO (not the composed document).
    // The bucket tool does flood-fill on the layer pixels, same as D2D reads layer bitmap.
    GLuint readFBO = 0;
    auto layerIt = std::find_if(layers.begin(), layers.end(),
        [](const std::optional<Layer>& ol) {
            return ol.has_value()
                && ol->LayerID   == layerIndex
                && ol->FrameIndex == CurrentFrameIndex;
        });
    if (layerIt != layers.end() && layerIt->has_value() && layerIt->value().surfaceHandle) {
        auto* glSurf = dynamic_cast<OpenGLRenderSurface*>(layerIt->value().surfaceHandle.get());
        if (glSurf) readFBO = glSurf->GetFBO();
    }
    // Fallback to document FBO if layer not found
    if (!readFBO) readFBO = g_docFBO_;
    if (!readFBO) return {};

    glBindFramebuffer(GL_FRAMEBUFFER, readFBO);

    std::vector<uint8_t> rawPixels(static_cast<size_t>(logicalWidth) * logicalHeight * 4);
    // glReadPixels origin is bottom-left; our ortho is y-down so row 0 = visual bottom.
    // We flip to produce a top-down pixel array matching D2D's output.
    glReadPixels(0, 0, logicalWidth, logicalHeight, GL_RGBA, GL_UNSIGNED_BYTE, rawPixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    std::vector<COLORREF> pixels(static_cast<size_t>(logicalWidth) * logicalHeight);
    for (int row = 0; row < logicalHeight; ++row) {
        int glRow = logicalHeight - 1 - row; // flip: GL bottom-up -> logical top-down
        for (int col = 0; col < logicalWidth; ++col) {
            size_t srcIdx = (static_cast<size_t>(glRow) * logicalWidth + col) * 4;
            size_t dstIdx =  static_cast<size_t>(row)   * logicalWidth + col;
            // Composite over white to match D2D (which draws layer over white background)
            uint8_t r = rawPixels[srcIdx + 0];
            uint8_t g = rawPixels[srcIdx + 1];
            uint8_t b = rawPixels[srcIdx + 2];
            uint8_t a = rawPixels[srcIdx + 3];
            // Blend over white: out = src * alpha + white * (1-alpha)
            r = static_cast<uint8_t>(r * a / 255 + 255 * (255 - a) / 255);
            g = static_cast<uint8_t>(g * a / 255 + 255 * (255 - a) / 255);
            b = static_cast<uint8_t>(b * a / 255 + 255 * (255 - a) / 255);
            pixels[dstIdx] = RGB(r, g, b);
        }
    }
    return pixels;
}

SizeF OpenGLBackend::MeasureText(const std::wstring& text, const FontDesc& font,
                                  const RectF& layoutRect)
{
    if (text.empty()) return {};

    // Use GDI to measure – consistent with the GDI-based rasterizer in DrawText
    HDC hdc = CreateCompatibleDC(nullptr);
    if (!hdc) return {};

    LOGFONTW lf = {};
    lf.lfHeight  = static_cast<LONG>(font.size <= 0 ? 120 : font.size) / 10;
    lf.lfWeight  = font.weight == 0 ? FW_NORMAL : font.weight;
    lf.lfItalic  = font.italic ? TRUE : FALSE;
    wcscpy_s(lf.lfFaceName, font.family.empty() ? L"Arial" : font.family.c_str());

    HFONT hFont = CreateFontIndirectW(&lf);
    if (!hFont) { DeleteDC(hdc); return {}; }
    HFONT hOld = static_cast<HFONT>(SelectObject(hdc, hFont));

    float maxWidth = layoutRect.right - layoutRect.left;
    float totalH   = 0.0f;
    float maxW     = 0.0f;
    float lineW    = 0.0f;
    int   lineH    = static_cast<int>(lf.lfHeight);

    for (wchar_t ch : text) {
        if (ch == L'\n') {
            if (lineW > maxW) maxW = lineW;
            lineW  = 0.0f;
            totalH += lineH;
            continue;
        }
        SIZE sz = {};
        wchar_t str[2] = { ch, 0 };
        GetTextExtentPoint32W(hdc, str, 1, &sz);
        lineW += static_cast<float>(sz.cx);
        if (lineH < sz.cy) lineH = sz.cy;

        if (lineW > maxWidth) {
            if (lineW > maxW) maxW = lineW;
            lineW  = 0.0f;
            totalH += lineH;
        }
    }
    if (lineW > maxW) maxW = lineW;
    totalH += lineH;

    SelectObject(hdc, hOld);
    DeleteObject(hFont);
    DeleteDC(hdc);

    return SizeF{ maxW, totalH };
}

// Blit the off-screen document FBO onto the window's default framebuffer.
// This is the OpenGL equivalent of the D2D swap-chain Present: the FBO holds
// all rendered content but is invisible until copied to the back buffer.
void OpenGLBackend::BlitDocumentToWindow() {
    if (!g_docFBO_ || !g_docColorTex_ || !docHwnd_) return;
    if (!g_glShaderTex || !g_glQuadVAO || !g_glQuadVBO) return;
    if (!glUniformMatrix4fv || !glUniformMatrix3x2fv) return;

    // Ensure context is current on the document DC
    EnsureContextCurrent(g_glMainDC);

    // Get actual window client size for the viewport
    RECT rc{};
    GetClientRect(docHwnd_, &rc);
    int winW = rc.right  - rc.left;
    int winH = rc.bottom - rc.top;
    if (winW <= 0 || winH <= 0) return;

    // Render to the default framebuffer (the window)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, winW, winH);

    // Clear to a neutral background so letterbox areas are visible
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Build an ortho matrix for the window size (y-down, matching D2D convention)
    float ortho[16];
    MakeOrtho(ortho, static_cast<float>(winW), static_cast<float>(winH));

    // Identity transform — we draw the FBO texture filling the whole window
    float xform[6] = { 1,0, 0,1, 0,0 }; // col0=(1,0) col1=(0,1) col2=(0,0)

    // Upload a full-window quad.
    // FBO was rendered y-down (same as D2D), but OpenGL textures are y-up by default.
    // Flip V coords (v0=1, v1=0) to correct the orientation.
    float dw = static_cast<float>(winW);
    float dh = static_cast<float>(winH);
    float verts[] = {
        0,  0,   0.f, 1.f,   // top-left     → UV (0,1) = FBO bottom (visually top)
        dw, 0,   1.f, 1.f,   // top-right
        dw, dh,  1.f, 0.f,   // bottom-right → UV (1,0) = FBO top (visually bottom)

        0,  0,   0.f, 1.f,
        dw, dh,  1.f, 0.f,
        0,  dh,  0.f, 0.f,
    };

    glBindVertexArray(g_glQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_glQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(verts)), verts, GL_DYNAMIC_DRAW);

    glUseProgram(g_glShaderTex);
    {
        GLint l;
        l = glGetUniformLocation(g_glShaderTex, "uOrtho");
        if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, ortho);
        l = glGetUniformLocation(g_glShaderTex, "uTransform");
        if (l >= 0) glUniformMatrix3x2fv(l, 1, GL_FALSE, xform);
        l = glGetUniformLocation(g_glShaderTex, "uColor");
        if (l >= 0) glUniform4f(l, 1.f, 1.f, 1.f, 1.f);
        l = glGetUniformLocation(g_glShaderTex, "uOpacity");
        if (l >= 0) glUniform1f(l, 1.f);
        l = glGetUniformLocation(g_glShaderTex, "uGlyphMode");
        if (l >= 0) glUniform1i(l, 0);  // normal RGBA, not glyph
    }

    if (glActiveTexture) glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_docColorTex_);
    {
        GLint l = glGetUniformLocation(g_glShaderTex, "uTex");
        if (l >= 0) glUniform1i(l, 0);
    }

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glUseProgram(0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGLBackend::PresentDocument(UINT syncInterval) {
    // documentSurface_->Present() does glReadPixels + StretchDIBits to docHwnd_
    if (documentSurface_) {
        documentSurface_->Present(syncInterval);
    }
}

void OpenGLBackend::Cleanup() {
    documentSurface_.reset();

    if (g_docFBO_)      { glDeleteFramebuffers(1,  &g_docFBO_);       g_docFBO_      = 0; }
    if (g_docColorTex_) { glDeleteTextures(1,      &g_docColorTex_);  g_docColorTex_ = 0; }
    if (g_docDepthRBO_) { glDeleteRenderbuffers(1, &g_docDepthRBO_);  g_docDepthRBO_ = 0; }

    if (g_glShaderColor) { glDeleteProgram(g_glShaderColor); g_glShaderColor = 0; }
    if (g_glShaderTex)   { glDeleteProgram(g_glShaderTex);   g_glShaderTex   = 0; }
    if (g_glQuadVAO)     { glDeleteVertexArrays(1, &g_glQuadVAO); g_glQuadVAO = 0; }
    if (g_glQuadVBO)     { glDeleteBuffers(1, &g_glQuadVBO);      g_glQuadVBO = 0; }

    if (g_glContext) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(g_glContext);
        g_glContext = nullptr;
    }
    if (g_glPermaDC) {
        DeleteDC(g_glPermaDC);
        g_glPermaDC = nullptr;
    }
    if (g_glMainDC && g_glMainHwnd) {
        ReleaseDC(g_glMainHwnd, g_glMainDC);
        g_glMainDC = nullptr;
    }
    // docHdc_ is no longer used (GDI blit uses GetDC/ReleaseDC per Present call)
}