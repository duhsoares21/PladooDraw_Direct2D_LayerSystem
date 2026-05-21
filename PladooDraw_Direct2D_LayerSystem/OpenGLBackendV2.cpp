// OpenGLBackendV2.cpp
// OpenGL backend (from scratch) implementing IGraphicsBackend.

#include "OpenGLBackendV2.h"
#include "Constants.h"
#include "Layers.h"

#include <cassert>
#include <cmath>
#include <cstring>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace {
    static const char* kColorVertSrc = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;

uniform mat4 uOrtho;
uniform mat3x2 uTransform; // columns: (m11,m12), (m21,m22), (dx,dy)

void main() {
    vec2 transformed = mat2(uTransform[0].xy, uTransform[1].xy) * aPos
                       + vec2(uTransform[2].x, uTransform[2].y);
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
uniform vec4      uColor;
uniform float     uOpacity;
uniform int       uGlyphMode; // 1 = use red channel as alpha (text)
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

    template<typename T>
    static bool LoadProc(T& fnPtr, const char* name) {
        PROC raw = wglGetProcAddress(name);
        if (!raw) {
            static HMODULE hGL = GetModuleHandleA("opengl32.dll");
            if (hGL) {
                raw = reinterpret_cast<PROC>(GetProcAddress(hGL, name));
            }
        }
        fnPtr = reinterpret_cast<T>(reinterpret_cast<void*>(raw));
        return fnPtr != nullptr;
    }

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
        GLuint vs = CompileShader(GL_VERTEX_SHADER, vertSrc);
        GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragSrc);
        if (!vs || !fs) return 0;

        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        GLint ok = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        glDeleteShader(vs);
        glDeleteShader(fs);
        if (!ok) {
            glDeleteProgram(program);
            return 0;
        }
        return program;
    }

    static void MakeOrtho(float out[16], float w, float h) {
        std::memset(out, 0, sizeof(float) * 16);
        out[0] = 2.0f / w;
        out[5] = -2.0f / h;
        out[10] = -1.0f;
        out[12] = -1.0f;
        out[13] = 1.0f;
        out[15] = 1.0f;
    }

    static void PackTransform(float out[6], const Matrix3x2& t) {
        out[0] = t.m11; out[1] = t.m12;
        out[2] = t.m21; out[3] = t.m22;
        out[4] = t.dx;  out[5] = t.dy;
    }
}

OpenGLBitmapSurfaceV2::OpenGLBitmapSurfaceV2(SizeU surfaceSize, GLuint textureId)
    : size_(surfaceSize), textureId_(textureId) {
}

OpenGLBitmapSurfaceV2::~OpenGLBitmapSurfaceV2() {
    if (textureId_ != 0) {
        glDeleteTextures(1, &textureId_);
    }
}

SizeU OpenGLBitmapSurfaceV2::GetSize() const {
    return size_;
}

GLuint OpenGLBitmapSurfaceV2::GetTextureId() const {
    return textureId_;
}

OpenGLRenderSurfaceV2::OpenGLRenderSurfaceV2(SizeU surfaceSize, GLuint fbo, GLuint colorTexture, GLuint depthRbo, HDC hdc, HWND hwnd, bool releaseHdc)
    : size_(surfaceSize),
      fbo_(fbo),
      colorTexture_(colorTexture),
      depthRbo_(depthRbo),
      hdc_(hdc),
      hwnd_(hwnd),
      releaseHdc_(releaseHdc),
      currentTransform_(MakeIdentityMatrix3x2()) {
}

OpenGLRenderSurfaceV2::~OpenGLRenderSurfaceV2() {
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
    }
    if (depthRbo_ != 0) {
        glDeleteRenderbuffers(1, &depthRbo_);
    }
}

SizeU OpenGLRenderSurfaceV2::GetSize() const {
    return size_;
}

