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

set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/appicon.rc")

set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/appicon.rc")

qt_add_executable(AtlasHub WIN32
    ${ATLASHUB_SOURCES}
    ${ATLASHUB_HEADERS}
    ${ATLASHUB_UIS}
    ${ATLASHUB_RESOURCES}
    ${APP_ICON_RESOURCE_WINDOWS}
)

target_include_directories(AtlasHub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(TESSERACT_DIR ${CMAKE_SOURCE_DIR}/third_party/tesseract)

if(NOT IS_DIRECTORY ${TESSERACT_DIR})
    message(FATAL_ERROR "Tesseract not found in third_party/tesseract")
endif()

if(NOT IS_DIRECTORY ${TESSERACT_DIR}/include OR NOT IS_DIRECTORY ${TESSERACT_DIR}/lib OR NOT IS_DIRECTORY ${TESSERACT_DIR}/bin OR NOT IS_DIRECTORY ${TESSERACT_DIR}/tessdata)
    message(FATAL_ERROR "Tesseract not found in third_party/tesseract")
endif()

target_include_directories(AtlasHub PRIVATE
    ${TESSERACT_DIR}/include
)

link_directories(${TESSERACT_DIR}/lib)

add_custom_command(TARGET AtlasHub POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${TESSERACT_DIR}/bin
    $<TARGET_FILE_DIR:AtlasHub>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${TESSERACT_DIR}/tessdata
    $<TARGET_FILE_DIR:AtlasHub>/tessdata
)

target_link_libraries(AtlasHub PRIVATE
    Qt6::Widgets
    tesseract
    leptonica
)

if(MINGW)
    target_link_options(AtlasHub PRIVATE -mwindows)
endif()
