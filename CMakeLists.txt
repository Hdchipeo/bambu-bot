# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)

option(BAMBU_ANIMATION_PREVIEW "Cycle all scenes and head emotions with demo telemetry" OFF)
option(BAMBU_PRINTER_PREVIEW "Show printer check while assistant is idle; retain alerts" OFF)
option(BAMBU_COFFEE_PREVIEW "Show coffee while assistant is idle; retain printer alerts" OFF)
option(BAMBU_FORCE_DEMO "Build without local printer credentials" OFF)
set(PROJECT_VER "0.5.3")
project(bambu_bot)
