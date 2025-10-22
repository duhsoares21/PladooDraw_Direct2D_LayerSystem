# Documentação de IDs de Ferramentas

Este documento fornece o mapeamento dos IDs de ferramentas (Tool IDs) necessários para interagir com a função `SetSelectedTool(int pselectedTool)` da API, conforme definido no `enum Tools` do arquivo `Tools.h`.

## Mapeamento de IDs de Ferramentas

O `enum Tools` define os IDs como inteiros sequenciais, onde o primeiro elemento (`TEraser`) assume o valor 0 (zero) por padrão.

| Ferramenta | Constante (`Tools`) | ID (Valor Inteiro) | Descrição |
| :--- | :--- | :--- | :--- |
| Borracha | `TEraser` | **0** | Usada para apagar conteúdo da camada ativa. |
| Pincel | `TBrush` | **1** | Usada para desenho de forma livre com cor e tamanho definidos. |
| Retângulo | `TRectangle` | **2** | Usada para desenhar formas retangulares. |
| Elipse | `TEllipse` | **3** | Usada para desenhar formas elípticas. |
| Linha | `TLine` | **4** | Usada para desenhar linhas retas entre dois pontos. |
| Balde de Tinta | `TPaintBucket` | **5** | Usada para preencher áreas contíguas com uma cor. |
| Mover | `TMove` | **6** | Usada para mover ou transformar elementos na área de trabalho. |
| Escrita/Texto | `TWrite` | **7** | Usada para inserir ou editar texto. |
| Camada | `TLayer` | **8** | Usada para interações específicas de camada (e.g., seleção ou manipulação de objetos na camada). |

### Uso na API

Para selecionar uma ferramenta, a aplicação host deve chamar a função `SetSelectedTool` com o ID correspondente:

```c
// Exemplo: Selecionar a ferramenta Pincel
SetSelectedTool(1); 

// Exemplo: Selecionar a ferramenta Escrita/Texto
SetSelectedTool(7);
```

## Funções de Ferramenta Auxiliares

O arquivo `Tools.h` também lista funções auxiliares que são, presumivelmente, *wrappers* internos chamados pela DLL após a seleção da ferramenta e eventos de mouse. Embora não sejam exportadas diretamente em `Main.h` (com exceção das funções de interação), elas confirmam a funcionalidade de cada ferramenta.

| Função Auxiliar | Descrição |
| :--- | :--- |
| `TEraserTool(...)` | Executa a operação de borracha em uma coordenada. |
| `TBrushTool(...)` | Executa a operação de pincel em uma coordenada com parâmetros de cor e modo pixel. |
| `TRectangleTool(...)` | Executa o desenho de um retângulo. |
| `TEllipseTool(...)` | Executa o desenho de uma elipse. |
| `TLineTool(...)` | Executa o desenho de uma linha. |
| `TPaintBucketTool(...)` | Executa a operação de preenchimento (balde de tinta). |
| `TWriteTool(...)` | Inicia a operação de escrita/texto. |
| `TWriteToolCommitText()` | Finaliza a edição e aplica o texto à camada. |
| `TInitTextTool(...)` | Inicializa o contexto para a ferramenta de texto. |

### Funções de Interação Exportadas (Revisão)

As funções de interação que gerenciam o ciclo de vida de uma ação de ferramenta são exportadas em `Main.h` e usam o ID da ferramenta selecionada:

| Função | Uso |
| :--- | :--- |
| `SelectTool(int xInitial, int yInitial)` | Chamada no evento **Mouse Down** para iniciar a ação da ferramenta selecionada. |
| `MoveTool(int xInitial, int yInitial, int x, int y)` | Chamada no evento **Mouse Move** para atualizar a ação (e.g., arrastar o retângulo). |
| `UnSelectTool()` | Chamada no evento **Mouse Up** para finalizar a ação da ferramenta e aplicar o resultado final. |
