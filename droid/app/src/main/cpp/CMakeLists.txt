cmake_minimum_required(VERSION 3.22)
project(Inbe C)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM AND EXISTS "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
else()
    set(CMAKE_C_COMPILER_LAUNCHER "")
    set(CMAKE_C_COMPILER_LAUNCHER "" CACHE STRING "" FORCE)
endif()

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
set(SRC_DIR "${REPO_ROOT}/src")
set(KRYON_DIR "${REPO_ROOT}/vendor/kryon")
set(RAYLIB_DIR "${KRYON_DIR}/vendor/raylib/src")
set(SQLITE_DIR "${REPO_ROOT}/vendor/sqlite")
set(SQLITE_BUILD_DIR "${CMAKE_BINARY_DIR}/sqlite")
set(SQLITE_AMALGAMATION_C "${SQLITE_BUILD_DIR}/sqlite3.c")
set(SQLITE_AMALGAMATION_H "${SQLITE_BUILD_DIR}/sqlite3.h")
set(NATIVE_APP_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")
set(OQS_BUILD_ONLY_LIB ON CACHE BOOL "" FORCE)
set(OQS_USE_OPENSSL OFF CACHE BOOL "" FORCE)
set(OQS_MINIMAL_BUILD "SIG_ml_dsa_44" CACHE STRING "" FORCE)
set(OQS_DIST_BUILD OFF CACHE BOOL "" FORCE)
set(OQS_OPT_TARGET generic CACHE STRING "" FORCE)
foreach(oqs_x86_feature
        ADX AES AVX AVX2 AVX512 AVX512BW AVX512DQ AVX512F
        BMI1 BMI2 FMA PCLMULQDQ POPCNT SSE SSE2 SSE3 VPCLMULQDQ)
    set(OQS_USE_${oqs_x86_feature}_INSTRUCTIONS OFF CACHE BOOL "" FORCE)
endforeach()
add_subdirectory("${KRYON_DIR}/vendor/liboqs" "${CMAKE_BINARY_DIR}/liboqs")
if(NOT TARGET oqs)
    message(FATAL_ERROR "Inbe Android builds require liboqs for sync account crypto")
endif()
file(GLOB_RECURSE KRYON_SOURCES CONFIGURE_DEPENDS "${KRYON_DIR}/src/*.c")
list(REMOVE_ITEM KRYON_SOURCES "${KRYON_DIR}/src/file_dialog/file_dialog.c")
find_program(HOST_MAKE NAMES gmake make REQUIRED)
set(KRYON_KC "${KRYON_DIR}/build/bin/kc")
set(INBE_KRY_GEN_DIR "${CMAKE_BINARY_DIR}/kryon/generated")
file(GLOB_RECURSE INBE_KRY_SOURCES CONFIGURE_DEPENDS "${SRC_DIR}/*.kry")
list(SORT INBE_KRY_SOURCES)
set(INBE_KRY_GENERATED_SOURCES)
set(INBE_KRY_GENERATED_HEADERS)
foreach(kry_source ${INBE_KRY_SOURCES})
    file(RELATIVE_PATH kry_rel "${REPO_ROOT}" "${kry_source}")
    get_filename_component(kry_rel_dir "${kry_rel}" DIRECTORY)
    get_filename_component(kry_name "${kry_rel}" NAME_WE)
    set(kry_output_c "${INBE_KRY_GEN_DIR}/${kry_rel_dir}/${kry_name}.c")
    set(kry_output_h "${INBE_KRY_GEN_DIR}/${kry_rel_dir}/${kry_name}.h")
    if(kry_rel STREQUAL "src/storage/db.kry")
        set(kry_output_c "${INBE_KRY_GEN_DIR}/src/storage/db_impl.c")
        set(kry_output_h "${INBE_KRY_GEN_DIR}/src/storage/db_impl.h")
    endif()
    if(kry_rel STREQUAL "src/storage/import.kry")
        set(kry_output_c "${INBE_KRY_GEN_DIR}/src/storage/import_impl.c")
        set(kry_output_h "${INBE_KRY_GEN_DIR}/src/storage/import_impl.h")
    endif()
    list(APPEND INBE_KRY_GENERATED_SOURCES "${kry_output_c}")
    list(APPEND INBE_KRY_GENERATED_HEADERS "${kry_output_h}")
endforeach()
set(INBE_KRY_PROJECT_HEADER "${INBE_KRY_GEN_DIR}/kryon_project.h")
add_custom_command(
    OUTPUT ${INBE_KRY_GENERATED_SOURCES} ${INBE_KRY_GENERATED_HEADERS} "${INBE_KRY_PROJECT_HEADER}"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${INBE_KRY_GEN_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${INBE_KRY_GEN_DIR}"
    COMMAND "${KRYON_KC}" --root "${REPO_ROOT}" -o "${INBE_KRY_GEN_DIR}" ${INBE_KRY_SOURCES}
    DEPENDS ${INBE_KRY_SOURCES} "${KRYON_KC}"
    WORKING_DIRECTORY "${REPO_ROOT}"
    VERBATIM
)
add_custom_command(
    OUTPUT "${KRYON_KC}"
    COMMAND "${HOST_MAKE}" -C "${KRYON_DIR}" build/bin/kc
    DEPENDS "${KRYON_DIR}/Makefile"
    VERBATIM
)
add_custom_target(inbe_kry_generated DEPENDS
    ${INBE_KRY_GENERATED_SOURCES}
    ${INBE_KRY_GENERATED_HEADERS}
    "${INBE_KRY_PROJECT_HEADER}"
)
file(GLOB INBE_LOCALE_ASSETS RELATIVE "${REPO_ROOT}" CONFIGURE_DEPENDS "${REPO_ROOT}/locales/*.txt")
file(GLOB INBE_SOUND_ASSETS RELATIVE "${REPO_ROOT}" CONFIGURE_DEPENDS "${REPO_ROOT}/assets/sounds/*.ogg")
file(GLOB INBE_PRACTICE_BANNER_ASSETS RELATIVE "${REPO_ROOT}" CONFIGURE_DEPENDS "${REPO_ROOT}/assets/practices/*/banner.png")
set(INBE_EMBEDDED_INPUTS
    ${INBE_LOCALE_ASSETS}
    assets/easteregg/art.png
    assets/easteregg/waozi.png
    assets/practices/whm/1.png
    assets/practices/whm/2.png
    assets/practices/meditation/1.png
    assets/pet/egg1.png
    ${INBE_PRACTICE_BANNER_ASSETS}
    assets/practices/sunsalutation/poses_man_sheet.png
    assets/practices/sunsalutation/poses_woman_sheet.png
    assets/fonts/subset/NotoSans-Inbe-Regular.ttf
    assets/fonts/subset/NotoSansSC-Inbe-Regular.otf
    assets/fonts/subset/NotoSansJP-Inbe-Regular.otf
    assets/fonts/subset/NotoSansKR-Inbe-Regular.otf
    assets/fonts/subset/NotoSansTC-Inbe-Regular.otf
    ${INBE_SOUND_ASSETS}
)
set(INBE_EMBEDDED_DEPS)
foreach(asset ${INBE_EMBEDDED_INPUTS})
    list(APPEND INBE_EMBEDDED_DEPS "${REPO_ROOT}/${asset}")
endforeach()
set(INBE_EMBEDDED_ASSETS_C "${CMAKE_BINARY_DIR}/inbe_embedded_assets.c")

add_custom_command(
    OUTPUT "${INBE_EMBEDDED_ASSETS_C}"
    COMMAND sh "${KRYON_DIR}/scripts/embed-assets.sh" "${INBE_EMBEDDED_ASSETS_C}" ${INBE_EMBEDDED_INPUTS}
    DEPENDS "${KRYON_DIR}/scripts/embed-assets.sh" ${INBE_EMBEDDED_DEPS}
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
    SUPPORT_FILEFORMAT_TTF=1
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
    "${SRC_DIR}/app/app.c"
    "${SRC_DIR}/storage/storage.c"
    "${SRC_DIR}/storage/sync_client.c"
    "${SRC_DIR}/third_party/miniz.c"
    "${SQLITE_AMALGAMATION_C}"
    "${SRC_DIR}/platform/android/android_device.c"
    "${SRC_DIR}/platform/android/android_share.c"
    "${INBE_EMBEDDED_ASSETS_C}"
    ${INBE_KRY_GENERATED_SOURCES}
    ${KRYON_SOURCES}
    "${SRC_DIR}/platform/android/android_insets.c"
    "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
)
add_dependencies(main inbe_kry_generated)

target_include_directories(main PRIVATE
    "${KRYON_DIR}/include"
    "${KRYON_DIR}/src"
    "${SRC_DIR}"
    "${SRC_DIR}/app"
    "${SRC_DIR}/core"
    "${SRC_DIR}/screens"
    "${SRC_DIR}/screens/settings"
    "${SRC_DIR}/practices"
    "${SRC_DIR}/practices/whm"
    "${SRC_DIR}/practices/meditation"
    "${SRC_DIR}/practices/sun_salutation"
    "${SRC_DIR}/storage"
    "${SRC_DIR}/platform/android"
    "${SRC_DIR}/third_party"
    "${INBE_KRY_GEN_DIR}"
    "${INBE_KRY_GEN_DIR}/src"
    "${NATIVE_APP_GLUE_DIR}"
    "${SQLITE_BUILD_DIR}"
)

target_compile_definitions(main PRIVATE
    PLATFORM_ANDROID
    ANDROID_BUILD=1
    _DEFAULT_SOURCE
    _GNU_SOURCE
    _FILE_OFFSET_BITS=64
    MG_TLS=MG_TLS_BUILTIN
    MG_ENABLE_EPOLL=0
    MG_ENABLE_POLL=1
    MG_ENABLE_POSIX_FS=0
    SUPPORT_MODULE_RAUDIO=1
    SUPPORT_FILEFORMAT_OGG=1
    UI_EMBEDDED_ONLY=1
    HAS_LIBOQS=1
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
    -Wl,-z,max-page-size=16384
    -Wl,-z,common-page-size=16384
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
