# Backend OpenGLBackendV2 — Arquitetura e Guia Completo

## 1. Visão geral
Backend OpenGL 3.3 Core via WGL. Usa shaders simples (cor sólida e textura) e renderiza em FBOs offscreen ou no backbuffer da janela.

> **Concept Box: FBO**
> Framebuffer Object permite renderizar para uma textura (offscreen). Cada layer é um FBO com textura RGBA.

---

## 2. Estado global (`OpenGLBackendV2State.h/.cpp`)
- Ponteiros para funções OpenGL (carregadas via `wglGetProcAddress`)
- `g_glProgramColor`, `g_glProgramTex`: shaders
- `g_glQuadVAO`, `g_glQuadVBO`: geometria base
- `g_glContext`, `g_glMainDC`, `g_glMainHwnd`: contexto WGL

---

## 3. Classes

### 3.1 `OpenGLBitmapSurfaceV2`
| Função | Descrição |
|---|---|
| `GetSize` | Retorna `SizeU`. |
| `GetTextureId` | Retorna `GLuint` da textura. |

### 3.2 `OpenGLRenderSurfaceV2`
| Função | Descrição |
|---|---|
| `BeginDraw` | Bind FBO, viewport, blend, scissor. |
| `EndDraw` | Retorna `S_OK`. |
| `Clear` | `glClear`. |
| `SetTransform` | Guarda `Matrix3x2`. |
| `PushClip` | Calcula scissor e ativa. |
| `PopClip` | Remove último clip. |
| `DrawBitmap` | Desenha textura com UV invertido. |
| `FillRect` | Quad com shader de cor. |
| `StrokeRect` | Quatro rects finos. |
| `FillEllipse` | `TRIANGLE_FAN` aproximado. |
| `StrokeEllipse` | `LINE_STRIP`. |
| `DrawLine` | Quad orientado à linha. |
| `DrawText` | Texto via GDI -> textura. |
| `SetPrimitiveBlendCopy` | Alterna blend. |
| `SetAliased` | Configura antialias (placeholder). |
| `Present` | `SwapBuffers` e libera HDC. |
| `GetFbo` | Retorna FBO id. |
| `GetColorTexture` | Retorna texture id. |
| `GetHdc` | Retorna HDC atual. |

### Funções internas importantes
- `BindTarget`
  Adquire HDC quando necessário e faz `wglMakeCurrent`.
- `UseColorProgram` / `UseTexProgram`
  Configura shaders e uniforms.
- `UploadQuad`
  Atualiza VBO com retângulo.
- `DrawEllipseInternal`
  Monta vértices para círculo/ellipse.

---

## 4. `OpenGLBackendV2` (função por função)

| Função | Descrição |
|---|---|
| `InitializeMainWindow` | Cria contexto WGL (3.3 core). |
| `InitializeDocument` | Configura pixel format da janela doc. |
| `InitializeText` | No-op (texto usa GDI). |
| `ResizeDocument` | Recria surface do documento. |
| `CreateWindowRenderData` | Surface de janela (swapbuffers). |
| `CreateBitmapRenderData` | FBO + textura. |
| `GetDocumentSurface` | Retorna surface do documento. |
| `ReadDocumentPixels` | `glReadPixels` do FBO da layer atual. |
| `MeasureText` | Usa GDI `GetTextExtentPoint32W`. |
| `PresentDocument` | `Present` na surface. |
| `Cleanup` | Libera shaders, VAO/VBO, contexto. |

### Funções internas críticas
- `EnsurePixelFormat`
  Define pixel format compatível (RGBA, double buffer).
- `LoadExtensions`
  Carrega funções modernas do OpenGL.
- `BuildShaders`
  Compila shaders simples (cor e textura).
- `BuildGeometry`
  Inicializa VAO/VBO.
- `CreateDocumentSurface`
  Associa surface do documento ao HWND.

---

## 5. Simplificações possíveis
- Substituir o carregamento manual por `glad` ou `glew`.
- Usar `stb_truetype` para texto, eliminando GDI.
- Consolidar `UploadQuad` e evitar reupload em chamadas repetidas.
- Usar um único VAO para todas as primitivas (já quase assim).
- Remover `depthRbo_` se não for utilizado.
