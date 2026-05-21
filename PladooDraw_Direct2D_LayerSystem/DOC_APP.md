# PladooDraw DLL — Guia de Arquitetura, Fluxo Interno e Integração de UI

## 1. Objetivo e visão geral
A DLL implementa toda a lógica de desenho, camadas, histórico (Undo/Redo), replay e animação. A UI (janelas, botões, timeline, ferramentas) é fornecida pela aplicação host, que passa `HWND`s para a DLL. A renderização é feita por um backend gráfico abstrato (`IGraphicsBackend`) e a aplicação funciona com Windows/x86.

> **Concept Box: DLL centrada em estado global**
> A DLL mantém praticamente todo o estado em variáveis globais (`Constants.h/.cpp`), e as funções exportadas agem sobre esse estado. A integração correta depende da ordem de inicialização e da atualização dessas variáveis.

---

## 2. Principais módulos
- **Main**: expõe a API pública (funções exportadas).
- **Render**: inicializa backend e associa janelas.
- **Helpers**: utilitários, renderização de ações, criação de superfícies, modos (replay/animation).
- **Layers**: gestão de camadas, botões de camadas, preview.
- **Tools / ToolsAux**: ferramentas de desenho e lógica de mouse/up/seleção.
- **Transforms**: zoom e tamanho de brush/eraser.
- **SaveLoad**: formato binário `.pdd`.
- **Replay / Animation**: timeline e reprodução.
- **SurfaceDial**: integração com Surface Dial.
- **SvgExporter**: exportação de ações para SVG.
- **BackendSelector**: escolhe backend gráfico.

---

## 3. Tipos base e estruturas

### 3.1 Tipos gráficos (`GraphicsTypes.h`)
- `PointF`, `RectF`, `EllipseF`, `SizeU`, `SizeF`, `ColorRGBA`, `Matrix3x2`
- Funções utilitárias: `MakePointF`, `MakeRectF`, `MakeEllipseF`, `MakeIdentityMatrix3x2`, `MakeScaleMatrix3x2`

### 3.2 Estruturas de domínio (`Structs.h`)
- `VERTICE`: ponto com brush size.
- `EDGE`: lista de `VERTICE` (freeform).
- `LINE`: start/end.
- `FLOATPOINT`: x/y (usado no PaintBucket).
- `TRANSFORM` + sub-estruturas `LOCATION`, `ROTATION`, `SCALE`: reservado para transformações.
- `ACTION`: unidade principal do histórico. Campos relevantes:
  - `Tool`, `Layer`, `FrameIndex`
  - `Position`, `Ellipse`, `Line`, `FreeForm`
  - `Color`, `FillColor`, `BrushSize`, `isPixelMode`
  - `Text` e metadata de fonte
  - `pixelsToFill` (PaintBucket)
  - `TargetID` e `LastMovedPosition` (movimento)
- `Layer`: contém `RenderSurfacePtr` + `BitmapSurfacePtr`.
- `LayerOrder`: ordenação visual.
- `LayerButton`, `TimelineFrameButton`: UI externa associada ao backend.

---

## 4. Estado global (núcleo do comportamento)
Definido em `Constants.h/.cpp`:
- Handles: `mainHWND`, `docHWND`, `layersHWND`, `timelineHWND`, etc.
- Backends: `gGraphicsBackend`
- Dimensões: `width`, `height`, `logicalWidth`, `logicalHeight`
- Zoom: `zoomFactor`
- Brush/Eraser: `currentBrushSize`, `currentEraserSize`
- Flags: `isDrawingRectangle`, `isReplayMode`, `isAnimationMode`, etc.
- Vetores de dados: `layers`, `layersOrder`, `Actions`, `RedoActions`, `LayerButtons`, `TimelineFrameButtons`

> **Concept Box: Ações como fonte da verdade**
> A lista `Actions` representa tudo que existe no desenho. As camadas são reconstruídas ao re-renderizar ações, e o replay/undo opera diretamente nelas.

---

## 5. API pública (funções exportadas)
Todas as funções abaixo são definidas em `Main.h` e implementadas em `Main.cpp`, servindo como ponte para os módulos internos.

### 5.1 Inicialização e infraestrutura

