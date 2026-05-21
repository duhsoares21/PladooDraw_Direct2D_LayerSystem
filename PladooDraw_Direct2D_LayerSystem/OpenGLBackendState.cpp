#include "OpenGLBackendState.h"

// Win32 / WGL context handles
HDC   g_glMainDC = nullptr;
HDC   g_glPermaDC = nullptr;
HDC   g_glDocDC = nullptr;
HGLRC g_glContext = nullptr;
HWND  g_glMainHwnd = nullptr;

// Shader programs
GLuint g_glShaderColor = 0;
GLuint g_glShaderTex = 0;

// Shared quad geometry
GLuint g_glQuadVAO = 0;
GLuint g_glQuadVBO = 0;

// --- Extension function pointers (all start as nullptr) ---

// FBO
PFNGLGENFRAMEBUFFERSPROC          glGenFramebuffers = nullptr;
PFNGLBINDFRAMEBUFFERPROC          glBindFramebuffer = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC     glFramebufferTexture2D = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC   glCheckFramebufferStatus = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC       glDeleteFramebuffers = nullptr;
PFNGLGENRENDERBUFFERSPROC         glGenRenderbuffers = nullptr;
PFNGLBINDRENDERBUFFERPROC         glBindRenderbuffer = nullptr;
PFNGLRENDERBUFFERSTORAGEPROC      glRenderbufferStorage = nullptr;
PFNGLFRAMEBUFFERRENDERBUFFERPROC  glFramebufferRenderbuffer = nullptr;
PFNGLDELETERENDERBUFFERSPROC      glDeleteRenderbuffers = nullptr;

// Shaders
PFNGLCREATESHADERPROC             glCreateShader = nullptr;
PFNGLSHADERSOURCEPROC             glShaderSource = nullptr;
PFNGLCOMPILESHADERPROC            glCompileShader = nullptr;
PFNGLCREATEPROGRAMPROC            glCreateProgram = nullptr;
PFNGLATTACHSHADERPROC             glAttachShader = nullptr;
PFNGLLINKPROGRAMPROC              glLinkProgram = nullptr;
PFNGLUSEPROGRAMPROC               glUseProgram = nullptr;
PFNGLDELETESHADERPROC             glDeleteShader = nullptr;
PFNGLDELETEPROGRAMPROC            glDeleteProgram = nullptr;
PFNGLGETUNIFORMLOCATIONPROC       gl_GetUniformLocation = nullptr;
PFNGLUNIFORM1IPROC                glUniform1i = nullptr;
PFNGLUNIFORM1FPROC                glUniform1f = nullptr;
PFNGLUNIFORM2FPROC                glUniform2f = nullptr;
PFNGLUNIFORM4FPROC                glUniform4f = nullptr;
PFNGLUNIFORMMATRIX3X2FVPROC       glUniformMatrix3x2fv = nullptr;
PFNGLUNIFORMMATRIX4FVPROC         glUniformMatrix4fv = nullptr;
PFNGLGETSHADERIVPROC              glGetShaderiv = nullptr;
PFNGLGETPROGRAMIVPROC             glGetProgramiv = nullptr;
PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog = nullptr;

// VAO / VBO
PFNGLGENVERTEXARRAYSPROC          glGenVertexArrays = nullptr;
PFNGLBINDVERTEXARRAYPROC          glBindVertexArray = nullptr;
PFNGLDELETEVERTEXARRAYSPROC       glDeleteVertexArrays = nullptr;
PFNGLGENBUFFERSPROC               glGenBuffers = nullptr;
PFNGLBINDBUFFERPROC               glBindBuffer = nullptr;
PFNGLBUFFERDATAPROC               glBufferData = nullptr;
PFNGLBUFFERSUBDATAPROC            glBufferSubData = nullptr;
PFNGLDELETEBUFFERSPROC            glDeleteBuffers = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer = nullptr;

// WGL extensions
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;
PFNWGLSWAPINTERVALEXTPROC         wglSwapIntervalEXT = nullptr;
PFNGLACTIVETEXTUREPROC            glActiveTexture = nullptr;