cmake_minimum_required(VERSION 3.20)
project(BambuBotStudio VERSION 0.2.0 LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BAMBU_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
set(BAMBU_LVGL_DIR "" CACHE PATH "Optional LVGL 9.5.0 source directory")
if(NOT BAMBU_LVGL_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/lvgl__lvgl/lvgl.h")
        set(BAMBU_LVGL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components/lvgl__lvgl")
    else()
        include(FetchContent)
        FetchContent_Declare(lvgl_source
            GIT_REPOSITORY https://github.com/lvgl/lvgl.git
            GIT_TAG v9.5.0
            GIT_SHALLOW TRUE
            SOURCE_SUBDIR source-only)
        FetchContent_MakeAvailable(lvgl_source)
        set(BAMBU_LVGL_DIR "${lvgl_source_SOURCE_DIR}")
    endif()
endif()

find_package(SDL2 CONFIG REQUIRED HINTS /opt/homebrew/lib/cmake/SDL2 /usr/local/lib/cmake/SDL2)

# Only core sources, no demos or platform drivers. Ignore AppleDouble metadata on external drives.
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS
    "${BAMBU_LVGL_DIR}/src/*.c" "${BAMBU_LVGL_DIR}/src/*.cpp" "${BAMBU_LVGL_DIR}/src/*.S")
list(FILTER LVGL_SOURCES EXCLUDE REGEX "/[.]_")
add_library(bambu_lvgl STATIC ${LVGL_SOURCES})
target_include_directories(bambu_lvgl PUBLIC "${BAMBU_LVGL_DIR}")
target_compile_definitions(bambu_lvgl PUBLIC
    LV_CONF_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h"
    LV_KCONFIG_IGNORE LV_LVGL_H_INCLUDE_SIMPLE)
target_compile_options(bambu_lvgl PRIVATE -Wno-deprecated-declarations)

add_library(bambu_face STATIC ../ui/bambu_face.cc)
target_include_directories(bambu_face PUBLIC ../ui)
target_link_libraries(bambu_face PUBLIC bambu_lvgl)
target_compile_options(bambu_face PRIVATE -Wall -Wextra -Werror)

add_executable(bambu_studio MACOSX_BUNDLE main.cc)
target_link_libraries(bambu_studio PRIVATE bambu_face SDL2::SDL2)
target_compile_options(bambu_studio PRIVATE -Wall -Wextra -Werror)
set_target_properties(bambu_studio PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "Bambu Bot Studio"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.hdchipeo.bambubot.studio"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.2.0"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.2.0")

if(BAMBU_SANITIZERS)
    foreach(target bambu_lvgl bambu_face bambu_studio)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    endforeach()
    target_link_options(bambu_studio PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()
add_test(NAME emotion-studio-smoke COMMAND bambu_studio --self-test)
set_tests_properties(emotion-studio-smoke PROPERTIES TIMEOUT 60)
