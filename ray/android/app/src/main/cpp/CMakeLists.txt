cmake_minimum_required(VERSION 3.22)
project(RayInbe C)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../../..")
set(RAY_DIR "${REPO_ROOT}/ray")
set(RAYLIB_DIR "${REPO_ROOT}/vendor/raylib/src")
set(NATIVE_APP_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")

add_library(android_native_app_glue STATIC
    "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
)

target_include_directories(android_native_app_glue PUBLIC
    "${NATIVE_APP_GLUE_DIR}"
)

add_library(raylib STATIC
    "${RAYLIB_DIR}/rcore.c"
    "${RAYLIB_DIR}/rshapes.c"
    "${RAYLIB_DIR}/rtextures.c"
    "${RAYLIB_DIR}/rtext.c"
)

target_compile_definitions(raylib PRIVATE
    PLATFORM_ANDROID
    GRAPHICS_API_OPENGL_ES2
    SUPPORT_SCREEN_CAPTURE=0
    SUPPORT_COMPRESSION_API=0
    SUPPORT_AUTOMATION_EVENTS=0
    SUPPORT_CLIPBOARD_IMAGE=0
    SUPPORT_FILEFORMAT_PNG=0
    SUPPORT_FILEFORMAT_BMP=0
    SUPPORT_FILEFORMAT_GIF=0
    SUPPORT_FILEFORMAT_QOI=0
    SUPPORT_FILEFORMAT_DDS=0
    SUPPORT_FILEFORMAT_TTF=0
)

target_compile_options(raylib PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
)

target_include_directories(raylib PUBLIC
    "${RAYLIB_DIR}"
    "${NATIVE_APP_GLUE_DIR}"
)

add_library(main SHARED
    "${RAY_DIR}/main.c"
)

target_compile_options(main PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
)

target_link_options(main PRIVATE
    -Wl,--gc-sections
    -Wl,--wrap=fopen
)

target_link_libraries(main
    raylib
    android_native_app_glue
    android
    log
    EGL
    GLESv2
    m
)
