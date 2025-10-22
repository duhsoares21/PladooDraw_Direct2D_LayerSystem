# Documentação da API da DLL

Esta documentação detalha os métodos externos acessíveis (exportados) pela DLL, conforme definidos no arquivo de cabeçalho `Main.h`. A API é projetada para ser consumida por aplicações host, fornecendo funcionalidades de desenho, gerenciamento de camadas, animação, histórico (replay) e interação com ferramentas.

## Convenções e Tipos de Dados

Os métodos são exportados com a convenção de chamada `__stdcall` e são acessíveis externamente via `extern "C" __declspec(dllexport)`.

| Tipo de Dado | Descrição |
| :--- | :--- |
| `HWND` | Handle de janela do Windows (Window Handle). Usado para referenciar janelas ou controles da UI. |
| `HRESULT` | Tipo de retorno comum em APIs Windows, indica sucesso (`S_OK`) ou falha. |
| `COLORREF` | Valor de 32 bits que especifica uma cor no formato RGB (Red, Green, Blue). No Windows, é tipicamente 0x00BBGGRR. |
| `LPCSTR` | Ponteiro longo para uma string constante ANSI (usado para caminhos de arquivo). |
| `LPWSTR` | Ponteiro longo para uma string Unicode/Wide Character (usado para caminhos de arquivo). |
| `int`, `float`, `bool` | Tipos de dados primitivos padrão. |

---

## 1. Inicialização e Configuração

Estes métodos são usados para configurar e iniciar os componentes principais da DLL.

### `Initialize`
Inicializa os componentes principais da DLL.
| Assinatura | `HRESULT Initialize(HWND pmainHWND)` |
| :--- | :--- |
| **Parâmetros** | |
| `pmainHWND` | `HWND`. Handle da janela principal (main window handle) da aplicação hospedeira. |
| **Retorno** | `HRESULT`. Indica o sucesso ou falha da inicialização. |
| **Descrição** | Deve ser a primeira função a ser chamada para preparar o ambiente da DLL. |

### `InitializeDocument`
Inicializa um novo documento de desenho com as dimensões e configurações de pixel especificadas.
| Assinatura | `HRESULT InitializeDocument(HWND hWnd, int pWidth, int pHeight, int pPixelSizeRatio, int pBtnWidth, int pBtnHeight)` |
| :--- | :--- |
| **Parâmetros** | |
| `hWnd` | `HWND`. Handle da janela onde o documento será renderizado. |
| `pWidth` | `int`. Largura do documento em pixels. |
| `pHeight` | `int`. Altura do documento em pixels. |
| `pPixelSizeRatio` | `int`. Fator de escala para o modo pixel (pixel size ratio). |
| `pBtnWidth` | `int`. Largura dos botões (possivelmente relacionados à UI). |
| `pBtnHeight` | `int`. Altura dos botões (possivelmente relacionados à UI). |
| **Retorno** | `HRESULT`. |

### `Resize`
Redimensiona a área de trabalho ou os componentes internos da DLL.
| Assinatura | `void __stdcall Resize()` |
| :--- | :--- |
| **Parâmetros** | Nenhum. |
| **Retorno** | `void`. |
| **Descrição** | Chamado tipicamente após um evento de redimensionamento de janela para ajustar a área de desenho. |

### Outras Inicializações

| Método | Assinatura | Descrição |
| :--- | :--- | :--- |
| `InitializeWrite` | `HRESULT InitializeWrite()` | Inicializa o componente de escrita/texto da DLL. |
| `InitializeSurfaceDial` | `void InitializeSurfaceDial(HWND pmainHWND)` | Inicializa a integração e o suporte ao Microsoft Surface Dial. Requer o handle da janela principal. |
| `InitializeLayerRenderPreview` | `HRESULT InitializeLayerRenderPreview()` | Inicializa o componente de pré-visualização de renderização de camadas. |
| `InitializeLayersButtons` | `HRESULT InitializeLayersButtons(HWND* buttonsHwnd)` | Inicializa a interface e a lógica dos botões de gerenciamento de camadas. Recebe um ponteiro para um array de Handles de janelas dos botões. |
| `InitializeLayers` | `HRESULT InitializeLayers(HWND pLayerWindow, HWND pLayers, HWND pControlButtons)` | Inicializa o sistema de gerenciamento de camadas, requer handles para os componentes da UI de camadas. |
| `InitializeTools` | `HRESULT InitializeTools(HWND hWnd)` | Inicializa as ferramentas de desenho e edição. |
| `InitializeReplay` | `HRESULT InitializeReplay(HWND hWnd)` | Inicializa o sistema de gravação e reprodução (Replay) de ações. |