void OpenGLRenderSurfaceV2::BeginDraw() {
    BindTarget();
    glViewport(0, 0, static_cast<GLsizei>(size_.width), static_cast<GLsizei>(size_.height));

    if (blendCopy_) {
        glDisable(GL_BLEND);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    if (!clipStack_.empty()) {
        glEnable(GL_SCISSOR_TEST);
        ApplyClip();
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

HRESULT OpenGLRenderSurfaceV2::EndDraw() {
    return S_OK;
}

void OpenGLRenderSurfaceV2::Clear(const ColorRGBA& color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void OpenGLRenderSurfaceV2::SetTransform(const Matrix3x2& transform) {
    currentTransform_ = transform;
}

PointF OpenGLRenderSurfaceV2::TransformPoint(const PointF& point) const {
    return PointF{
        currentTransform_.m11 * point.x + currentTransform_.m21 * point.y + currentTransform_.dx,
        currentTransform_.m12 * point.x + currentTransform_.m22 * point.y + currentTransform_.dy
    };
}

void OpenGLRenderSurfaceV2::PushClip(const RectF& clipRect) {
    PointF p1 = TransformPoint(PointF{ clipRect.left, clipRect.top });
    PointF p2 = TransformPoint(PointF{ clipRect.right, clipRect.top });
    PointF p3 = TransformPoint(PointF{ clipRect.right, clipRect.bottom });
    PointF p4 = TransformPoint(PointF{ clipRect.left, clipRect.bottom });

    float minX = std::min(std::min(p1.x, p2.x), std::min(p3.x, p4.x));
    float maxX = std::max(std::max(p1.x, p2.x), std::max(p3.x, p4.x));
    float minY = std::min(std::min(p1.y, p2.y), std::min(p3.y, p4.y));
    float maxY = std::max(std::max(p1.y, p2.y), std::max(p3.y, p4.y));

    int x = static_cast<int>(std::floor(minX));
    int y = static_cast<int>(std::floor(static_cast<float>(size_.height) - maxY));
    int w = static_cast<int>(std::ceil(maxX - minX));
    int h = static_cast<int>(std::ceil(maxY - minY));

    if (w < 0) w = 0;
    if (h < 0) h = 0;

    clipStack_.push_back(ClipRect{ x, y, w, h });
    glEnable(GL_SCISSOR_TEST);
    ApplyClip();
}

void OpenGLRenderSurfaceV2::PopClip() {
    if (!clipStack_.empty()) {
        clipStack_.pop_back();
    }
    if (clipStack_.empty()) {
        glDisable(GL_SCISSOR_TEST);
    } else {
        ApplyClip();
    }
}

void OpenGLRenderSurfaceV2::ApplyClip() {
    if (clipStack_.empty()) return;
    const ClipRect& c = clipStack_.back();
    glScissor(c.x, c.y, c.w, c.h);
}

void OpenGLRenderSurfaceV2::UseColorProgram(const ColorRGBA& color, float opacity) {
    if (!g_glProgramColor.program) return;
    glUseProgram(g_glProgramColor.program);
    float ortho[16];
    MakeOrtho(ortho, static_cast<float>(size_.width), static_cast<float>(size_.height));
    float transform[6];
    PackTransform(transform, currentTransform_);
    glUniformMatrix4fv(g_glProgramColor.uOrtho, 1, GL_FALSE, ortho);
    glUniformMatrix3x2fv(g_glProgramColor.uTransform, 1, GL_FALSE, transform);
    glUniform4f(g_glProgramColor.uColor, color.r, color.g, color.b, color.a);
    glUniform1f(g_glProgramColor.uOpacity, opacity);
}

void OpenGLRenderSurfaceV2::UseTexProgram(const ColorRGBA& color, float opacity, bool glyphMode) {
    if (!g_glProgramTex.program) return;
    glUseProgram(g_glProgramTex.program);
    float ortho[16];
    MakeOrtho(ortho, static_cast<float>(size_.width), static_cast<float>(size_.height));
    float transform[6];
    PackTransform(transform, currentTransform_);
    glUniformMatrix4fv(g_glProgramTex.uOrtho, 1, GL_FALSE, ortho);
    glUniformMatrix3x2fv(g_glProgramTex.uTransform, 1, GL_FALSE, transform);
    glUniform4f(g_glProgramTex.uColor, color.r, color.g, color.b, color.a);
    glUniform1f(g_glProgramTex.uOpacity, opacity);
    glUniform1i(g_glProgramTex.uGlyphMode, glyphMode ? 1 : 0);
    glUniform1i(g_glProgramTex.uTex, 0);
}

void OpenGLRenderSurfaceV2::UploadQuad(const RectF& rect, bool withUv, float u0, float v0, float u1, float v1) {
    if (!g_glQuadVBO || !g_glQuadVAO) return;
    glBindVertexArray(g_glQuadVAO);
    float x = rect.left;
    float y = rect.top;
    float w = rect.right - rect.left;
    float h = rect.bottom - rect.top;
    float verts[] = {
        x,     y,      u0, v0,
        x + w, y,      u1, v0,
        x + w, y + h,  u1, v1,
        x,     y,      u0, v0,
        x + w, y + h,  u1, v1,
        x,     y + h,  u0, v1
    };
    glBindBuffer(GL_ARRAY_BUFFER, g_glQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

    if (!withUv) {
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);
    } else {
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }
}

void OpenGLRenderSurfaceV2::FillRect(const RectF& rect, const ColorRGBA& color) {
    UseColorProgram(color, 1.0f);
    UploadQuad(rect, false);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void OpenGLRenderSurfaceV2::StrokeRect(const RectF& rect, const ColorRGBA& color, float strokeWidth) {
    RectF top{ rect.left, rect.top, rect.right, rect.top + strokeWidth };
    RectF bottom{ rect.left, rect.bottom - strokeWidth, rect.right, rect.bottom };
    RectF left{ rect.left, rect.top, rect.left + strokeWidth, rect.bottom };
    RectF right{ rect.right - strokeWidth, rect.top, rect.right, rect.bottom };
    FillRect(top, color);
    FillRect(bottom, color);
    FillRect(left, color);
    FillRect(right, color);
}

void OpenGLRenderSurfaceV2::DrawEllipseInternal(const EllipseF& ellipse, const ColorRGBA& color, bool fill, float strokeWidth) {
    UseColorProgram(color, 1.0f);
    const int segments = 64;
    std::vector<float> verts;
    if (fill) {
        verts.reserve((segments + 2) * 2);
        verts.push_back(ellipse.point.x);
        verts.push_back(ellipse.point.y);
        for (int i = 0; i <= segments; ++i) {
            float theta = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * static_cast<float>(M_PI);
            float x = ellipse.point.x + std::cos(theta) * ellipse.radiusX;
            float y = ellipse.point.y + std::sin(theta) * ellipse.radiusY;
            verts.push_back(x);
            verts.push_back(y);
        }
        glBindVertexArray(g_glQuadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, g_glQuadVBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(verts.size() / 2));
    } else {
        glLineWidth(strokeWidth);
        verts.reserve((segments + 1) * 2);
        for (int i = 0; i <= segments; ++i) {
            float theta = static_cast<float>(i) / static_cast<float>(segments) * 2.0f * static_cast<float>(M_PI);
            float x = ellipse.point.x + std::cos(theta) * ellipse.radiusX;
            float y = ellipse.point.y + std::sin(theta) * ellipse.radiusY;
            verts.push_back(x);
            verts.push_back(y);
        }
        glBindVertexArray(g_glQuadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, g_glQuadVBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(verts.size() / 2));
    }
}

void OpenGLRenderSurfaceV2::FillEllipse(const EllipseF& ellipse, const ColorRGBA& color) {
    DrawEllipseInternal(ellipse, color, true, 1.0f);
}

void OpenGLRenderSurfaceV2::StrokeEllipse(const EllipseF& ellipse, const ColorRGBA& color, float strokeWidth) {
    DrawEllipseInternal(ellipse, color, false, strokeWidth);
}

void OpenGLRenderSurfaceV2::DrawLine(const PointF& start, const PointF& end, const ColorRGBA& color, float strokeWidth) {
    UseColorProgram(color, 1.0f);

    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f) {
        float half = strokeWidth * 0.5f;
        RectF dot = MakeRectF(start.x - half, start.y - half, start.x + half, start.y + half);
        FillRect(dot, color);
        return;
    }

    float nx = -dy / len * (strokeWidth * 0.5f);
    float ny =  dx / len * (strokeWidth * 0.5f);

    float verts[] = {
        start.x + nx, start.y + ny,
        start.x - nx, start.y - ny,
        end.x   + nx, end.y   + ny,
        start.x - nx, start.y - ny,
        end.x   - nx, end.y   - ny,
        end.x   + nx, end.y   + ny
    };

    glBindVertexArray(g_glQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_glQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void OpenGLRenderSurfaceV2::DrawBitmap(IBitmapSurface& bitmap, const RectF* destination, float opacity) {
    auto* glBitmap = dynamic_cast<OpenGLBitmapSurfaceV2*>(&bitmap);
    if (!glBitmap) return;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glBitmap->GetTextureId());

    RectF destRect{};
    if (destination) {
        destRect = *destination;
    } else {
        SizeU size = glBitmap->GetSize();
        destRect = MakeRectF(0.0f, 0.0f, static_cast<float>(size.width), static_cast<float>(size.height));
    }

    UseTexProgram(ColorRGBA{ 1.0f, 1.0f, 1.0f, 1.0f }, opacity, false);
    // Flip V so FBO-rendered textures appear with top-left origin.
    UploadQuad(destRect, true, 0.0f, 1.0f, 1.0f, 0.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void OpenGLRenderSurfaceV2::DrawText(const std::wstring& text, const FontDesc& font, const RectF& layoutRect, const ColorRGBA& color) {
    if (text.empty()) return;

    int fontSize = font.size > 0 ? font.size : 12;
    int pixelSize = fontSize / 10;
    if (pixelSize <= 0) pixelSize = 12;

    int width = static_cast<int>(layoutRect.right - layoutRect.left);
    int height = static_cast<int>(layoutRect.bottom - layoutRect.top);
    if (width <= 0 || height <= 0) return;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC memdc = CreateCompatibleDC(nullptr);
    HBITMAP dib = CreateDIBSection(memdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
        if (dib) DeleteObject(dib);
        DeleteDC(memdc);
        return;
    }

    HGDIOBJ oldBitmap = SelectObject(memdc, dib);
    RECT rc{ 0, 0, width, height };
    HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
    ::FillRect(memdc, &rc, bg);
    DeleteObject(bg);

    int weight = font.weight <= 0 ? FW_NORMAL : font.weight;
    HFONT hfont = CreateFontW(
        -pixelSize, 0, 0, 0,
        weight,
        font.italic ? TRUE : FALSE,
        font.underline ? TRUE : FALSE,
        font.strike ? TRUE : FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        font.family.empty() ? L"Arial" : font.family.c_str()
    );
    HGDIOBJ oldFont = SelectObject(memdc, hfont);
    SetBkMode(memdc, OPAQUE);
    SetBkColor(memdc, RGB(0, 0, 0));
    SetTextColor(memdc, RGB(255, 255, 255));
    ::DrawTextW(memdc, text.c_str(), static_cast<int>(text.size()), &rc, DT_LEFT | DT_TOP | DT_NOPREFIX);

    glActiveTexture(GL_TEXTURE0);
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, bits);

    UseTexProgram(color, 1.0f, true);
    RectF dest = layoutRect;
    UploadQuad(dest, true);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDeleteTextures(1, &tex);

    SelectObject(memdc, oldFont);
    SelectObject(memdc, oldBitmap);
    DeleteObject(hfont);
    DeleteObject(dib);
    DeleteDC(memdc);
}

void OpenGLRenderSurfaceV2::SetPrimitiveBlendCopy(bool enabled) {
    blendCopy_ = enabled;
}

void OpenGLRenderSurfaceV2::SetAliased(bool enabled) {
    antialiased_ = enabled;
    if (antialiased_) {
        glDisable(GL_LINE_SMOOTH);
    } else {
        glDisable(GL_LINE_SMOOTH);
    }
}

void OpenGLRenderSurfaceV2::Present(UINT) {
    if (hdc_ && hwnd_) {
        SwapBuffers(hdc_);
        if (releaseHdc_) {
            ReleaseDC(hwnd_, hdc_);
            hdc_ = nullptr;
        }
    }
}

GLuint OpenGLRenderSurfaceV2::GetFbo() const {
    return fbo_;
}

GLuint OpenGLRenderSurfaceV2::GetColorTexture() const {
    return colorTexture_;
}

HDC OpenGLRenderSurfaceV2::GetHdc() const {
    return hdc_;
}

void OpenGLRenderSurfaceV2::BindTarget() {
    if (hwnd_ && releaseHdc_ && !hdc_) {
        hdc_ = GetDC(hwnd_);
    }

    HDC target = hdc_;
    if (!target && !hwnd_) {
        target = g_glMainDC;
    }

    if (target) {
        wglMakeCurrent(target, g_glContext);
    }

    if (fbo_ != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

HRESULT OpenGLBackendV2::InitializeMainWindow(HWND mainWindow) {
    g_glMainHwnd = mainWindow;
    g_glMainDC = GetDC(mainWindow);
    if (!g_glMainDC) {
        return E_FAIL;
    }

    if (!EnsurePixelFormat(g_glMainDC)) {
        return E_FAIL;
    }

    HGLRC tempContext = wglCreateContext(g_glMainDC);
    if (!tempContext) {
        return E_FAIL;
    }

    if (!wglMakeCurrent(g_glMainDC, tempContext)) {
        wglDeleteContext(tempContext);
        return E_FAIL;
    }

    LoadProc(wglCreateContextAttribsARB, "wglCreateContextAttribsARB");
    LoadProc(wglSwapIntervalEXT, "wglSwapIntervalEXT");

    int attribs[] = {
        0x2091, 3, // WGL_CONTEXT_MAJOR_VERSION_ARB
        0x2092, 3, // WGL_CONTEXT_MINOR_VERSION_ARB
        0x9126, 0x00000001, // WGL_CONTEXT_PROFILE_MASK_ARB, CORE
        0
    };

    if (wglCreateContextAttribsARB) {
        HGLRC modern = wglCreateContextAttribsARB(g_glMainDC, nullptr, attribs);
        if (modern) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(tempContext);
            g_glContext = modern;
            wglMakeCurrent(g_glMainDC, g_glContext);
        } else {
            g_glContext = tempContext;
        }
    } else {
        g_glContext = tempContext;
    }

    if (!LoadExtensions()) {
        return E_FAIL;
    }
    if (!BuildShaders()) {
        return E_FAIL;
    }
    if (!BuildGeometry()) {
        return E_FAIL;
    }

    if (wglSwapIntervalEXT) {
        wglSwapIntervalEXT(0);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return S_OK;
}

HRESULT OpenGLBackendV2::InitializeDocument(HWND documentWindow, int, int, int, int, int) {
    documentHwnd_ = documentWindow;
    documentHdc_ = GetDC(documentWindow);
    if (!documentHdc_) {
        return E_FAIL;
    }
    if (!EnsurePixelFormat(documentHdc_)) {
        return E_FAIL;
    }
    if (!CreateDocumentSurface(documentWindow)) {
        return E_FAIL;
    }
    return S_OK;
}

HRESULT OpenGLBackendV2::InitializeText() {
    return S_OK;
}

void OpenGLBackendV2::ResizeDocument(int, int, float) {
    if (!documentHwnd_) return;
    CreateDocumentSurface(documentHwnd_);
}

RenderData OpenGLBackendV2::CreateWindowRenderData(HWND windowHandle) {
    RECT rc{};
    GetClientRect(windowHandle, &rc);
    SizeU size{
        static_cast<uint32_t>(rc.right - rc.left),
        static_cast<uint32_t>(rc.bottom - rc.top)
    };

    HDC hdc = GetDC(windowHandle);
    if (!hdc) return {};
    if (!EnsurePixelFormat(hdc)) {
        ReleaseDC(windowHandle, hdc);
        return {};
    }
    ReleaseDC(windowHandle, hdc);

    RenderData data;
    data.size = size;
    data.surfaceHandle = std::make_shared<OpenGLRenderSurfaceV2>(size, 0, 0, 0, nullptr, windowHandle, true);
    data.bitmapHandle = nullptr;
    return data;
}

RenderData OpenGLBackendV2::CreateBitmapRenderData(const SizeU& size) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.width, size.height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tex);
        return {};
    }

    RenderData data;
    data.size = size;
    data.surfaceHandle = std::make_shared<OpenGLRenderSurfaceV2>(size, fbo, tex, 0, nullptr, nullptr, false);
    data.bitmapHandle = std::make_shared<OpenGLBitmapSurfaceV2>(size, tex);
    return data;
}

RenderSurfacePtr OpenGLBackendV2::GetDocumentSurface() {
    return documentSurface_;
}

std::vector<COLORREF> OpenGLBackendV2::ReadDocumentPixels(int logicalWidth, int logicalHeight) {
    if (logicalWidth <= 0 || logicalHeight <= 0) {
        return {};
    }

    auto it = std::find_if(
        layers.begin(),
        layers.end(),
        [](const std::optional<Layer>& optLayer) {
            return optLayer.has_value() && optLayer->LayerID == layerIndex && optLayer->FrameIndex == CurrentFrameIndex;
        }
    );
    if (it == layers.end() || !it->has_value()) {
        return {};
    }

    OpenGLRenderSurfaceV2* surface = nullptr;
    if (it->value().surfaceHandle) {
        surface = dynamic_cast<OpenGLRenderSurfaceV2*>(it->value().surfaceHandle.get());
    }
    if (!surface) {
        return {};
    }

    GLuint fbo = surface->GetFbo();
    if (fbo == 0) {
        return {};
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    std::vector<unsigned char> rgba(static_cast<size_t>(logicalWidth) * static_cast<size_t>(logicalHeight) * 4);
    glReadPixels(0, 0, logicalWidth, logicalHeight, GL_BGRA, GL_UNSIGNED_BYTE, rgba.data());

    std::vector<COLORREF> pixels(static_cast<size_t>(logicalWidth) * static_cast<size_t>(logicalHeight));
    for (int y = 0; y < logicalHeight; ++y) {
        for (int x = 0; x < logicalWidth; ++x) {
            int srcIndex = ((logicalHeight - 1 - y) * logicalWidth + x) * 4;
            BYTE b = rgba[srcIndex + 0];
            BYTE g = rgba[srcIndex + 1];
            BYTE r = rgba[srcIndex + 2];
            pixels[static_cast<size_t>(y) * logicalWidth + x] = RGB(r, g, b);
        }
    }
    return pixels;
}

SizeF OpenGLBackendV2::MeasureText(const std::wstring& text, const FontDesc& font, const RectF&) {
    if (text.empty()) {
        return {};
    }

    int fontSize = font.size > 0 ? font.size : 12;
    int pixelSize = fontSize / 10;
    if (pixelSize <= 0) pixelSize = 12;

    HDC hdc = GetDC(nullptr);
    HFONT hfont = CreateFontW(
        -pixelSize, 0, 0, 0,
        font.weight <= 0 ? FW_NORMAL : font.weight,
        font.italic ? TRUE : FALSE,
        font.underline ? TRUE : FALSE,
        font.strike ? TRUE : FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        font.family.empty() ? L"Arial" : font.family.c_str()
    );
    HGDIOBJ oldFont = SelectObject(hdc, hfont);
    SIZE size{};
    GetTextExtentPoint32W(hdc, text.c_str(), static_cast<int>(text.size()), &size);
    SelectObject(hdc, oldFont);
    DeleteObject(hfont);
    ReleaseDC(nullptr, hdc);
    return SizeF{ static_cast<float>(size.cx), static_cast<float>(size.cy) };
}

void OpenGLBackendV2::PresentDocument(UINT) {
    if (documentSurface_) {
        documentSurface_->Present(1);
    }
}

void OpenGLBackendV2::Cleanup() {
    documentSurface_.reset();
    if (g_glProgramColor.program) {
        glDeleteProgram(g_glProgramColor.program);
        g_glProgramColor = {};
    }
    if (g_glProgramTex.program) {
        glDeleteProgram(g_glProgramTex.program);
        g_glProgramTex = {};
    }
    if (g_glQuadVBO) {
        glDeleteBuffers(1, &g_glQuadVBO);
        g_glQuadVBO = 0;
    }
    if (g_glQuadVAO) {
        glDeleteVertexArrays(1, &g_glQuadVAO);
        g_glQuadVAO = 0;
    }
    if (g_glContext) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(g_glContext);
        g_glContext = nullptr;
    }
}

bool OpenGLBackendV2::EnsureContext(HDC targetDc) {
    if (!g_glContext || !targetDc) return false;
    return wglMakeCurrent(targetDc, g_glContext) == TRUE;
}

bool OpenGLBackendV2::EnsurePixelFormat(HDC dc) {
    if (!dc) return false;
    int current = GetPixelFormat(dc);
    if (current != 0) {
        return true;
    }

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(dc, &pfd);
    if (pf == 0) return false;
    return SetPixelFormat(dc, pf, &pfd) == TRUE;
}

bool OpenGLBackendV2::LoadExtensions() {
    bool ok = true;
    ok &= LoadProc(glActiveTexture, "glActiveTexture");
    ok &= LoadProc(glGenFramebuffers, "glGenFramebuffers");
    ok &= LoadProc(glBindFramebuffer, "glBindFramebuffer");
    ok &= LoadProc(glFramebufferTexture2D, "glFramebufferTexture2D");
    ok &= LoadProc(glCheckFramebufferStatus, "glCheckFramebufferStatus");
    ok &= LoadProc(glDeleteFramebuffers, "glDeleteFramebuffers");
    ok &= LoadProc(glGenRenderbuffers, "glGenRenderbuffers");
    ok &= LoadProc(glBindRenderbuffer, "glBindRenderbuffer");
    ok &= LoadProc(glRenderbufferStorage, "glRenderbufferStorage");
    ok &= LoadProc(glFramebufferRenderbuffer, "glFramebufferRenderbuffer");
    ok &= LoadProc(glDeleteRenderbuffers, "glDeleteRenderbuffers");

    ok &= LoadProc(glCreateShader, "glCreateShader");
    ok &= LoadProc(glShaderSource, "glShaderSource");
    ok &= LoadProc(glCompileShader, "glCompileShader");
    ok &= LoadProc(glGetShaderiv, "glGetShaderiv");
    ok &= LoadProc(glGetShaderInfoLog, "glGetShaderInfoLog");
    ok &= LoadProc(glCreateProgram, "glCreateProgram");
    ok &= LoadProc(glAttachShader, "glAttachShader");
    ok &= LoadProc(glLinkProgram, "glLinkProgram");
    ok &= LoadProc(glGetProgramiv, "glGetProgramiv");
    ok &= LoadProc(glUseProgram, "glUseProgram");
    ok &= LoadProc(glDeleteShader, "glDeleteShader");
    ok &= LoadProc(glDeleteProgram, "glDeleteProgram");
    ok &= LoadProc(gl_GetUniformLocation, "glGetUniformLocation");
    ok &= LoadProc(glUniform1i, "glUniform1i");
    ok &= LoadProc(glUniform1f, "glUniform1f");
    ok &= LoadProc(glUniform4f, "glUniform4f");
    ok &= LoadProc(glUniformMatrix3x2fv, "glUniformMatrix3x2fv");
    ok &= LoadProc(glUniformMatrix4fv, "glUniformMatrix4fv");

    ok &= LoadProc(glGenVertexArrays, "glGenVertexArrays");
    ok &= LoadProc(glBindVertexArray, "glBindVertexArray");
    ok &= LoadProc(glDeleteVertexArrays, "glDeleteVertexArrays");
    ok &= LoadProc(glGenBuffers, "glGenBuffers");
    ok &= LoadProc(glBindBuffer, "glBindBuffer");
    ok &= LoadProc(glBufferData, "glBufferData");
    ok &= LoadProc(glBufferSubData, "glBufferSubData");
    ok &= LoadProc(glDeleteBuffers, "glDeleteBuffers");
    ok &= LoadProc(glEnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= LoadProc(glVertexAttribPointer, "glVertexAttribPointer");
    return ok;
}

bool OpenGLBackendV2::BuildShaders() {
    g_glProgramColor.program = LinkProgram(kColorVertSrc, kColorFragSrc);
    if (!g_glProgramColor.program) return false;
    g_glProgramColor.uOrtho = glGetUniformLocation(g_glProgramColor.program, "uOrtho");
    g_glProgramColor.uTransform = glGetUniformLocation(g_glProgramColor.program, "uTransform");
    g_glProgramColor.uColor = glGetUniformLocation(g_glProgramColor.program, "uColor");
    g_glProgramColor.uOpacity = glGetUniformLocation(g_glProgramColor.program, "uOpacity");

    g_glProgramTex.program = LinkProgram(kTexVertSrc, kTexFragSrc);
    if (!g_glProgramTex.program) return false;
    g_glProgramTex.uOrtho = glGetUniformLocation(g_glProgramTex.program, "uOrtho");
    g_glProgramTex.uTransform = glGetUniformLocation(g_glProgramTex.program, "uTransform");
    g_glProgramTex.uColor = glGetUniformLocation(g_glProgramTex.program, "uColor");
    g_glProgramTex.uOpacity = glGetUniformLocation(g_glProgramTex.program, "uOpacity");
    g_glProgramTex.uTex = glGetUniformLocation(g_glProgramTex.program, "uTex");
    g_glProgramTex.uGlyphMode = glGetUniformLocation(g_glProgramTex.program, "uGlyphMode");
    return true;
}

bool OpenGLBackendV2::BuildGeometry() {
    glGenVertexArrays(1, &g_glQuadVAO);
    glGenBuffers(1, &g_glQuadVBO);
    glBindVertexArray(g_glQuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_glQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    return true;
}

bool OpenGLBackendV2::CreateDocumentSurface(HWND documentWindow) {
    RECT rc{};
    GetClientRect(documentWindow, &rc);
    SizeU size{
        static_cast<uint32_t>(rc.right - rc.left),
        static_cast<uint32_t>(rc.bottom - rc.top)
    };

    documentSurface_ = std::make_shared<OpenGLRenderSurfaceV2>(size, 0, 0, 0, documentHdc_, documentWindow, false);
    return true;
}
