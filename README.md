# Atlas-Hub

Assistente de desktop leve, modular e independente para produtividade no Windows.

## Objetivo

Construir um **hub de ferramentas rápidas** para uso diário, com foco em:

- abrir e usar recursos com poucos cliques;
- executar OCR de tela de forma prática;
- acelerar tarefas repetitivas com atalhos;
- manter arquitetura pronta para expansão com novas automações.

## Valores do projeto

- **Leveza:** resposta rápida e baixo consumo de recursos.
- **Modularidade:** componentes separados para facilitar manutenção e evolução.
- **Independência:** evitar dependências externas obrigatórias.
- **Nativo:** priorizar WinAPI e recursos internos do sistema.
- **Clareza:** código limpo, legível e fácil de modificar.
- **Evolução contínua:** base preparada para novas ferramentas no Atlas-Hub.

## Funcionalidades atuais

- Interface gráfica retrô (tema azul) com botões:
  - `OCR TELA`
  - `CONFIG`
  - `SAIR`
- Loop de comandos no console:
  - `ajuda`
  - `ocr_tela`
  - `sair`
- OCR com fluxo nativo Windows (`Windows.Media.Ocr`) quando disponível.
- Captura de tela com WinAPI (`BitBlt`) em tela inteira ou por região.
- Atalhos globais preparados:
  - `Ctrl + Shift + A` → abrir interface
  - `Ctrl + Shift + \` → ocultar interface, selecionar região na tela e aplicar OCR

## Estrutura

```text
atlas/
  core/
  ui/
  input/
  utils/
  main.cpp
```

## Build (CMake)

```bash
cmake -S . -B build
cmake --build build -j4
```
