cmake_minimum_required(VERSION 3.16)

project(AtlasHub LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/atlas-ocr/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/modules/atlas-pdf/ui
)

find_package(Qt6 REQUIRED COMPONENTS Concurrent Core Gui LinguistTools Network Pdf PdfWidgets Widgets)

qt_standard_project_setup()

set(ATLASHUB_SOURCES
    main.cpp
    core/AppController.cpp
    core/AppController.h
    core/TranslationService.cpp
    core/TranslationService.h
    modules/atlas-ocr/ui/MainWindow.cpp
    modules/atlas-ocr/ui/MainWindow.h
    modules/atlas-ocr/ui/MainWindow.ui
    modules/atlas-ocr/ui/OCRService.cpp
    modules/atlas-ocr/ui/OCRService.h
    modules/atlas-ocr/ui/ScreenCaptureOverlay.cpp
    modules/atlas-ocr/ui/ScreenCaptureOverlay.h
    modules/atlas-pdf/core/AtlasPdfDocument.cpp
    modules/atlas-pdf/core/AtlasPdfDocument.h
    modules/atlas-pdf/core/AtlasPdfService.cpp
    modules/atlas-pdf/core/AtlasPdfService.h
    modules/atlas-pdf/engine/AtlasPdfImageExtractor.cpp
    modules/atlas-pdf/engine/AtlasPdfImageExtractor.h
    modules/atlas-pdf/engine/AtlasPdfTextEngine.cpp
    modules/atlas-pdf/engine/AtlasPdfTextEngine.h
    modules/atlas-pdf/ui/AtlasPdfEditor.ui
    modules/atlas-pdf/ui/AtlasPdfSelectableView.cpp
    modules/atlas-pdf/ui/AtlasPdfSelectableView.h
    utils/GlobalHotkey.cpp
    utils/GlobalHotkey.h
    resources/app.rc
    resources/resources.qrc
)

set(ATLASHUB_TRANSLATIONS
    translations/atlashub_pt_BR.ts
    translations/atlashub_en_US.ts
    translations/atlashub_fr_FR.ts
)

qt_add_executable(AtlasHub WIN32
    ${ATLASHUB_SOURCES}
)

set(ATLASPDF_SOURCES
    modules/atlas-ocr/ui/OCRService.cpp
    modules/atlas-ocr/ui/OCRService.h
    modules/atlas-pdf/core/AtlasPdfDocument.cpp
    modules/atlas-pdf/core/AtlasPdfDocument.h
    modules/atlas-pdf/core/AtlasPdfService.cpp
    modules/atlas-pdf/core/AtlasPdfService.h
    modules/atlas-pdf/engine/AtlasPdfImageExtractor.cpp
    modules/atlas-pdf/engine/AtlasPdfImageExtractor.h
    modules/atlas-pdf/engine/AtlasPdfTextEngine.cpp
    modules/atlas-pdf/engine/AtlasPdfTextEngine.h
    modules/atlas-pdf/ui/AtlasPdfEditorWindow.cpp
    modules/atlas-pdf/ui/AtlasPdfEditorWindow.h
    modules/atlas-pdf/ui/AtlasPdfEditor.ui
    modules/atlas-pdf/ui/AtlasPdfSelectableView.cpp
    modules/atlas-pdf/ui/AtlasPdfSelectableView.h
    modules/atlas-pdf/ui/main.cpp
    resources/app.rc
    resources/resources.qrc
)

qt_add_executable(AtlasPdfEditor WIN32
    ${ATLASPDF_SOURCES}
)

qt_add_translations(AtlasHub
    TS_FILES ${ATLASHUB_TRANSLATIONS}
    RESOURCE_PREFIX "/i18n"
)

target_include_directories(AtlasHub PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(AtlasPdfEditor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(AtlasHub PRIVATE
    Qt6::Concurrent
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Pdf
    Qt6::PdfWidgets
    Qt6::Widgets
    z
)

target_link_libraries(AtlasPdfEditor PRIVATE
    Qt6::Concurrent
    Qt6::Core
    Qt6::Gui
    Qt6::Pdf
    Qt6::PdfWidgets
    Qt6::Widgets
    z
)

if(WIN32)
    target_link_libraries(AtlasHub PRIVATE user32)
    target_link_libraries(AtlasPdfEditor PRIVATE user32)
endif()

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
        add_custom_command(TARGET AtlasPdfEditor POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                "$<TARGET_FILE:AtlasPdfEditor>"
        )
    endif()
endif()

set(ATLAS_OCR_TESSERACT_DIR "${CMAKE_SOURCE_DIR}/modules/atlas-ocr/third_party/tesseract")

if(NOT EXISTS "${ATLAS_OCR_TESSERACT_DIR}")
    message(FATAL_ERROR
        "Tesseract runtime not found at ${ATLAS_OCR_TESSERACT_DIR}"
    )
endif()

add_custom_command(TARGET AtlasHub POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -Dsrc="${ATLAS_OCR_TESSERACT_DIR}"
        -Ddst="$<TARGET_FILE_DIR:AtlasHub>/modules/atlas-ocr/third_party/tesseract"
        -P "${CMAKE_SOURCE_DIR}/cmake/copy_tesseract_runtime.cmake"
)

add_custom_command(TARGET AtlasPdfEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -Dsrc="${ATLAS_OCR_TESSERACT_DIR}"
        -Ddst="$<TARGET_FILE_DIR:AtlasPdfEditor>/modules/atlas-ocr/third_party/tesseract"
        -P "${CMAKE_SOURCE_DIR}/cmake/copy_tesseract_runtime.cmake"
)
