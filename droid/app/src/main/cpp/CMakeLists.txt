cmake_minimum_required(VERSION 3.22)
project(InnerBreezeNative C)

add_library(inbe SHARED main.c)

target_compile_options(inbe PRIVATE -fno-builtin -fno-stack-protector)
target_link_options(inbe PRIVATE -nostdlib)

target_link_libraries(inbe android)
