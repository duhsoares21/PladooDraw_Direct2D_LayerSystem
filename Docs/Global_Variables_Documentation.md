# Documentação de Variáveis Globais e Definições de UI

Este documento detalha as principais variáveis globais e constantes de definição de UI (Interface do Usuário) declaradas no arquivo `Constants.h`. Essas variáveis representam o estado interno da DLL e os recursos do sistema (como Handles de Janela e objetos Direct2D/DirectWrite) que são gerenciados pela DLL.

> **Atenção:** Como estas são variáveis globais, a manipulação direta por código externo é **desencorajada**. O acesso a elas deve ser feito prioritariamente através dos métodos da API documentados em `API_Documentation.md`.

## 1. Constantes de Definição de UI

Estas constantes são usadas para definir as dimensões padrão de elementos da interface, como botões.

| Constante | Valor Típico | Descrição |
| :--- | :--- | :--- |
| `BTN_WIDTH_DEFAULT` | 120 | Largura padrão para botões. |
| `BTN_WIDTH_WIDE_DEFAULT` | 190 | Largura estendida para botões, possivelmente para botões com rótulos mais longos. |
| `BTN_HEIGHT_DEFAULT` | 120 | Altura padrão para botões. |
| `BTN_HEIGHT_WIDE_DEFAULT` | 120 | Altura estendida para botões. |

## 2. Handles de Janela (HWNDs)

A DLL gerencia o estado de diversos componentes da UI através de seus Handles de Janela (HWNDs). A aplicação host é responsável por fornecer e gerenciar essas janelas, mas a DLL as utiliza para renderização e processamento de eventos.

| Variável Global | Tipo | Descrição |
| :--- | :--- | :--- |
| `mainHWND` | `HWND` | Handle da janela principal da aplicação host. |
| `docHWND` | `HWND` | Handle da janela onde o documento de desenho principal é renderizado. |
| `layerWindowHWND` | `HWND` | Handle da janela que contém o painel de gerenciamento de camadas. |
| `layersHWND` | `HWND` | Handle do controle que lista as camadas dentro do painel. |
| `layersControlButtonsGroupHWND` | `HWND` | Handle do grupo de botões de controle de camadas (Adicionar, Remover, etc.). |
| `toolsHWND` | `HWND` | Handle da janela que contém a barra de ferramentas ou painel de ferramentas. |
| `timelineHWND` | `HWND` | Handle da janela que contém a linha do tempo de animação/replay. |
| `highlightFrame` | `HWND` | Handle de uma janela/controle usado para destacar o quadro de animação atual. |
| `hLayerButtons` | `HWND*` | Ponteiro para um array de Handles de janelas dos botões individuais de camada. |
| `hTextInput` | `HWND` | Handle do controle de entrada de texto (usado pela ferramenta `WriteTool`). |
| `buttonUp`, `buttonDown`, `buttonPlus`, `buttonMinus` | `HWND` | Handles de janelas para botões de controle específicos (e.g., reordenar camadas, zoom/tamanho do pincel). |

## 3. Variáveis de Estado de Desenho e Configuração

Estas variáveis definem o estado atual da ferramenta, visualização e configurações do pincel.

| Variável Global | Tipo | Descrição |
| :--- | :--- | :--- |
| `currentColor` | `COLORREF` | A cor de primeiro plano atualmente selecionada para desenho. |
| `lastHexColor` | `unsigned int` | A última cor hexadecimal usada (possivelmente para alternância rápida de cores). |
| `defaultBrushSize` | `float` | Tamanho padrão do pincel. |
| `defaultEraserSize` | `float` | Tamanho padrão da borracha. |
| `currentBrushSize` | `float` | Tamanho atual do pincel. |
| `currentEraserSize` | `float` | Tamanho atual da borracha. |
| `selectedTool` | `int` | O ID da ferramenta atualmente selecionada (o mapeamento de ID deve ser fornecido externamente). |
| `isPixelMode` | `bool` | Indica se o modo de desenho pixelizado está ativo. |
| `zoomFactor` | `float` | O fator de zoom atual da área de trabalho. |
| `pixelSizeRatio` | `int` | O fator de escala do pixel (pixel size ratio) para o modo pixel. |
| `dpiX`, `dpiY` | `float` | Resolução de pontos por polegada (DPI) da tela, usado para escala de renderização. |
| `width`, `height` | `int` | Largura e altura atuais do documento de desenho. |

## 4. Variáveis de Estado de Interação

Variáveis que rastreiam o estado de interação do usuário, como o que está sendo desenhado no momento.

