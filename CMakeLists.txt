cmake_minimum_required(VERSION 3.21)

project(AtlasHub VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Widgets QUIET)

if(NOT Qt6_FOUND)
    message(FATAL_ERROR
        "Qt6 (Widgets) nao foi encontrado.\n"
        "Defina CMAKE_PREFIX_PATH apontando para o diretorio de instalacao do Qt.\n"
        "Exemplos:\n"
        "  - Windows (Qt online installer): -DCMAKE_PREFIX_PATH=C:/Qt/6.8.2/mingw_64\n"
        "  - Linux (apt): geralmente instalado automaticamente via pacote qt6-base-dev\n"
        "Tambem e possivel definir Qt6_DIR para a pasta que contem Qt6Config.cmake."
    )
endif()

set(ATLASHUB_SOURCES
    main.cpp
    core/AppController.cpp
    core/HotkeyManager.cpp
    ui/MainWindow.cpp
    ui/OCRWindow.cpp
    modules/ocr/OCRManager.cpp
    modules/capture/ScreenCapture.cpp
    modules/capture/ScreenOverlay.cpp
    utils/Logger.cpp
    utils/ConfigManager.cpp
)

set(ATLASHUB_HEADERS
    core/AppController.h
    core/HotkeyManager.h
    ui/MainWindow.h
    ui/OCRWindow.h
    modules/ocr/OCRManager.h
    modules/capture/ScreenCapture.h
    modules/capture/ScreenOverlay.h
    utils/Logger.h
    utils/ConfigManager.h
)

set(ATLASHUB_UIS
    ui/MainWindow.ui
    ui/OCRWindow.ui
)

set(ATLASHUB_RESOURCES
    resources/branding.qrc
)

find_path(TESSERACT_INCLUDE_DIR tesseract/baseapi.h)
find_path(LEPTONICA_INCLUDE_DIR leptonica/allheaders.h)
find_library(TESSERACT_LIBRARY NAMES tesseract libtesseract)
find_library(LEPTONICA_LIBRARY NAMES lept liblept)

set(Tesseract_INCLUDE_DIRS
    ${TESSERACT_INCLUDE_DIR}
    ${LEPTONICA_INCLUDE_DIR}
)
set(Tesseract_LIBRARIES
    ${TESSERACT_LIBRARY}
    ${LEPTONICA_LIBRARY}
)

if(NOT TESSERACT_INCLUDE_DIR OR NOT LEPTONICA_INCLUDE_DIR OR NOT TESSERACT_LIBRARY OR NOT LEPTONICA_LIBRARY)
    message(FATAL_ERROR "Tesseract/Leptonica not found. Install development packages and retry.")
endif()

qt_add_executable(AtlasHub WIN32
    ${ATLASHUB_SOURCES}
    ${ATLASHUB_HEADERS}
    ${ATLASHUB_UIS}
    ${ATLASHUB_RESOURCES}
)

target_include_directories(AtlasHub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Tesseract_INCLUDE_DIRS}
)

target_link_libraries(AtlasHub PRIVATE
    Qt6::Widgets
    ${Tesseract_LIBRARIES}
)

if(MINGW)
    target_link_options(AtlasHub PRIVATE -mwindows)
endif()
