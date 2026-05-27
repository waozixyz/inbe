cmake_minimum_required(VERSION 3.22)
project(Inbe C)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
set(SRC_DIR "${REPO_ROOT}/src")
set(LIBINBE_DIR "${REPO_ROOT}/libinbe")
set(RAYLIB_DIR "${REPO_ROOT}/vendor/raylib/src")
set(NATIVE_APP_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")

add_library(raylib STATIC
    "${RAYLIB_DIR}/rcore.c"
    "${RAYLIB_DIR}/rshapes.c"
    "${RAYLIB_DIR}/rtextures.c"
    "${RAYLIB_DIR}/rtext.c"
    "${RAYLIB_DIR}/raudio.c"
)

target_compile_definitions(raylib PRIVATE
    PLATFORM_ANDROID
    GRAPHICS_API_OPENGL_ES2
    SUPPORT_SCREEN_CAPTURE=0
    SUPPORT_COMPRESSION_API=0
    SUPPORT_AUTOMATION_EVENTS=0
    SUPPORT_CLIPBOARD_IMAGE=0
    SUPPORT_FILEFORMAT_BMP=0
    SUPPORT_FILEFORMAT_GIF=0
    SUPPORT_FILEFORMAT_QOI=0
    SUPPORT_FILEFORMAT_DDS=0
    SUPPORT_FILEFORMAT_TTF=0
    SUPPORT_FILEFORMAT_JPG=1
    SUPPORT_FILEFORMAT_PNG=1
    SUPPORT_FILEFORMAT_OGG=1
    SUPPORT_MODULE_RAUDIO=1
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

add_library(inbe STATIC
    "${LIBINBE_DIR}/inbe.c"
)

target_include_directories(inbe PUBLIC
    "${LIBINBE_DIR}"
)

add_library(main SHARED
    "${SRC_DIR}/main.c"
    "${SRC_DIR}/app.c"
    "${SRC_DIR}/ui.c"
    "${SRC_DIR}/theme_meta.c"
    "${SRC_DIR}/theme.c"
    "${SRC_DIR}/android_insets.c"
    "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
)

target_include_directories(main PRIVATE
    "${SRC_DIR}"
    "${NATIVE_APP_GLUE_DIR}"
    "${LIBINBE_DIR}"
)

target_compile_definitions(main PRIVATE
    PLATFORM_ANDROID
    SUPPORT_MODULE_RAUDIO=1
    SUPPORT_FILEFORMAT_OGG=1
)

target_compile_options(main PRIVATE
    -Os
    -ffunction-sections
    -fdata-sections
    -fvisibility=hidden
)

target_link_options(main PRIVATE
    -Wl,--gc-sections
    -Wl,--strip-all
    -Wl,--wrap=fopen
)

target_link_libraries(main
    raylib
    inbe
    android
    log
    OpenSLES
    EGL
    GLESv2
    m
)