| Variável Global | Tipo | Descrição |
| :--- | :--- | :--- |
| `isDrawingRectangle` | `bool` | `true` se um retângulo estiver sendo desenhado ativamente. |
| `isDrawingEllipse` | `bool` | `true` se uma elipse estiver sendo desenhada ativamente. |
| `isDrawingBrush` | `bool` | `true` se um traço de pincel estiver sendo desenhado ativamente. |
| `isDrawingLine` | `bool` | `true` se uma linha estiver sendo desenhada ativamente. |
| `isDrawingWindowText` | `bool` | `true` se o texto da janela estiver sendo desenhado (possivelmente um texto temporário de visualização). |
| `isWritingText` | `bool` | `true` se a ferramenta de texto estiver ativa e pronta para entrada. |
| `isPaintBucket` | `bool` | `true` se a ferramenta Balde de Tinta estiver ativa. |
| `mouseLastClickPosition` | `POINT` | Coordenadas do último clique do mouse. |
| `prevLeft`, `prevTop` | `int` | Coordenadas anteriores (usadas para rastrear o movimento do mouse). |

## 5. Variáveis de Estado de Animação e Histórico (Replay)

| Variável Global | Tipo | Descrição |
| :--- | :--- | :--- |
| `CurrentFrameIndex` | `int` | O índice do quadro de animação atualmente selecionado. |
| `isReplayMode` | `int` | O modo de operação do sistema de Replay/Histórico. |
| `isAnimationMode` | `int` | O modo de operação da animação. |
| `isPlayingAnimation` | `bool` | Indica se a animação está sendo reproduzida no momento. |
| `lastActiveReplayFrame` | `int` | O último quadro ativo no histórico de replay. |
| `Actions` | `std::vector<ACTION>` | O histórico principal de ações (usado para Undo). |
| `RedoActions` | `std::vector<ACTION>` | O histórico de ações desfeitas (usado para Redo). |
| `ReplayActions` | `std::vector<ACTION>` | Histórico de ações específico para o modo Replay. |
| `ReplayRedoActions` | `std::vector<ACTION>` | Histórico de ações desfeitas no modo Replay. |

## 6. Variáveis de Gerenciamento de Camadas e Recursos

| Variável Global | Tipo | Descrição |
| :--- | :--- | :--- |
| `layersOrder` | `std::vector<LayerOrder>` | A ordem de renderização das camadas. |
| `layerBitmaps` | `std::vector<Layer>` | Coleção de camadas (incluindo seus bitmaps de renderização). |
| `layers` | `std::vector<std::optional<Layer>>` | Camadas principais do documento. |
| `animationLayers` | `std::vector<std::optional<Layer>>` | Camadas específicas para animação. |
| `LayerButtons` | `std::vector<std::optional<LayerButton>>` | Coleção de objetos que gerenciam os botões de camada na UI. |
| `TimelineFrameButtons` | `std::vector<std::optional<TimelineFrameButton>>` | Coleção de objetos que gerenciam os botões de quadro na linha do tempo. |
| `bitmapData` | `std::unordered_map<...>` | Dados brutos do bitmap (possivelmente para acesso rápido a pixels). |
| `loadedFileName` | `std::string` | O nome do arquivo do projeto atualmente carregado. |

## 7. Objetos de Renderização Direct2D/DirectWrite

A DLL utiliza a API Direct2D e DirectWrite para renderização acelerada por hardware. Estas variáveis armazenam os objetos de contexto necessários.

| Variável Global | Tipo | Descrição |
| :--- | :--- | :--- |
| `pD2DFactory` | `ID2D1Factory1` | A fábrica Direct2D principal. |
| `pD2DTargetBitmap` | `ID2D1Bitmap1` | O bitmap de destino para renderização. |
| `pBrush` | `ID2D1SolidColorBrush` | Um pincel de cor sólida Direct2D. |
| `pRenderTarget`, `pRenderTargetLayer` | `ID2D1DeviceContext` | Contextos de dispositivo Direct2D para comandos de desenho. |
| `pDWriteFactory` | `IDWriteFactory` | A fábrica DirectWrite para manipulação de texto. |
| `pTextFormat` | `IDWriteTextFormat` | O formato de texto atual (fonte, tamanho, etc.). |
| **Objetos D3D/DXGI** | | |
| `g_pD3DDevice`, `g_pD3DContext` | `ID3D11Device`, `ID3D11DeviceContext` | Dispositivo e contexto Direct3D (usado como base para Direct2D). |
| `g_pSwapChain` | `IDXGISwapChain1` | Cadeia de troca DXGI para apresentação na tela. |
| `g_pD2DDevice` | `ID2D1Device` | Dispositivo Direct2D. |
| **Objetos DirectComposition** | | |
| `g_pDCompDevice`, `g_pDCompTarget`, `g_pDCompVisual` | `IDComposition...` | Objetos para o uso do DirectComposition (possivelmente para composição de UI ou animações). |
