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

if(WIN32 AND TARGET Qt6::qmake)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET AtlasHub POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                "$<TARGET_FILE:AtlasHub>"
        )
    endif()
endif()

set(ATLASHUB_TESSERACT_DIR
    "${CMAKE_SOURCE_DIR}/third_party/tesseract"
    CACHE PATH "Directory containing tesseract.exe and tessdata"
)

if(NOT EXISTS "${ATLASHUB_TESSERACT_DIR}"
    AND EXISTS "${CMAKE_BINARY_DIR}/third_party/tesseract"
)
    set(ATLASHUB_TESSERACT_DIR "${CMAKE_BINARY_DIR}/third_party/tesseract")
endif()

if(EXISTS "${ATLASHUB_TESSERACT_DIR}")
    add_custom_command(TARGET AtlasHub POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -Dsrc="${ATLASHUB_TESSERACT_DIR}"
            -Ddst="$<TARGET_FILE_DIR:AtlasHub>/third_party/tesseract"
            -P "${CMAKE_SOURCE_DIR}/cmake/copy_tesseract_runtime.cmake"
    )
else()
    message(WARNING
        "Tesseract runtime not found at ${ATLASHUB_TESSERACT_DIR}; "
        "OCR will be unavailable until this folder is added."
    )
endif()