| Função | O que faz | Notas |
|---|---|---|
| `Initialize(HWND)` | Inicializa backend gráfico com a janela principal. | Chamar primeiro. |
| `InitializeDocument(HWND, w, h, pixelRatio, btnW, btnH)` | Registra janela de desenho e dimensões. | Cria resources de backend. |
| `InitializeWrite()` | Inicializa backend de texto. | Necessário antes de texto. |
| `InitializeSurfaceDial(HWND)` | Habilita Surface Dial. | Requer WinRT. |
| `InitializeLayerRenderPreview()` | Força render inicial de camadas. | Chama `TRenderLayers` e `TZoom`. |
| `InitializeLayers(HWND pLayerWindow, HWND pLayers, HWND pControlButtons)` | Armazena HWNDs de UI de layers. | Sem render direto. |
| `InitializeLayersButtons(HWND* buttons)` | Armazena botões up/down/plus/minus. | Apenas referência. |
| `InitializeTools(HWND)` | Guarda HWND da janela de ferramentas. | Sem render direto. |
| `InitializeTimeline(HWND)` | Guarda HWND da timeline. | Sem render direto. |
| `Resize()` | Invoca `ResizeDocument` do backend. | Usa `width/height/zoomFactor`. |
| `Cleanup()` | Libera backend e estado interno. | Chamado no `DllMain` detach. |

### 5.2 Seleção de ferramenta

| Função | O que faz |
|---|---|
| `SetSelectedTool(int)` | Define `selectedTool`. |
| `SelectTool(x,y)` | Seleciona ação clicada (hit-test). |
| `UnSelectTool()` | Limpa seleção ativa. |
| `MoveTool(x0,y0,x1,y1)` | Move ação selecionada (cria ação `TMove`). |

### 5.3 Ferramentas de desenho

| Função | O que faz |
|---|---|
| `EraserTool(x,y)` | Apaga com brush circular (clear clip). |
| `BrushTool(x,y,color,pixelMode,ratio)` | Traço contínuo ou pixelizado. |
| `RectangleTool(l,t,r,b,color)` | Preview em layer temporário. |
| `EllipseTool(l,t,r,b,color)` | Preview em layer temporário. |
| `LineTool(x0,y0,x1,y1,color)` | Preview em layer temporário. |
| `PaintBucketTool(x,y,fillColor,HWND)` | Flood fill usando pixels do canvas. |
| `WriteTool(x,y)` | Cria edit box e prepara texto. |
| `handleMouseUp()` | Fecha ferramentas de preview e grava action final. |

### 5.4 Layers e organização

| Função | O que faz |
|---|---|
| `AddLayer(fromFile, layer, frame)` | Cria nova layer render target. |
| `RemoveLayer()` | Remove layer atual. |
| `AddLayerButton(id)` | Cria botão de layer com preview. |
| `RemoveLayerButton()` | Remove botão atual. |
| `ReorderLayers(isAdding)` | Reordena UI de botões. |
| `ReorderLayerUp()` | Move layer para cima. |
| `ReorderLayerDown()` | Move layer para baixo. |
| `SetLayer(index)` | Define layer ativa. |
| `GetLayer()` | Retorna layer ativa. |
| `IsLayerActive(layer, *out)` | Informa se layer existe e está ativa. |
| `GetActiveLayersCount()` | Total de layers ativas. |
| `LayersCount()` | Total lógico de layers. |
| `ShowCurrentLayerOnly()` | Oculta demais e re-renderiza. |
| `UpdateLayers(layerIndex)` | Re-renderiza a layer alvo. |
| `RenderLayers()` | Compõe todas layers na superfície do documento. |
| `DrawLayerPreview(layer)` | Atualiza preview do botão. |

### 5.5 Replay e animação

