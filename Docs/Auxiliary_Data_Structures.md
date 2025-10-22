# Documentação Auxiliar: Estruturas de Dados Internas (`Structs.h`)

Este documento detalha as principais estruturas de dados definidas no arquivo `Structs.h`. Embora estas estruturas não sejam diretamente expostas como parâmetros na maioria das funções da API (conforme `Main.h`), elas definem o modelo de dados interno da DLL, sendo cruciais para entender como as ações, camadas e elementos gráficos são representados.

## 1. Estruturas de Geometria Básica

Estas estruturas são usadas para definir pontos, linhas e formas de desenho.

### `VERTICE`

Representa um ponto individual em um traço de forma livre, contendo a localização e o tamanho do pincel naquele ponto.

| Campo | Tipo | Descrição |
| :--- | :--- | :--- |
| `x` | `float` | Coordenada X do ponto. |
| `y` | `float` | Coordenada Y do ponto. |
| `BrushSize` | `int` | O tamanho do pincel (espessura do traço) no momento da criação deste vértice. |

### `EDGE`

Representa um traço completo de forma livre (como um desenho contínuo com a ferramenta Pincel ou Lápis).

| Campo | Tipo | Descrição |
| :--- | :--- | :--- |
| `vertices` | `std::vector<VERTICE>` | Uma coleção ordenada de vértices que compõem o traço. |

### `LINE`

Representa uma linha reta simples, usando o tipo de ponto Direct2D.

| Campo | Tipo | Descrição |
| :--- | :--- | :--- |
| `startPoint` | `D2D1_POINT_2F` | Ponto inicial da linha (coordenadas X e Y em ponto flutuante). |
| `endPoint` | `D2D1_POINT_2F` | Ponto final da linha (coordenadas X e Y em ponto flutuante). |

### `PIXEL`

Estrutura simples para representar coordenadas de pixel, usada internamente, possivelmente em operações de baixo nível como o preenchimento (Paint Bucket).

| Campo | Tipo | Descrição |
| :--- | :--- | :--- |
| `x` | `int` | Coordenada X do pixel. |
| `y` | `int` | Coordenada Y do pixel. |

> **Nota:** A estrutura `PIXEL` inclui sobrecarga do operador `==` e uma especialização de `std::hash`, indicando que ela é usada como chave em estruturas de dados hash (como `std::unordered_map` ou `std::unordered_set`), o que é comum em algoritmos de preenchimento (flood fill).

---

## 2. Estrutura de Ação (`ACTION`)

Esta é a estrutura mais complexa, pois encapsula todos os dados necessários para representar uma única ação do usuário no sistema de histórico (Replay/Undo/Redo).

| Campo | Tipo | Descrição |
| :--- | :--- | :--- |
| `Tool` | `int` | ID da ferramenta que gerou a ação. |
| `Layer` | `int` | Índice da camada onde a ação ocorreu. |
| `FrameIndex` | `int` | Índice do quadro de animação associado. |
| `isLayerVisible` | `int` | Estado de visibilidade da camada após a ação. |
| `Position` | `D2D1_RECT_F` | Retângulo delimitador (bounding box) da ação (usado para retângulos, elipses ou seleção). |
| `FreeForm` | `EDGE` | Dados do traço de forma livre (se a ferramenta for Pincel ou Lápis). |
| `Ellipse` | `D2D1_ELLIPSE` | Dados da elipse (se a ferramenta for Elipse). |
| `FillColor` | `COLORREF` | Cor de preenchimento. |
| `Color` | `COLORREF` | Cor do traço ou contorno. |
| `Line` | `LINE` | Dados da linha (se a ferramenta for Linha). |
| `BrushSize` | `int` | Tamanho do pincel/traço usado. |
| `IsFilled` | `bool` | Indica se a forma deve ser preenchida. |
| `isPixelMode` | `bool` | Indica se a ação ocorreu no modo pixel. |
| `mouseX`, `mouseY` | `int` | Coordenadas do mouse (possivelmente para ações pontuais ou de clique). |
| `pixelsToFill` | `std::vector<POINT>` | Lista de pixels a serem preenchidos (usado pela ferramenta Balde de Tinta). |
| **Campos de Texto** | | |
| `Text` | `std::wstring` | O conteúdo do texto inserido. |
| `FontFamily` | `std::wstring` | Família da fonte (e.g., "Arial"). |
| `FontSize` | `int` | Tamanho da fonte em DIPs. |
| `FontWeight` | `int` | Peso da fonte (mapeado para `DWRITE_FONT_WEIGHT`). |
| `FontItalic`, `FontUnderline`, `FontStrike` | `bool` | Estilos de formatação de texto. |

---

## 3. Estruturas de Gerenciamento de Camadas e UI

Estas estruturas são usadas para gerenciar a representação interna das camadas e seus componentes de interface.

### `Layer`

Define uma camada individual de desenho.

| Campo | Tipo | Descrição |
| :--- | :--- | :--- |
| `LayerID` | `int` | Identificador único da camada. |
| `FrameIndex` | `int` | Quadro de animação ao qual esta camada pertence. |
| `isActive` | `bool` | Indica se a camada está visível e editável. |
| `pBitmap` | `Microsoft::WRL::ComPtr<ID2D1Bitmap1>` | Ponteiro inteligente para o bitmap Direct2D que armazena o conteúdo renderizado da camada. |

### `LayerOrder`

Usada para gerenciar a ordem de renderização das camadas.

| Campo | Tipo | Descrição |
| :--- | :--- | :--- |
| `layerOrder` | `int` | A posição atual da camada na ordem de renderização (z-index). |
| `layerIndex` | `int` | O índice interno da camada. |

### `LayerButton`

Representa um botão de camada na interface do usuário.

| Campo | Tipo | Descrição |
| :--- | :--- | :--- |
| `LayerID` | `int` | ID da camada associada. |
| `FrameIndex` | `int` | Índice do quadro de animação. |
| `button` | `HWND` | Handle da janela do botão na UI. |
| `deviceContext`, `swapChain` | `ComPtr` | Componentes Direct2D/DXGI para renderização da pré-visualização do botão. |
| `isActive` | `bool` | Estado de ativação/seleção do botão. |

### `TimelineFrameButton`

Representa um botão de quadro na linha do tempo da animação.

| Campo | Tipo | Descrição |
| :--- | :--- | :--- |
| `LayerIndex` | `int` | Índice da camada associada. |
| `FrameIndex` | `int` | Índice do quadro de animação. |
| `frame` | `HWND` | Handle da janela do botão na UI. |
| `deviceContext`, `swapChain` | `ComPtr` | Componentes Direct2D/DXGI para renderização. |
| `bitmap` | `ComPtr<ID2D1Bitmap1>` | Bitmap de pré-visualização do quadro. |

---

## 4. Estruturas de Renderização (`RenderData`)

Contém informações e objetos de contexto de renderização do Direct2D, essenciais para o pipeline gráfico.

| Campo | Tipo | Descrição |
| :--- | :--- | :--- |
| `size` | `D2D1_SIZE_U` | Dimensões da área de renderização. |
| `deviceContext` | `ComPtr<ID2D1DeviceContext>` | Contexto do dispositivo Direct2D para comandos de desenho. |
| `swapChain` | `ComPtr<IDXGISwapChain1>` | Cadeia de troca DXGI, usada para apresentar o resultado na tela. |
| `bitmap` | `ComPtr<ID2D1Bitmap1>` | O bitmap de destino para a renderização. |
