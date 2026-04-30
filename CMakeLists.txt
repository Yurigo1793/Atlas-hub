cmake_minimum_required(VERSION 3.16)

project(AtlasHub VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

set(ATLASHUB_SOURCES
    main.cpp
    core/AppController.cpp
    core/AppController.h
    core/OCRService.cpp
    core/OCRService.h
    ui/MainWindow.cpp
    ui/MainWindow.h
    ui/MainWindow.ui
    ui/ScreenCaptureOverlay.cpp
    ui/ScreenCaptureOverlay.h
)

qt_add_executable(AtlasHub
    ${ATLASHUB_SOURCES}
)

target_include_directories(AtlasHub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(AtlasHub PRIVATE
    Qt6::Widgets
)
