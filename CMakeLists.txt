cmake_minimum_required(VERSION 3.20)
project(AtlasHub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(atlas_hub
  atlas/main.cpp
  atlas/core/screen_capture.cpp
  atlas/core/image_processing.cpp
  atlas/core/ocr_engine.cpp
  atlas/ui/app_window.cpp
  atlas/input/hotkey_manager.cpp
  atlas/utils/command_loop.cpp
)

target_include_directories(atlas_hub PRIVATE atlas)

if(WIN32)
  target_compile_definitions(atlas_hub PRIVATE UNICODE _UNICODE)
  target_link_libraries(atlas_hub PRIVATE msimg32)
endif()
