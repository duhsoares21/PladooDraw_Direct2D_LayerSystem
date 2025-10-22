# Guia de Fluxo de Trabalho e Inicialização da DLL

Este guia descreve o fluxo de trabalho recomendado para a inicialização, configuração e uso dos principais recursos da DLL, com base nas funções exportadas em `Main.h` e nas variáveis globais em `Constants.h`.

## 1. Sequência de Inicialização Recomendada

A DLL exige que vários componentes sejam inicializados em uma ordem específica para garantir que as dependências (como Handles de Janela e contextos de renderização) estejam prontas.

### 1.1. Inicialização do Core

A inicialização deve começar com a preparação do ambiente principal e a definição da área de trabalho.

| Passo | Função da API | Parâmetros Chave | Propósito |
| :--- | :--- | :--- | :--- |
| **1** | `Initialize` | `pmainHWND` (Handle da janela principal) | Inicializa o *core* da DLL e os objetos de renderização Direct2D/Direct3D. |
| **2** | `InitializeDocument` | `hWnd` (Handle da janela de desenho), `pWidth`, `pHeight`, `pPixelSizeRatio`, `pBtnWidth`, `pBtnHeight` | Define as dimensões do documento e inicializa o contexto de desenho principal. |
| **3** | `Resize` | Nenhum | Chamado após a inicialização do documento para garantir que todos os componentes internos se ajustem ao tamanho da janela de desenho. |

### 1.2. Inicialização dos Módulos

Após a inicialização do *core*, os módulos específicos (Camadas, Ferramentas, Replay, etc.) podem ser inicializados.

| Passo | Função da API | Parâmetros Chave | Propósito |
| :--- | :--- | :--- | :--- |
| **4** | `InitializeLayers` | `pLayerWindow`, `pLayers`, `pControlButtons` (Handles de janela da UI de camadas) | Inicializa o subsistema de gerenciamento de camadas. |
| **5** | `InitializeLayersButtons` | `buttonsHwnd` (Ponteiro para array de Handles) | Inicializa os controles de UI para botões de camada. |
| **6** | `InitializeLayerRenderPreview` | Nenhum | Inicializa o subsistema de pré-visualização de camadas. |
| **7** | `InitializeTools` | `hWnd` (Handle da janela de desenho) | Inicializa o subsistema de ferramentas de desenho. |
| **8** | `InitializeWrite` | Nenhum | Inicializa o subsistema de texto/escrita. |
| **9** | `InitializeReplay` | `hWnd` (Handle da janela de desenho) | Inicializa o subsistema de histórico de ações (Undo/Redo/Replay). |
| **10** | `InitializeSurfaceDial` | `pmainHWND` (Handle da janela principal) | Inicializa a integração com o Microsoft Surface Dial (se aplicável). |

> **Fluxo Mínimo:** Para uma operação básica de desenho sem UI de camadas ou replay, os passos 1, 2, 3 e 7 são essenciais.

## 2. Fluxo de Trabalho de Desenho e Interação

O desenho interativo segue um padrão de três etapas, tipicamente mapeado aos eventos de mouse da aplicação host:

### 2.1. Seleção da Ferramenta

Antes de desenhar, a ferramenta deve ser selecionada.

| Ação | Função da API | Detalhes |
| :--- | :--- | :--- |
| **Selecionar Ferramenta** | `SetSelectedTool(int pselectedTool)` | Define qual ferramenta está ativa. **Nota:** O ID (`pselectedTool`) deve ser um valor previamente definido, que não foi encontrado nos arquivos fornecidos. A aplicação host deve ter esse mapeamento. |
| **Configurar Ferramenta** | `IncreaseBrushSize`, `DecreaseBrushSize`, `SetFont`, etc. | Ajusta as propriedades da ferramenta selecionada (tamanho, cor, fonte). A cor deve ser definida via `currentColor` (variável global). |

### 2.2. Interação (Mouse Down / Mouse Move)

A interação de desenho começa com um clique e continua com o movimento do mouse.

| Evento | Função da API | Propósito |
| :--- | :--- | :--- |
| **Mouse Down** | `SelectTool(int xInitial, int yInitial)` | Inicia a ação de desenho ou seleção na posição inicial. |
| **Mouse Move** | `MoveTool(int xInitial, int yInitial, int x, int y)` | Atualiza a visualização da ação (e.g., a linha temporária sendo desenhada, o retângulo de seleção) enquanto o mouse é arrastado. |
| **Desenho Contínuo** | `BrushTool`, `EraserTool` | Para ferramentas de traço contínuo (Pincel, Borracha), estas funções devem ser chamadas repetidamente no evento `Mouse Move` da aplicação host para registrar cada ponto do traço. |

### 2.3. Finalização da Ação (Mouse Up)

A ação é registrada no histórico e o desenho é finalizado.

| Evento | Função da API | Propósito |
| :--- | :--- | :--- |
| **Mouse Up** | `UnSelectTool()` e/ou `handleMouseUp()` | Finaliza a interação com a ferramenta. A ação é consolidada, registrada no histórico (`Actions` global) e o desenho final é aplicado à camada. |

## 3. Gerenciamento de Camadas

A DLL gerencia a ordem e o conteúdo das camadas.

| Tarefa | Sequência de Chamadas |
| :--- | :--- |
| **Criar Nova Camada** | 1. Chamar `AddLayer(false, ...)` para criar a camada. <br> 2. Chamar `AddLayerButton(...)` para adicionar o controle de UI correspondente. |
| **Alterar Camada Ativa** | Chamar `SetLayer(int index)` para definir a camada onde as próximas ações de desenho ocorrerão. |
| **Reordenar Camadas** | Chamar `ReorderLayerUp()` ou `ReorderLayerDown()` para mover a camada ativa. |
| **Alternar Visibilidade** | Chamar `IsLayerActive(...)` para verificar o estado e, em seguida, chamar uma função interna (não exposta) para alternar a visibilidade, seguida de `RenderLayers()` para atualizar a visualização. |

## 4. Fluxo de Trabalho de Animação

O subsistema de animação é centrado em quadros (`FrameIndex`) e modos de reprodução.

| Tarefa | Sequência de Chamadas |
| :--- | :--- |
| **Criar Quadro** | Chamar `CreateAnimationFrame()`. |
| **Navegar** | Chamar `AnimationForward()` ou `AnimationBackward()`. |
| **Reproduzir** | 1. Chamar `SetAnimationMode(...)` para definir o modo de reprodução. <br> 2. Chamar `PlayAnimation()`. <br> 3. Chamar `PauseAnimation()` para parar a reprodução. |
| **Renderizar** | Chamar `RenderAnimation()` para forçar a renderização do quadro atual no *viewport*. |

## 5. Fluxo de Trabalho de Histórico (Undo/Redo)

As ações são registradas automaticamente na variável global `Actions`.

| Tarefa | Função da API |
| :--- | :--- |
| **Desfazer** | `Undo()` |
| **Refazer** | `Redo()` |
| **Modo Replay** | Chamar `SetReplayMode(...)` para ativar o modo de gravação ou reprodução de histórico. |
| **Edição a Partir do Ponto** | Chamar `EditFromThisPoint()` para descartar o histórico de `RedoActions` e começar uma nova linha do tempo de ações. |

## 6. Finalização

Ao fechar a aplicação host ou descarregar a DLL, a função de limpeza deve ser chamada para liberar recursos de hardware (Direct2D, Direct3D).

| Tarefa | Função da API |
| :--- | :--- |
| **Limpeza** | `Cleanup()` |
| **Propósito** | Libera todos os recursos alocados pela DLL, como objetos Direct2D/Direct3D e memória. |
