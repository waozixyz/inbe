cmake_minimum_required(VERSION 3.22)
project(Inbe C)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
set(SRC_DIR "${REPO_ROOT}/src")
set(LIBINBE_DIR "${REPO_ROOT}/libinbe")
set(RAYLIB_DIR "${REPO_ROOT}/vendor/raylib/src")
set(FLINT_DIR "${REPO_ROOT}/vendor/flint")
set(NATIVE_APP_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")
file(GLOB FLINT_SOURCES CONFIGURE_DEPENDS "${FLINT_DIR}/src/*.c")
list(REMOVE_ITEM FLINT_SOURCES "${FLINT_DIR}/src/flint_file_dialog.c")

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

add_library(flint STATIC ${FLINT_SOURCES})

target_include_directories(flint PUBLIC
    "${FLINT_DIR}/include"
    "${RAYLIB_DIR}"
)

add_library(main SHARED
    "${SRC_DIR}/main.c"
    "${SRC_DIR}/app.c"
    "${SRC_DIR}/app_session.c"
    "${SRC_DIR}/locale.c"
    "${SRC_DIR}/theme_meta.c"
    "${SRC_DIR}/theme.c"
    "${SRC_DIR}/data.c"
    "${SRC_DIR}/miniz.c"
    "${SRC_DIR}/android/android_import.c"
    "${SRC_DIR}/android/android_share.c"
    "${SRC_DIR}/tabs/history_tab.c"
    "${SRC_DIR}/tabs/language_tab.c"
    "${SRC_DIR}/tabs/manual_tab.c"
    "${SRC_DIR}/tabs/settings_tab.c"
    "${SRC_DIR}/android/android_insets.c"
    "${SRC_DIR}/android/android_wakelock.c"
    "${SRC_DIR}/android/android_timer.c"
    "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
)

target_include_directories(main PRIVATE
    "${SRC_DIR}"
    "${SRC_DIR}/android"
    "${NATIVE_APP_GLUE_DIR}"
    "${LIBINBE_DIR}"
    "${FLINT_DIR}/include"
)

target_compile_definitions(main PRIVATE
    PLATFORM_ANDROID
    _DEFAULT_SOURCE
    _GNU_SOURCE
    _FILE_OFFSET_BITS=64
    MG_TLS=MG_TLS_BUILTIN
    MG_ENABLE_EPOLL=0
    MG_ENABLE_POLL=1
    MG_ENABLE_POSIX_FS=0
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
    flint
    android
    log
    OpenSLES
    EGL
    GLESv2
    m
)
