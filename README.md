# Atlas Hub

Atlas Hub e um aplicativo desktop em Qt/C++ para produtividade de escritorio, reunindo OCR, traducao, historico e ferramentas rapidas de texto em uma interface simples, portatil e integrada ao Windows.

O app permite capturar uma area da tela, extrair texto localmente com Tesseract, traduzir automaticamente ou manualmente, restaurar capturas antigas pelo historico e trabalhar com o texto reconhecido sem depender de uma instalacao global do OCR.

## Funcionalidades

### OCR e captura

- Captura de uma area da tela por selecao visual.
- OCR local com Tesseract empacotado no projeto, sem depender do `PATH` do sistema.
- Suporte aos idiomas `por`, `eng`, `spa` e `fra` via `tessdata`.
- Atalho global configuravel para executar OCR rapidamente.
- Execucao a partir da bandeja do sistema.

### Traducao

- Traducao automatica do texto capturado por OCR usando mirrors gratuitos compativeis com LibreTranslate.
- Traducao manual do texto atual.
- Suporte de traducao para portugues, ingles, espanhol e frances.
- Deteccao automatica do idioma do texto antes da traducao.
- Modo de idiomas inteligentes para alternar automaticamente entre idioma principal e secundario.
- Configuracao de idiomas salva entre execucoes do app.
- Suporte a instancia propria do LibreTranslate via variaveis de ambiente.

### Historico

- Historico das capturas de OCR e suas traducoes.
- Restauracao de capturas antigas com duplo clique.
- Busca no historico por texto OCR ou texto traduzido.
- Filtro por data: todas as datas, hoje, ultimos 7 dias e ultimos 30 dias.
- Favoritos no historico.
- Filtro para mostrar apenas favoritos.
- Persistencia dos favoritos junto dos itens do historico.

### Edicao e produtividade

- Campos editaveis para ajustar o texto OCR e a traducao.
- Botoes rapidos para copiar texto OCR e traducao.
- Barra de formatacao com fonte, tamanho, negrito, italico, sublinhado e cor do texto.
- Menu de ajuda com link para suporte pelo repositorio.

### Interface e sistema

- Interface em portugues, ingles e frances.
- Tema claro e tema escuro.
- Opcao para iniciar com o Windows.
- Opcao para iniciar minimizado na bandeja do sistema.
- Build portatil com as DLLs do Qt e do Tesseract copiadas para a pasta do executavel.
- Execucao no Windows sem abrir janela de console.

## Estrutura importante

O Tesseract usado pelo app deve ficar versionado dentro do projeto:

```text
third_party/tesseract/
```

Essa pasta deve conter:

- `tesseract.exe`
- DLLs necessarias do Tesseract
- `tessdata/`
- `tessdata/por.traineddata`
- `tessdata/eng.traineddata`
- `tessdata/spa.traineddata`
- `tessdata/fra.traineddata`

Durante o build, o CMake copia automaticamente essa pasta para o diretorio do executavel:

```text
<build>/third_party/tesseract/
```

O codigo sempre carrega o Tesseract a partir da pasta do executavel, usando `QCoreApplication::applicationDirPath()`. O app nao usa o `PATH` do sistema e nao tenta usar uma instalacao global do Tesseract.

## Requisitos

- Windows
- Qt instalado, na mesma versao do projeto ou em versao compativel
- Compilador configurado no Qt Creator, como MinGW ou MSVC
- CMake funcionando pelo kit do Qt Creator
- Pasta `third_party/tesseract/` presente no repositorio
- Acesso a pelo menos um mirror gratuito compativel com LibreTranslate. O app tenta automaticamente:
  `https://translate.argosopentech.com/translate`, `https://translate.libregalaxy.org/translate`,
  `https://translate.fedilab.app/translate` e `https://translate.cutie.dating/translate`.

Opcionalmente, aponte o app para outra instancia compativel:

```powershell
$env:LIBRETRANSLATE_URL = "http://localhost:5000/translate"
.\build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug\AtlasHub.exe
```

Se a instancia escolhida exigir chave, configure tambem:

```powershell
$env:LIBRETRANSLATE_API_KEY = "sua-chave-aqui"
```

## Build pelo Qt Creator

1. Abra o Qt Creator.
2. Clique em `File > Open File or Project`.
3. Selecione o arquivo `CMakeLists.txt` na raiz do projeto.
4. Escolha um kit compativel, por exemplo `Desktop Qt MinGW 64-bit`.
5. Aguarde o Qt Creator configurar o CMake.
6. Clique em `Build`.
7. Ao final do build, o executavel ficara dentro da pasta `build`.

Depois do build, confira se a estrutura abaixo existe ao lado do executavel:

```text
AtlasHub.exe
third_party/tesseract/tesseract.exe
third_party/tesseract/tessdata/por.traineddata
third_party/tesseract/tessdata/eng.traineddata
```

## Build pela linha de comando

Exemplo usando o kit MinGW instalado com o Qt:

```powershell
$env:Path = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\CMake_64\bin;" + $env:Path

C:\Qt\Tools\CMake_64\bin\cmake.exe `
  -S . `
  -B build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug `
  -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.0/mingw_64"

C:\Qt\Tools\CMake_64\bin\cmake.exe `
  --build build\Desktop_Qt_6_11_0_MinGW_64_bit-Debug `
  --config Debug
```

Se estiver usando outra versao do Qt, ajuste os caminhos para a versao instalada na sua maquina.

## Build limpo

Para testar como se fosse a primeira vez:

1. Apague a pasta `build/`.
2. Abra o projeto novamente no Qt Creator ou rode os comandos de build.
3. Compile o projeto.
4. Verifique se `third_party/tesseract` foi copiado para a pasta do executavel.
5. Execute o app e teste o OCR.

## Observacoes para distribuicao portatil

Para distribuir o app, use a pasta onde esta o `AtlasHub.exe` depois do build. Ela deve incluir as DLLs do Qt, plugins do Qt e a pasta `third_party/tesseract`.

Nao remova `third_party/tesseract` da pasta do executavel, pois o OCR depende somente da versao empacotada.
