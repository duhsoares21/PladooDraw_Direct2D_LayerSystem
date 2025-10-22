# Documentação da API da DLL de Desenho

Bem-vindo à documentação da API da DLL de Desenho. Este repositório contém todas as informações necessárias para inicializar, configurar e interagir com os métodos exportados pela biblioteca.

A documentação está organizada em módulos temáticos para facilitar a consulta.

## Índice da Documentação

| Documento | Descrição | Foco |
| :--- | :--- | :--- |
| [**API_Documentation.md**](API_Documentation.md) | Referência completa de todos os métodos externos (`extern "C" __declspec(dllexport)`) definidos em `Main.h`. Inclui assinaturas, parâmetros e descrições de funcionalidade. | **Referência de Funções** |
| [**Workflow_Guide.md**](Workflow_Guide.md) | Guia prático que descreve a sequência de chamadas recomendada para inicialização da DLL e o fluxo de trabalho de interação (desenho, animação, histórico). | **Guia de Uso Prático** |
| [**Tool_IDs_Documentation.md**](Tool_IDs_Documentation.md) | Mapeamento essencial dos IDs numéricos para cada ferramenta (Pincel, Borracha, Retângulo, etc.), conforme definido em `Tools.h`. Crucial para usar a função `SetSelectedTool`. | **Constantes de Ferramentas** |
| [**Auxiliary_Data_Structures.md**](Auxiliary_Data_Structures.md) | Detalhamento das estruturas de dados principais (`ACTION`, `Layer`, `VERTICE`) definidas em `Structs.h`. Essencial para entender o modelo de dados interno. | **Estruturas de Dados** |
| [**Global_Variables_Documentation.md**](Global_Variables_Documentation.md) | Documentação das variáveis globais de estado (Handles de Janela, fatores de zoom, cor atual, etc.) definidas em `Constants.h`. | **Estado Interno** |

---

## Como Começar

Para integrar a DLL em sua aplicação, comece consultando o [**Workflow Guide**](Workflow_Guide.md) para entender a sequência correta de inicialização.

Em seguida, use a [**API Documentation**](API_Documentation.md) como referência para as chamadas de função específicas.

## Estrutura do Projeto

A documentação foi gerada a partir dos seguintes arquivos de cabeçalho:

*   `Main.h`: Define as funções exportadas (API).
*   `Structs.h`: Define as estruturas de dados internas.
*   `Constants.h`: Define as variáveis globais de estado e UI.
*   `Tools.h`: Define os IDs das ferramentas.

> **Nota:** A DLL utiliza as APIs gráficas Direct2D/DirectWrite/Direct3D. A aplicação host deve garantir a compatibilidade e o gerenciamento correto dos Handles de Janela (`HWND`) passados à DLL.
