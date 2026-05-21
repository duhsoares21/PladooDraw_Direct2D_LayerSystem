# Backend Direct2D/DX11 — Arquitetura e Guia Completo

## 1. Visão geral
Backend baseado em Direct2D + D3D11 + DXGI. Renderiza para `ID2D1DeviceContext` com targets que podem ser swapchain (janela) ou bitmap (offscreen).

> **Concept Box: Swapchain**
> Swapchain é a fila de buffers para uma janela. O D2D desenha em um bitmap que aponta para o backbuffer do swapchain, e `Present()` exibe.

---

## 2. Estado global (`Direct2DBackendState.h`)
Principais objetos:
- `g_pD3DDevice`, `g_pD3DContext`: dispositivo D3D11
- `g_dxgiDevice`, `g_adapter`, `g_dxgiFactory`: infraestrutura DXGI
- `g_pSwapChain`: swapchain do documento
- `g_pD2DDevice`, `pRenderTarget`: device/context D2D
- `pD2DTargetBitmap`: bitmap target do documento
- `pDWriteFactory`: texto (DirectWrite)

---

## 3. Classes

### 3.1 `Direct2DBitmapSurface`
| Função | Descrição |
|---|---|
| `GetSize` | Retorna `SizeU`. |
| `GetBitmap` | Retorna ponteiro `ID2D1Bitmap1`. |

### 3.2 `Direct2DRenderSurface`
| Função | Descrição |
|---|---|
| `BeginDraw` | `SetTarget` e `BeginDraw`. |
| `EndDraw` | Finaliza o draw. |
| `Clear` | `context_->Clear`. |
| `SetTransform` | `SetTransform(D2D1::Matrix3x2F)`. |
| `PushClip` | `PushAxisAlignedClip`. |
| `PopClip` | `PopAxisAlignedClip`. |
| `DrawBitmap` | `DrawBitmap` com interp linear. |
| `FillRect` | `FillRectangle`. |
| `StrokeRect` | `DrawRectangle`. |
| `FillEllipse` | `FillEllipse`. |
| `StrokeEllipse` | `DrawEllipse`. |
| `DrawLine` | `DrawLine`. |
| `DrawText` | Usa `IDWriteTextFormat` e `DrawTextW`. |
| `SetPrimitiveBlendCopy` | `D2D1_PRIMITIVE_BLEND_COPY` ou `SOURCE_OVER`. |
| `SetAliased` | Alterna antialias. |
| `Present` | `swapChain->Present`. |
| `GetContext` | Retorna `ID2D1DeviceContext`. |
| `GetSwapChain` | Retorna swapchain. |
| `GetTargetBitmap` | Retorna bitmap alvo. |

---

## 4. `Direct2DBackend` (função por função)

| Função | Descrição |
|---|---|
| `InitializeMainWindow` | `RoInitialize` + `D2D1CreateFactory`. |
| `InitializeDocument` | Cria D3D11 device, swapchain e bitmap target. |
| `InitializeText` | Cria `IDWriteFactory`. |
| `ResizeDocument` | Recria buffers do swapchain e bitmap target. |
| `CreateWindowRenderData` | Cria swapchain para HWND e bitmap target. |
| `CreateBitmapRenderData` | Cria bitmap D2D offscreen. |
| `GetDocumentSurface` | Retorna `documentSurface_`. |
| `ReadDocumentPixels` | Renderiza layer em bitmap, copia para CPU. |
| `MeasureText` | Usa DirectWrite layout para medir. |
| `PresentDocument` | `Present` no swapchain principal. |
| `Cleanup` | Libera todos os COM objects. |

### Funções internas relevantes
- `CreateDeviceResources`
  Conecta D3D11 ao D2D e cria `pRenderTarget`.
- `RebuildDocumentTargetBitmap`
  Cria bitmap D2D que referencia o backbuffer do swapchain.
- `RefreshDocumentSurface`
  Reconstrói `documentSurface_`.
- `MakeSwapChainDescription`
  Descreve swapchain para janela.

---

## 5. Simplificações possíveis
- Remover dependência de `IDComposition` (não é usada no fluxo atual).
- Consolidar criação de swapchain para uma função única com parâmetros.
- Evitar `CreateDeviceContext` múltiplas vezes criando um pool.
