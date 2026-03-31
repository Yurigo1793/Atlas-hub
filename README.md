# AtlasHub

## Visão geral
O **AtlasHub** é uma aplicação desktop para Windows desenvolvida em **C++** com **Qt 6 (Qt Widgets)**, projetada para ser um assistente de produtividade leve, rápido e modular.

O foco do projeto é centralizar fluxos de captura e processamento de texto, automação de documentos e planilhas, e manipulação de tarefas com atalhos intuitivos, em uma base arquitetural preparada para crescimento contínuo.

## Propósito do projeto
O AtlasHub existe para oferecer uma base sólida para um assistente desktop que permita:

- Capturar informações textuais da tela de forma eficiente.
- Aplicar processamento e transformação de texto com baixo atrito.
- Evoluir para fluxos de automação orientados por conteúdo.
- Organizar e exportar dados em formatos úteis para rotinas de trabalho.

## Funcionalidades

### Funcionalidades atuais
- Interface gráfica desktop com **Qt Widgets**.
- Estrutura modular com separação clara de responsabilidades.
- Camadas base para captura de tela e OCR (em modo estrutural/placeholder).
- Orquestração central da aplicação via controller (`AppController`).

### Funcionalidades planejadas
- OCR de tela (captura + extração de texto).
- Atalhos globais para ações rápidas.
- Automação baseada em texto.
- Exportação para **TXT**, **CSV** e **PDF**.
- Sistema de configurações mais completo.
- Processamento e organização de documentos.

## Tecnologias utilizadas
- **C++17**
- **Qt 6 (Qt Widgets)**
- **CMake**
- **MinGW (Windows)**

## Requisitos do sistema

### Ambiente de desenvolvimento
- Windows 10 ou superior.
- Qt 6 com componente **Qt Widgets** instalado.
- Compilador **MinGW** compatível com Qt 6.
- CMake 3.21 ou superior.
- Qt Creator (recomendado para fluxo de build e depuração).

### Dependências de runtime
- Bibliotecas Qt 6 necessárias para execução do binário gerado.

## Como compilar e executar

### Opção 1: Qt Creator (recomendado)
1. Abra o **Qt Creator**.
2. Selecione **Open Project** e escolha o arquivo `CMakeLists.txt` na raiz do repositório.
3. Escolha um kit com **Qt 6 + MinGW**.
4. Aguarde a configuração do CMake.
5. Compile o projeto com **Build**.
6. Execute com **Run**.

### Opção 2: Linha de comando com CMake
No terminal, na raiz do projeto:

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
./build/AtlasHub.exe
```

Observações:
- Se o CMake não localizar o Qt 6, ajuste `CMAKE_PREFIX_PATH` para o diretório de instalação do Qt.
- O nome final do executável pode variar conforme configuração do gerador/ambiente.

## Roadmap

### Fase 1 — Base arquitetural
- Estrutura modular inicial.
- UI principal com fluxo básico de interação.
- Stubs para captura e OCR.

### Fase 2 — OCR funcional
- Implementação de captura real de tela.
- Integração com engine de OCR.
- Exibição e tratamento inicial do texto extraído.

### Fase 3 — Produtividade e automação
- Atalhos globais.
- Rotinas de automação orientadas por texto.
- Aprimoramento do sistema de configurações.

### Fase 4 — Documentos e exportação
- Exportação para TXT/CSV/PDF.
- Organização de conteúdo e fluxos de documentos.
- Evolução para arquitetura com módulos/plugins adicionais.

## Diretrizes de contribuição
Contribuições são bem-vindas e devem manter o foco em qualidade de código e consistência arquitetural.

1. Abra uma issue descrevendo problema, melhoria ou nova proposta.
2. Crie uma branch específica para sua alteração.
3. Mantenha a separação de responsabilidades (UI, core, modules, utils).
4. Garanta que o projeto configure e compile com Qt 6 + CMake.
5. Envie um Pull Request com descrição clara de motivação, impacto e validação.

## Licença
Este repositório está sob licença **a definir**.

Placeholder sugerido até a definição oficial:

`License: TBD`