---

## 2. Gerenciamento de Projeto e Exportação

Métodos para salvar, carregar e exportar o projeto.

| Método | Assinatura | Descrição |
| :--- | :--- | :--- |
| `SaveProjectDll` | `void __stdcall SaveProjectDll(const char* pathAnsi)` | Salva o estado atual do projeto em um arquivo no caminho especificado (ANSI). |
| `LoadProjectDll` | `void __stdcall LoadProjectDll(LPCSTR apath)` | Carrega um projeto salvo a partir do caminho especificado (ANSI). |
| `LoadProjectDllW` | `void __stdcall LoadProjectDllW(LPWSTR lppath)` | Carrega um projeto salvo a partir do caminho especificado (Unicode/Wide Character). **Recomendado para caminhos modernos.** |
| `ExportSVG` | `void __stdcall ExportSVG()` | Exporta o conteúdo atual do documento para o formato SVG. |

---

## 3. Gerenciamento de Camadas

Funções para manipular a ordem, visibilidade e contagem de camadas.

| Método | Assinatura | Descrição |
| :--- | :--- | :--- |
| `AddLayer` | `void AddLayer(bool fromFile, int currentLayer, int currentFrame)` | Adiciona uma nova camada. `fromFile` indica se deve ser carregada de um arquivo. |
| `RemoveLayer` | `HRESULT RemoveLayer()` | Remove a camada atualmente selecionada. |
| `GetLayer` | `int GetLayer()` | Retorna o índice da camada atualmente selecionada. |
| `SetLayer` | `void SetLayer(int index)` | Define a camada ativa pelo seu índice. |
| `LayersCount` | `int LayersCount()` | Retorna o número total de camadas no projeto. |
| `GetActiveLayersCount` | `int GetActiveLayersCount()` | Retorna o número total de camadas ativas (visíveis). |
| `IsLayerActive` | `int __stdcall IsLayerActive(int layer, int* isActive)` | Verifica se uma camada específica está ativa (visível). O resultado é retornado via ponteiro `isActive`. |
| `ReorderLayers` | `void ReorderLayers(int isAddingLayer)` | Reordena as camadas. `isAddingLayer` (1 para sim, 0 para não) ajusta o comportamento. |
| `ReorderLayerUp` | `void ReorderLayerUp()` | Move a camada selecionada uma posição para cima. |
| `ReorderLayerDown` | `void ReorderLayerDown()` | Move a camada selecionada uma posição para baixo. |
| `AddLayerButton` | `void AddLayerButton(int LayerButtonID)` | Adiciona um botão de camada à interface do usuário (UI). |
| `RemoveLayerButton` | `void RemoveLayerButton()` | Remove o botão de camada atualmente selecionado da UI. |
| `DrawLayerPreview` | `void DrawLayerPreview(int currentLayer)` | Desenha a pré-visualização de uma camada específica (tipicamente usada no painel de camadas). |

---

## 4. Ferramentas de Desenho e Interação

Métodos para selecionar ferramentas e realizar operações de desenho.

### 4.1. Seleção e Interação com Ferramentas

| Método | Assinatura | Descrição |
| :--- | :--- | :--- |
| `SetSelectedTool` | `void SetSelectedTool(int pselectedTool)` | Define a ferramenta ativa pelo seu ID. Os IDs devem ser consultados em `Constants.h`. |
| `SelectTool` | `void __stdcall SelectTool(int xInitial, int yInitial)` | Inicia a interação com a ferramenta atual nas coordenadas iniciais (ex: `Mouse Down`). |
| `UnSelectTool` | `void __stdcall UnSelectTool()` | Finaliza a interação com a ferramenta atual (ex: `Mouse Up`). |
| `MoveTool` | `void __stdcall MoveTool(int xInitial, int yInitial, int x, int y)` | Atualiza a posição da ferramenta durante um arrasto. `(xInitial, yInitial)` é o ponto de início, `(x, y)` é o ponto atual. |
| `handleMouseUp` | `void handleMouseUp()` | Processa o evento de liberação do botão do mouse. Tipicamente finaliza a ação da ferramenta. |
| `SetFont` | `void __stdcall SetFont()` | Abre a caixa de diálogo de seleção de fonte ou aplica a fonte previamente selecionada para a ferramenta de texto. |

### 4.2. Operações de Desenho

