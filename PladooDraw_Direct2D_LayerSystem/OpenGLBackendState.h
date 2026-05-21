#pragma once

// OpenGL / WGL (Windows)
#include <windows.h>
#include <GL/gl.h>

// -----------------------------------------------------------------------
// Types missing from the Windows SDK GL/gl.h (OpenGL 1.1 only).
// These are standard OpenGL types defined in glext.h / gl3.h.
// We declare them here so no external headers are needed.
// -----------------------------------------------------------------------
#ifndef GL_VERSION_1_5
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
#endif

#ifndef GL_ARB_shader_objects
typedef char     GLchar;
#endif

// glActiveTexture / GL_TEXTURE0 are OpenGL 1.3 � not in the old SDK header
#ifndef GL_TEXTURE0
#define GL_TEXTURE0  0x84C0
#endif

// glActiveTexture function typedef and pointer
typedef void (APIENTRY* PFNGLACTIVETEXTUREPROC)(GLenum texture);

// -----------------------------------------------------------------------
// OpenGL extension typedefs needed (loaded at runtime via wglGetProcAddress)
// -----------------------------------------------------------------------
// FBO
typedef void  (APIENTRY* PFNGLGENFRAMEBUFFERSPROC)          (GLsizei, GLuint*);
typedef void  (APIENTRY* PFNGLBINDFRAMEBUFFERPROC)          (GLenum, GLuint);
typedef void  (APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DPROC)     (GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum(APIENTRY* PFNGLCHECKFRAMEBUFFERSTATUSPROC)   (GLenum);
typedef void  (APIENTRY* PFNGLDELETEFRAMEBUFFERSPROC)       (GLsizei, const GLuint*);
typedef void  (APIENTRY* PFNGLGENRENDERBUFFERSPROC)         (GLsizei, GLuint*);
typedef void  (APIENTRY* PFNGLBINDRENDERBUFFERPROC)         (GLenum, GLuint);
typedef void  (APIENTRY* PFNGLRENDERBUFFERSTORAGEPROC)      (GLenum, GLenum, GLsizei, GLsizei);
typedef void  (APIENTRY* PFNGLFRAMEBUFFERRENDERBUFFERPROC)  (GLenum, GLenum, GLenum, GLuint);
typedef void  (APIENTRY* PFNGLDELETERENDERBUFFERSPROC)      (GLsizei, const GLuint*);
// Shaders
typedef GLuint(APIENTRY* PFNGLCREATESHADERPROC)             (GLenum);
typedef void  (APIENTRY* PFNGLSHADERSOURCEPROC)             (GLuint, GLsizei, const GLchar**, const GLint*);
typedef void  (APIENTRY* PFNGLCOMPILESHADERPROC)            (GLuint);
typedef GLuint(APIENTRY* PFNGLCREATEPROGRAMPROC)            ();
typedef void  (APIENTRY* PFNGLATTACHSHADERPROC)             (GLuint, GLuint);
typedef void  (APIENTRY* PFNGLLINKPROGRAMPROC)              (GLuint);
typedef void  (APIENTRY* PFNGLUSEPROGRAMPROC)               (GLuint);
typedef void  (APIENTRY* PFNGLDELETESHADERPROC)             (GLuint);
typedef void  (APIENTRY* PFNGLDELETEPROGRAMPROC)            (GLuint);
typedef GLint(APIENTRY* PFNGLGETUNIFORMLOCATIONPROC)       (GLuint, const GLchar*);
typedef void  (APIENTRY* PFNGLUNIFORM1IPROC)                (GLint, GLint);
typedef void  (APIENTRY* PFNGLUNIFORM1FPROC)                (GLint, GLfloat);
typedef void  (APIENTRY* PFNGLUNIFORM2FPROC)                (GLint, GLfloat, GLfloat);
typedef void  (APIENTRY* PFNGLUNIFORM4FPROC)                (GLint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void  (APIENTRY* PFNGLUNIFORMMATRIX3X2FVPROC)       (GLint, GLsizei, GLboolean, const GLfloat*);
typedef void  (APIENTRY* PFNGLUNIFORMMATRIX4FVPROC)         (GLint, GLsizei, GLboolean, const GLfloat*);
typedef void  (APIENTRY* PFNGLGETSHADERIVPROC)              (GLuint, GLenum, GLint*);
typedef void  (APIENTRY* PFNGLGETPROGRAMIVPROC)             (GLuint, GLenum, GLint*);
typedef void  (APIENTRY* PFNGLGETSHADERINFOLOGPROC)         (GLuint, GLsizei, GLsizei*, GLchar*);
// VAO / VBO
typedef void  (APIENTRY* PFNGLGENVERTEXARRAYSPROC)          (GLsizei, GLuint*);
typedef void  (APIENTRY* PFNGLBINDVERTEXARRAYPROC)          (GLuint);
typedef void  (APIENTRY* PFNGLDELETEVERTEXARRAYSPROC)       (GLsizei, const GLuint*);
typedef void  (APIENTRY* PFNGLGENBUFFERSPROC)               (GLsizei, GLuint*);
typedef void  (APIENTRY* PFNGLBINDBUFFERPROC)               (GLenum, GLuint);
typedef void  (APIENTRY* PFNGLBUFFERDATAPROC)               (GLenum, GLsizeiptr, const GLvoid*, GLenum);
typedef void  (APIENTRY* PFNGLBUFFERSUBDATAPROC)            (GLenum, GLintptr, GLsizeiptr, const GLvoid*);
typedef void  (APIENTRY* PFNGLDELETEBUFFERSPROC)            (GLsizei, const GLuint*);
typedef void  (APIENTRY* PFNGLENABLEVERTEXATTRIBARRAYPROC)  (GLuint);
typedef void  (APIENTRY* PFNGLVERTEXATTRIBPOINTERPROC)      (GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid*);
// WGL extension for modern context creation
typedef HGLRC(APIENTRY* PFNWGLCREATECONTEXTATTRIBSARBPROC)  (HDC, HGLRC, const int*);
typedef BOOL(APIENTRY* PFNWGLSWAPINTERVALEXTPROC)          (int);

// ---------------------------------------------------------
// Global OpenGL state (analogous to Direct2DBackendState.h)
// ---------------------------------------------------------

// WGL / Win32
extern HDC   g_glMainDC;
extern HDC   g_glPermaDC;  // persistent memory DC for stable wglMakeCurrent
extern HDC   g_glDocDC;    // DC of the document window, used for SwapBuffers
extern HGLRC g_glContext;

// Extension function pointers
extern PFNGLGENFRAMEBUFFERSPROC          glGenFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC          glBindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC     glFramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC   glCheckFramebufferStatus;
extern PFNGLDELETEFRAMEBUFFERSPROC       glDeleteFramebuffers;
extern PFNGLGENRENDERBUFFERSPROC         glGenRenderbuffers;
extern PFNGLBINDRENDERBUFFERPROC         glBindRenderbuffer;
extern PFNGLRENDERBUFFERSTORAGEPROC      glRenderbufferStorage;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC  glFramebufferRenderbuffer;
extern PFNGLDELETERENDERBUFFERSPROC      glDeleteRenderbuffers;

extern PFNGLCREATESHADERPROC             glCreateShader;
extern PFNGLSHADERSOURCEPROC             glShaderSource;
extern PFNGLCOMPILESHADERPROC            glCompileShader;
extern PFNGLCREATEPROGRAMPROC            glCreateProgram;
extern PFNGLATTACHSHADERPROC             glAttachShader;
extern PFNGLLINKPROGRAMPROC              glLinkProgram;
extern PFNGLUSEPROGRAMPROC               glUseProgram;
extern PFNGLDELETESHADERPROC             glDeleteShader;
extern PFNGLDELETEPROGRAMPROC            glDeleteProgram;
extern PFNGLGETUNIFORMLOCATIONPROC       gl_GetUniformLocation;
extern PFNGLUNIFORM1IPROC                glUniform1i;
extern PFNGLUNIFORM1FPROC                glUniform1f;
extern PFNGLUNIFORM2FPROC                glUniform2f;
extern PFNGLUNIFORM4FPROC                glUniform4f;
extern PFNGLUNIFORMMATRIX3X2FVPROC       glUniformMatrix3x2fv;
extern PFNGLUNIFORMMATRIX4FVPROC         glUniformMatrix4fv;
extern PFNGLGETSHADERIVPROC              glGetShaderiv;
extern PFNGLGETPROGRAMIVPROC             glGetProgramiv;
extern PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog;

// Inline wrapper so call sites can use the standard name without
// clashing with the OpenGL 1.1 SDK prototype (which has wrong signature).
inline GLint glGetUniformLocation(GLuint prog, const GLchar* name) {
    return gl_GetUniformLocation ? gl_GetUniformLocation(prog, name) : -1;
}

extern PFNGLGENVERTEXARRAYSPROC          glGenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC          glBindVertexArray;
extern PFNGLDELETEVERTEXARRAYSPROC       glDeleteVertexArrays;
extern PFNGLGENBUFFERSPROC               glGenBuffers;
extern PFNGLBINDBUFFERPROC               glBindBuffer;
extern PFNGLBUFFERDATAPROC               glBufferData;
extern PFNGLBUFFERSUBDATAPROC            glBufferSubData;
extern PFNGLDELETEBUFFERSPROC            glDeleteBuffers;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer;

extern PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
extern PFNWGLSWAPINTERVALEXTPROC         wglSwapIntervalEXT;
extern PFNGLACTIVETEXTUREPROC            glActiveTexture;

// Shared shader programs
extern GLuint g_glShaderColor;   // solid-color shader (rects, ellipses, lines)
extern GLuint g_glShaderTex;     // textured quad shader (bitmaps, glyph atlas)

// Shared quad geometry (VAO + VBO, updated per draw call)
extern GLuint g_glQuadVAO;
extern GLuint g_glQuadVBO;

// Main window swap target
extern HWND g_glMainHwnd;