| Função | O que faz |
|---|---|
| `SetReplayMode(int)` | Entra/saí do modo replay. |
| `ReplayForward()` | Avança um step no replay. |
| `ReplayBackwards()` | Retrocede um step no replay. |
| `EditFromThisPoint()` | Apaga ações após ponto. |
| `SetAnimationMode(int)` | Entra/saí do modo animation. |
| `CreateAnimationFrame()` | Cria novo frame na timeline. |
| `RemoveAnimationFrame()` | Remove frame atual. |
| `AnimationForward()` | Avança frame. |
| `AnimationBackward()` | Retrocede frame. |
| `RenderAnimation()` | Renderiza thumbnails da timeline. |
| `PlayAnimation()` | Reproduz frames automaticamente. |
| `PauseAnimation()` | Interrompe reprodução. |
| `SetAnimationFrame(int)` | Define `CurrentFrameIndex`. |
| `GetCurrentFrameIndex()` | Retorna índice do frame. |
| `GetMaxFrameIndex()` | Retorna maior frame usado. |

### 5.6 Zoom e brush size

| Função | O que faz |
|---|---|
| `GetZoomFactor()` | Retorna zoom atual. |
| `SetZoomFactor(int)` | Ajusta zoom e re-render. |
| `ZoomIn_Default()` | Incremento de 0.1. |
| `ZoomOut_Default()` | Decremento de 0.1. |
| `ZoomIn(float)` | Zoom com fator custom. |
| `ZoomOut(float)` | Zoom com fator custom. |
| `Zoom()` | Aplica zoom reposicionando janela. |
| `IncreaseBrushSize_Default()` | +0.5. |
| `DecreaseBrushSize_Default()` | -0.5. |
| `IncreaseBrushSize(float)` | Ajusta brush/eraser conforme tool. |
| `DecreaseBrushSize(float)` | Ajusta brush/eraser conforme tool. |

### 5.7 Persistência e exportação

| Função | O que faz |
|---|---|
| `SaveProjectDll(path)` | Salva binário `.pdd`. |
| `LoadProjectDll(path)` | Carrega binário `.pdd`. |
| `LoadProjectDllW(pathW)` | Versão wide. |
| `ExportSVG()` | Exporta ações para SVG. |

### 5.8 Input/scroll

| Função | O que faz |
|---|---|
| `OnScrollWheelLayers(wParam)` | Scroll no painel de layers. |
| `OnScrollWheelTimeline(wParam)` | Scroll na timeline. |

---

## 6. Fluxo de execução típico (integração de UI)
1. `Initialize(mainHWND)`
2. `InitializeDocument(docHWND, w, h, pixelRatio, btnW, btnH)`
3. `InitializeLayers(...)`, `InitializeTimeline(...)`, `InitializeTools(...)`
4. `AddLayer(...)` ou `LoadProjectDll(...)`
5. Input do usuário chama `BrushTool` / `EraserTool` / etc.
6. Ao soltar mouse: `handleMouseUp()`
7. UI chama `RenderLayers()` quando necessário

> **Concept Box: Preview layer temporária**
> Ferramentas como `Rectangle`, `Ellipse`, `Line` criam uma layer extra temporária durante o drag. Essa layer é removida em `THandleMouseUp`.

---

## 7. Módulos internos (função por função)

### 7.1 Render (`Render.h/.cpp`)
| Função | Descrição |
|---|---|
| `TInitialize` | Instancia backend e inicializa janela principal. |
| `TInitializeDocument` | Define dimensões e janela de documento, ajusta zoom inicial. |
| `TInitializeWrite` | Inicializa suporte a texto via backend. |
| `TInitializeLayerRenderPreview` | Render inicial + aplica zoom. |
| `TInitializeLayers` | Guarda HWNDs da UI de layers. |
| `TInitializeTools` | Guarda HWND da UI de ferramentas. |
| `TInitializeTimeline` | Guarda HWND da timeline. |
| `TInitializeLayersButtons` | Guarda HWNDs dos botões (up/down/etc). |