| Método | Assinatura | Descrição |
| :--- | :--- | :--- |
| `EraserTool` | `void EraserTool(int left, int top)` | Aplica a Borracha na coordenada especificada. |
| `BrushTool` | `void BrushTool(int left, int top, COLORREF hexColor, bool pixelMode, int pPixelSizeRatio)` | Aplica o Pincel na coordenada. `hexColor` define a cor, `pixelMode` ativa o modo pixel, e `pPixelSizeRatio` define a escala do pixel. |
| `RectangleTool` | `void RectangleTool(int left, int top, int right, int bottom, unsigned int hexColor)` | Desenha um retângulo. As coordenadas definem o canto superior esquerdo e inferior direito. |
| `EllipseTool` | `void EllipseTool(int left, int top, int right, int bottom, unsigned int hexColor)` | Desenha uma elipse dentro do *bounding box* definido pelas coordenadas. |
| `LineTool` | `void LineTool(int xInitial, int yInitial, int x, int y, unsigned int hexColor)` | Desenha uma linha reta entre `(xInitial, yInitial)` e `(x, y)`. |
| `PaintBucketTool` | `void PaintBucketTool(int xInitial, int yInitial, COLORREF hexFillColor, HWND hWnd)` | Aplica o Balde de Tinta para preencher uma área contígua a partir de `(xInitial, yInitial)` com a cor `hexFillColor`. |
| `WriteTool` | `void WriteTool(int x, int y)` | Inicia a inserção ou edição de texto na posição especificada. |

---

## 5. Zoom e Configuração de Ferramentas

Métodos para controle de visualização e tamanho de ferramentas.

### 5.1. Zoom

| Método | Assinatura | Descrição |
| :--- | :--- | :--- |
| `GetZoomFactor` | `float __stdcall GetZoomFactor()` | Retorna o fator de zoom atual. |
| `SetZoomFactor` | `void __stdcall SetZoomFactor(int pZoomFactor)` | Define um novo fator de zoom. |
| `ZoomIn_Default` | `void ZoomIn_Default()` | Aplica zoom in com incremento padrão. |
| `ZoomOut_Default` | `void ZoomOut_Default()` | Aplica zoom out com incremento padrão. |
| `ZoomIn` | `void ZoomIn(float zoomIncrement)` | Aplica zoom in com um incremento personalizado. |
| `ZoomOut` | `void ZoomOut(float zoomIncrement)` | Aplica zoom out com um decremento personalizado. |
| `Zoom` | `void Zoom()` | Executa a operação de zoom (comportamento depende do estado interno). |

### 5.2. Tamanho do Pincel/Ferramenta

| Método | Assinatura | Descrição |
| :--- | :--- | :--- |
| `IncreaseBrushSize_Default` | `void IncreaseBrushSize_Default()` | Aumenta o tamanho do pincel/ferramenta com um incremento padrão. |
| `DecreaseBrushSize_Default` | `void DecreaseBrushSize_Default()` | Diminui o tamanho do pincel/ferramenta com um decremento padrão. |
| `IncreaseBrushSize` | `void IncreaseBrushSize(float sizeIncrement)` | Aumenta o tamanho do pincel/ferramenta com um incremento personalizado. |
| `DecreaseBrushSize` | `void DecreaseBrushSize(float sizeIncrement)` | Diminui o tamanho do pincel/ferramenta com um decremento personalizado. |

---

## 6. Replay e Histórico (Undo/Redo)

Funções para gerenciar o histórico de ações e a reprodução (Replay).

| Método | Assinatura | Descrição |
| :--- | :--- | :--- |
| `Undo` | `void Undo()` | Desfaz a última ação realizada. |
| `Redo` | `void Redo()` | Refaz a última ação desfeita. |
| `SetReplayMode` | `void SetReplayMode(int pReplayMode)` | Define o modo de operação do sistema de Replay/Histórico. |
| `EditFromThisPoint` | `void EditFromThisPoint()` | Permite iniciar a edição a partir do ponto atual no histórico, descartando ações futuras. |
| `ReplayBackwards` | `void ReplayBackwards()` | Retrocede uma etapa na reprodução do histórico de ações. |
| `ReplayForward` | `void ReplayForward()` | Avança uma etapa na reprodução do histórico de ações. |
| `OnScrollWheelReplay` | `void OnScrollWheelReplay(int wParam)` | Processa o evento de rolagem do mouse no painel de replay/histórico. |

---

## 7. Animação e Renderização

Métodos para controle de animação e renderização.

