cmake_minimum_required(VERSION 3.22)
project(InnerBreezeNative C)

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
set(LOTUS_DIR "${REPO_ROOT}/liblotus")

add_library(lotus STATIC
	"${LOTUS_DIR}/lotus.c"
)

target_include_directories(lotus PUBLIC
	"${LOTUS_DIR}"
)

add_library(inbe SHARED
    main.c
)

target_compile_options(inbe PRIVATE -fno-builtin -fno-stack-protector)
target_link_options(inbe PRIVATE -nostdlib)

target_link_libraries(inbe
    lotus
    android
    atomic
    m
    c
)