### 7.2 Helpers (`Helpers.h/.cpp`)
| Função | Descrição |
|---|---|
| `HGetActiveLayersCount` | Conta layers ativas. |
| `HGetMaxFrameIndex` | Maior `FrameIndex` em `Actions`. |
| `GetActionStepCount` | Steps para replay (brush/pbucket). |
| `MakeStepAction` | Gera action parcial (replay). |
| `HRenderAction` | Desenha uma `ACTION` em um `IRenderSurface`. |
| `HCreateRenderData` | Cria surface+bitmap offscreen. |
| `HCreateRenderDataHWND` | Cria surface para HWND. |
| `HCreateTimelineFrameButton` | Cria botão para frame. |
| `HCreateHighlightFrame` | Cria highlight da timeline. |
| `HSetTimelineHighlight` | Posiciona highlight. |
| `HSetReplayMode` | Entra/saí de replay e reconstrói timeline. |
| `HSetAnimationMode` | Entra/saí de animation. |
| `HSetSelectedTool` | Define ferramenta atual. |
| `HCreateLogData` | Append de log. |
| `HGetRGBAColor` | Converte `COLORREF` em RGBA. |
| `HScalePointsToButton` | Escala coords p/ preview. |
| `HSplit` | Split de string. |
| `HPrintHResultError` | Log HRESULT. |
| `HCleanup` | Zera estruturas globais. |
| `HOnScrollWheelLayers` | Scroll no painel layers. |
| `HOnScrollWheelTimeline` | Scroll na timeline. |
| `HSetScrollPosition` | Scroll timeline via índice. |

### 7.3 Layers (`Layers.h/.cpp`)
| Função | Descrição |
|---|---|
| `CreateEmptyLayerBitmap` | Cria bitmap offscreen com fallback 1x1. |
| `TLayersCount` | Retorna max LayerID + 1. |
| `TAddLayer` | Cria layer + action `TLayer`. |
| `TReorderLayers` | Reordena botões visíveis. |
| `TAddLayerButton` | Cria botão UI + surface. |
| `THideAllUnusedLayerButtons` | Oculta botões fora do frame atual. |
| `TRemoveLayerButton` | Remove botão e layer. |
| `TRemoveLayer` | Remove layer e ações. |
| `TGetLayer` | Retorna `layerIndex`. |
| `TSetLayer` | Atualiza `layerIndex`. |
| `TShowCurrentLayerOnly` | Isola camada ativa. |
| `TReorderLayerUp` | Sobe layer. |
| `TReorderLayerDown` | Desce layer. |
| `TUpdateLayers` | Recria bitmap da layer atual a partir das actions. |
| `TRenderLayers` | Compõe layers no documentSurface. |
| `TDrawLayerPreview` | Atualiza preview do botão. |
| `TSelectedLayerHighlight` | Desenha borda/preview em botões. |

### 7.4 Tools (`Tools.h/.cpp`)
| Função | Descrição |
|---|---|
| `TEraserTool` | Apaga em retângulos com clip. |
| `TBrushTool` | Pinta pontos e cria vertices. |
| `TRectangleTool` | Preview com layer temporária. |
| `TEllipseTool` | Preview com layer temporária. |
| `TLineTool` | Preview com layer temporária. |
| `TPaintBucketTool` | Flood fill no canvas com BFS. |
| `TInitTextTool` | Cria edit box para digitar texto. |
| `TWriteTool` | Abre edit box e prepara área. |
| `TWriteToolCommitText` | Converte edit box em ação de texto. |
| `TSelectTool` | Hit-test de action. |
| `TMoveTool` | Cria action `TMove`. |
| `TUnSelectTool` | Remove seleção. |
| `TextEditProc` | Subclasse do edit box para Enter. |

### 7.5 ToolsAux (`ToolsAux.h/.cpp`)
| Função | Descrição |
|---|---|
| `THandleMouseUp` | Finaliza ações temporárias (preview). |
| `TUpdatePaint` | Move pixels de PaintBucket vinculados. |
| `TUndo` | Undo com suporte a TMove. |
| `TRedo` | Redo simétrico ao Undo. |
| `CalculateMovementDelta` | Delta relativo para mover ação. |
| `HitTestAction` | Teste geométrico de clique. |
| `CaptureCanvasPixels` | Captura bitmap atual para PaintBucket. |
| `AuxCopyAction` | Copia action para Clipboard. |
| `AuxPasteAction` | Cola action. |
| `AuxDeleteAction` | Deleta action selecionada. |