| Método | Assinatura | Descrição |
| :--- | :--- | :--- |
| `SetAnimationMode` | `void SetAnimationMode(int pAnimationMode)` | Define o modo de reprodução da animação (e.g., play, pause, stop). |
| `CreateAnimationFrame` | `void CreateAnimationFrame()` | Cria um novo quadro (frame) na animação. |
| `RemoveAnimationFrame` | `void RemoveAnimationFrame()` | Remove o quadro de animação atualmente selecionado. |
| `AnimationForward` | `void AnimationForward()` | Avança para o próximo quadro de animação. |
| `AnimationBackward` | `void AnimationBackward()` | Volta para o quadro de animação anterior. |
| `PlayAnimation` | `void PlayAnimation()` | Inicia a reprodução da animação. |
| `PauseAnimation` | `void PauseAnimation()` | Pausa a reprodução da animação. |
| `RenderLayers` | `void RenderLayers()` | Força a renderização de todas as camadas visíveis na área de trabalho. |
| `UpdateLayers` | `void UpdateLayers(int layerIndexTarget)` | Força a atualização da renderização de uma camada específica. |
| `RenderAnimation` | `void RenderAnimation()` | Renderiza o quadro atual da animação. |

---

## 8. Utilitários e Finalização

| Método | Assinatura | Descrição |
| :--- | :--- | :--- |
| `CreateLogData` | `void CreateLogData(std::string fileName, std::string content)` | Cria ou anexa dados a um arquivo de log. **Nota:** O uso de `std::string` em uma exportação C pode exigir *marshalling* ou *wrapping* para evitar problemas de ABI/compatibilidade. |
| `Cleanup` | `void Cleanup()` | Executa a limpeza e liberação de recursos alocados pela DLL. Deve ser chamado antes de descarregar a DLL. |
| `OnScrollWheelLayers` | `void OnScrollWheelLayers(int wParam)` | Processa o evento de rolagem do mouse no painel de camadas. |

---

## Estruturas de Dados (Structs)

Embora as estruturas de dados não sejam diretamente exportadas como funções, elas definem os tipos de dados complexos que a DLL manipula internamente. O arquivo `Structs.h` revela a complexidade da manipulação de dados, especialmente para ações e camadas.

### `ACTION`
Representa uma ação realizada pelo usuário (desenho, edição, etc.) para fins de histórico (Replay/Undo/Redo).

| Campo | Tipo | Descrição |
| :--- | :--- | :--- |
| `Tool` | `int` | ID da ferramenta usada. |
| `Layer` | `int` | Índice da camada onde a ação ocorreu. |
| `FrameIndex` | `int` | Índice do quadro de animação. |
| `isLayerVisible` | `int` | Estado de visibilidade da camada. |
| `Position` | `D2D1_RECT_F` | Posição ou área de um retângulo (Direct2D). |
| `FreeForm` | `EDGE` | Dados de forma livre (traços de pincel). |
| `Ellipse` | `D2D1_ELLIPSE` | Dados de uma elipse (Direct2D). |
| `FillColor` | `COLORREF` | Cor de preenchimento. |
| `Color` | `COLORREF` | Cor da linha/traço. |
| `Line` | `LINE` | Pontos inicial e final de uma linha. |
| `BrushSize` | `int` | Tamanho do pincel. |
| `IsFilled` | `bool` | Indica se a forma está preenchida. |
| `isPixelMode` | `bool` | Indica se o modo pixel estava ativo. |
| `Text` | `std::wstring` | Conteúdo de texto. |
| `FontFamily`, `FontSize`, etc. | Vários | Propriedades da fonte para a ferramenta de texto. |

### `Layer`
Representa uma camada individual no projeto.

| Campo | Tipo | Descrição |
| :--- | :--- | :--- |
| `LayerID` | `int` | Identificador único da camada. |
| `FrameIndex` | `int` | Índice do quadro de animação associado. |
| `isActive` | `bool` | Indica se a camada está visível/ativa. |
| `pBitmap` | `ID2D1Bitmap1` | Ponteiro para o bitmap de renderização da camada (Direct2D). |

### Outras Estruturas

| Estrutura | Uso Principal |
| :--- | :--- |
| `VERTICE` | Coordenadas (`x`, `y`) e tamanho do pincel (`BrushSize`) para traços de forma livre. |
| `EDGE` | Contém um vetor de `VERTICE`s, representando um traço completo. |
| `LINE` | Contém os pontos `startPoint` e `endPoint` (`D2D1_POINT_2F`) para uma linha reta. |
| `LayerOrder` | Usada para gerenciar a ordem de renderização das camadas. |
| `PIXEL` | Usada para coordenadas de pixel, provavelmente para operações de baixo nível como o Balde de Tinta. |
| `LayerButton` | Usada para gerenciar os botões da UI de camadas, incluindo o handle da janela (`HWND`) e o contexto de renderização. |
| `TimelineFrameButton` | Usada para gerenciar os botões da UI da linha do tempo de animação. |
| `RenderData` | Contém informações e objetos de contexto de renderização (Direct2D). |
