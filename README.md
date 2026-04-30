# Atlas Hub

Atlas Hub e um aplicativo desktop em Qt/C++ pensado para reunir ferramentas locais em uma unica interface.

Nesta fase inicial, o app possui um modulo de OCR: ele permite selecionar uma area da tela, captura a imagem selecionada e extrai o texto usando uma copia empacotada do Tesseract. O objetivo e que o Atlas Hub cresca com muitos outros modulos no futuro, mantendo a mesma ideia central: utilitarios rapidos, locais e integrados em uma experiencia simples.

## Funcionalidades atuais

- Captura de uma area da tela.
- OCR local com Tesseract empacotado no projeto.
- Suporte aos idiomas `por` e `eng` via `tessdata`.
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
