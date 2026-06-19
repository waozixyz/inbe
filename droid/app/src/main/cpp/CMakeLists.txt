cmake_minimum_required(VERSION 3.22)
project(Inbe C)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
set(SRC_DIR "${REPO_ROOT}/src")
set(RAYLIB_DIR "${REPO_ROOT}/vendor/raylib/src")
set(FLINT_DIR "${REPO_ROOT}/flint")
set(SQLITE_DIR "${REPO_ROOT}/vendor/sqlite")
set(SQLITE_BUILD_DIR "${CMAKE_BINARY_DIR}/sqlite")
set(SQLITE_AMALGAMATION_C "${SQLITE_BUILD_DIR}/sqlite3.c")
set(SQLITE_AMALGAMATION_H "${SQLITE_BUILD_DIR}/sqlite3.h")
set(NATIVE_APP_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")
set(OQS_BUILD_ONLY_LIB ON CACHE BOOL "" FORCE)
set(OQS_USE_OPENSSL OFF CACHE BOOL "" FORCE)
set(OQS_MINIMAL_BUILD "SIG_ml_dsa_44" CACHE STRING "" FORCE)
add_subdirectory("${REPO_ROOT}/vendor/liboqs" "${CMAKE_BINARY_DIR}/liboqs")
file(GLOB FLINT_SOURCES CONFIGURE_DEPENDS "${FLINT_DIR}/src/*.c")
list(REMOVE_ITEM FLINT_SOURCES "${FLINT_DIR}/src/flint_file_dialog.c")
file(GLOB INBE_LOCALE_ASSETS RELATIVE "${REPO_ROOT}" CONFIGURE_DEPENDS "${REPO_ROOT}/locales/*.txt")
file(GLOB INBE_SOUND_ASSETS RELATIVE "${REPO_ROOT}" CONFIGURE_DEPENDS "${REPO_ROOT}/assets/sounds/*.ogg")
set(INBE_EMBEDDED_INPUTS
    ${INBE_LOCALE_ASSETS}
    assets/practices/whm/1.jpg
    assets/practices/whm/2.jpg
    assets/practices/meditation/1.jpg
    assets/fonts/locales.png
    assets/fonts/locales.dat
    assets/fonts/locales-8.png
    assets/fonts/locales-8.dat
    ${INBE_SOUND_ASSETS}
)
set(INBE_EMBEDDED_DEPS)
foreach(asset ${INBE_EMBEDDED_INPUTS})
    list(APPEND INBE_EMBEDDED_DEPS "${REPO_ROOT}/${asset}")
endforeach()
set(INBE_EMBEDDED_ASSETS_C "${CMAKE_BINARY_DIR}/inbe_embedded_assets.c")

add_custom_command(
    OUTPUT "${INBE_EMBEDDED_ASSETS_C}"
    COMMAND sh "${FLINT_DIR}/scripts/embed-assets.sh" "${INBE_EMBEDDED_ASSETS_C}" ${INBE_EMBEDDED_INPUTS}
    DEPENDS "${FLINT_DIR}/scripts/embed-assets.sh" ${INBE_EMBEDDED_DEPS}
    WORKING_DIRECTORY "${REPO_ROOT}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SQLITE_AMALGAMATION_C}" "${SQLITE_AMALGAMATION_H}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SQLITE_BUILD_DIR}"
    COMMAND sh -c "cd '${SQLITE_BUILD_DIR}' && '${SQLITE_DIR}/configure'"
    COMMAND make -C "${SQLITE_BUILD_DIR}" sqlite3.c sqlite3.h
    DEPENDS "${SQLITE_DIR}/configure" "${SQLITE_DIR}/manifest"
    VERBATIM
)

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

add_library(main SHARED
    "${SRC_DIR}/main.c"
    "${SRC_DIR}/core/breath_engine.c"
    "${SRC_DIR}/app/app.c"
    "${SRC_DIR}/app/app_settings.c"
    "${SRC_DIR}/app/device_preferences.c"
    "${SRC_DIR}/practices/practice_registry.c"
    "${SRC_DIR}/practices/whm/whm_practice.c"
    "${SRC_DIR}/practices/whm/whm_session.c"
    "${SRC_DIR}/practices/whm/whm_manual.c"
    "${SRC_DIR}/practices/whm/whm_config.c"
    "${SRC_DIR}/practices/meditation/meditation_practice.c"
    "${SRC_DIR}/practices/meditation/meditation_session.c"
    "${SRC_DIR}/practices/meditation/meditation_manual.c"
    "${SRC_DIR}/practices/meditation/meditation_config.c"
    "${SRC_DIR}/screens/habits_screen.c"
    "${SRC_DIR}/practices/meditation/meditation_music.c"
    "${SRC_DIR}/core/locale.c"
    "${SRC_DIR}/core/theme.c"
    "${SRC_DIR}/storage/data.c"
    "${SRC_DIR}/storage/storage.c"
    "${SRC_DIR}/storage/sync_account.c"
    "${SRC_DIR}/storage/sync_client.c"
    "${SRC_DIR}/third_party/miniz.c"
    "${SQLITE_AMALGAMATION_C}"
    "${SRC_DIR}/platform/android/android_device.c"
    "${SRC_DIR}/platform/android/android_import.c"
    "${SRC_DIR}/platform/android/android_runtime_assets.c"
    "${SRC_DIR}/platform/android/android_share.c"
    "${SRC_DIR}/screens/practice_screen.c"
    "${SRC_DIR}/screens/language_screen.c"
    "${SRC_DIR}/screens/manual_screen.c"
    "${SRC_DIR}/screens/settings/settings_screen.c"
    "${SRC_DIR}/screens/settings/settings_device.c"
    "${SRC_DIR}/screens/settings/settings_theme.c"
    "${SRC_DIR}/screens/settings/settings_data.c"
    "${SRC_DIR}/screens/settings/settings_sync_account.c"
    "${INBE_EMBEDDED_ASSETS_C}"
    ${FLINT_SOURCES}
    "${SRC_DIR}/platform/android/android_insets.c"
    "${SRC_DIR}/platform/android/android_wakelock.c"
    "${SRC_DIR}/platform/android/android_timer.c"
    "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
)

target_include_directories(main PRIVATE
    "${SRC_DIR}"
    "${SRC_DIR}/app"
    "${SRC_DIR}/core"
    "${SRC_DIR}/screens"
    "${SRC_DIR}/screens/settings"
    "${SRC_DIR}/practices"
    "${SRC_DIR}/practices/whm"
    "${SRC_DIR}/practices/meditation"
    "${SRC_DIR}/storage"
    "${SRC_DIR}/platform/android"
    "${SRC_DIR}/third_party"
    "${NATIVE_APP_GLUE_DIR}"
    "${FLINT_DIR}/include"
    "${SQLITE_BUILD_DIR}"
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
    FLINT_EMBEDDED_ONLY=1
    INBE_HAS_LIBOQS=1
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
    oqs
    android
    log
    OpenSLES
    EGL
    GLESv2
    m
)