### 7.6 Transforms (`Transforms.h/.cpp`)
| Função | Descrição |
|---|---|
| `TGetZoomFactor` | Retorna zoom atual. |
| `TSetZoomFactor` | Ajusta zoom e aplica. |
| `TZoomIn` | Incremento com clamp. |
| `TZoomOut` | Decremento com clamp. |
| `TZoom` | Redimensiona `docHWND` e re-renderiza. |
| `TIncreaseBrushSize_Default` | Wrapper. |
| `TDecreaseBrushSize_Default` | Wrapper. |
| `TIncreaseBrushSize` | Ajusta brush/eraser. |
| `TDecreaseBrushSize` | Ajusta brush/eraser. |

### 7.7 Animation (`Animation.h/.cpp`)
| Função | Descrição |
|---|---|
| `TSetAnimationFrame` | Define `CurrentFrameIndex`. |
| `TCreateAnimationFrame` | Cria frame na timeline. |
| `TReorganizeFrames` | Reposiciona botões. |
| `TRemoveAnimationFrame` | Remove frame atual. |
| `TUpdateAnimation` | Redesenha thumbnails. |
| `TRenderAnimation` | Renderiza preview dos frames. |
| `TAnimationForward` | Próximo frame. |
| `TAnimationBackward` | Frame anterior. |
| `TPlayAnimation` | Avança automaticamente. |
| `TPauseAnimation` | Interrompe playback. |

### 7.8 Replay (`Replay.h/.cpp`)
| Função | Descrição |
|---|---|
| `TReplayClearLayers` | Zera layers de replay. |
| `TCreateReplayFrame` | Cria frame de replay. |
| `TReplayRender` | Renderiza a sequência atual. |
| `TEditFromThisPoint` | Remove ações após posição. |
| `TReplayBackwards` | Volta por steps. |
| `TReplayForward` | Avança por steps. |

### 7.9 SaveLoad (`SaveLoad.h/.cpp`)
| Função | Descrição |
|---|---|
| `SaveBinaryProject` | Salva `.pdd` com header + actions. |
| `LoadBinaryProject` | Reconstroi projeto do `.pdd`. |

> **Concept Box: Formato `.pdd`**
> `magic(0x30444450)`, `version`, `isAnimationMode`, `width`, `height`, `pixelSizeRatio`, layers, layerOrder e lista de `Actions`. Ações `TWrite` serializam texto e fonte.

### 7.10 SvgExporter (`SvgExporter.h/.cpp`)
| Função | Descrição |
|---|---|
| `ColorRefToSvgHex` | Converte cor para `#RRGGBB`. |
| `ExportActionsToSvg` | Gera SVG a partir das ações. |

### 7.11 SurfaceDial (`SurfaceDial.h/.cpp`)
| Função | Descrição |
|---|---|
| `TInitializeSurfaceDial` | Habilita Dial e menu. |
| `CleanupSurfaceDial` | Libera tokens/eventos. |
| `OnDialRotation` | Zoom, brush ou replay/animation. |
| `OnDialButtonClick` | Reset zoom ou alternar replay. |

### 7.12 Actions (`Actions.h/.cpp`)
| Função | Descrição |
|---|---|
| `TCreateMoveAction` | Cria action do tipo `TMove`. |

### 7.13 TransformProperty (`TransformProperty.h/.cpp`)
| Função | Descrição |
|---|---|
| `SetLocation` | Aplica delta a ação selecionada. |
| `SetRotation` | Reservado. |
| `SetScale` | Reservado. |

### 7.14 BackendSelector (`BackendSelector.h/.cpp`)
| Função | Descrição |
|---|---|
| `CreateGraphicsBackend` | Seleciona o backend ativo. |

---

## 8. Limitações e suposições
- **x86-only** (build Win32).
- **Single-threaded**: chamadas de UI e render devem ocorrer no mesmo thread.
- **Fortemente global**: chamadas fora de ordem podem causar estado inválido.

---

## 9. Possíveis simplificações (mesma funcionalidade, código menor)
- Consolidar `Actions` e `Layer` para evitar múltiplas listas redundantes.
- Unificar `Replay` e `Animation` em um único pipeline de timeline.
- Substituir `std::optional<Layer>` por estrutura fixa com `isActive`.
- Reduzir uso de `HWND` específicos em helpers e receber callbacks genéricos.
- Centralizar controle de preview layer (em vez de criar/remover no `THandleMouseUp`).
