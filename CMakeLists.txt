cmake_minimum_required(VERSION 3.10)
project(fs
    VERSION 1.0
)


# Option for forcing compilation of C files as C++. This is useful for
# development to ensure the .c files can be included straight into C++
# files and built as a single translation unit.
option(USE_CXX_FOR_C "Use C++ compiler for C files" OFF)

if(USE_CXX_FOR_C)
    set(USE_CXX_FOR_C_COMPILE_OPTIONS -x c++)
endif()


# Warning Options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(WARNING_FLAGS -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(WARNING_FLAGS /W4)
endif()


# Static Libraries
add_library(fs STATIC
    fs.c
    fs.h
)
target_compile_features(fs PRIVATE c_std_90)
target_compile_options(fs PRIVATE ${WARNING_FLAGS})
target_compile_options(fs PRIVATE ${USE_CXX_FOR_C_COMPILE_OPTIONS})

add_library(fszip STATIC
    extras/backends/zip/fs_zip.c
    extras/backends/zip/fs_zip.h
)
target_compile_features(fszip PRIVATE c_std_90)
target_compile_options(fszip PRIVATE ${WARNING_FLAGS})
target_compile_options(fszip PRIVATE ${USE_CXX_FOR_C_COMPILE_OPTIONS})


# Tests
add_executable(fstest tests/fstest.c)
target_link_libraries(fstest PRIVATE
    fs
    fszip
)
target_compile_options(fstest PRIVATE ${WARNING_FLAGS})
target_compile_options(fstest PRIVATE ${USE_CXX_FOR_C_COMPILE_OPTIONS})


# Examples

## hello_world
add_executable(hello_world examples/hello_world.c)
target_link_libraries(hello_world PRIVATE
    fs
)
target_compile_options(hello_world PRIVATE ${WARNING_FLAGS})
target_compile_options(hello_world PRIVATE ${USE_CXX_FOR_C_COMPILE_OPTIONS})

## archives
add_executable(archives examples/archives.c)
target_link_libraries(archives PRIVATE
    fs
    fszip
)
target_compile_options(archives PRIVATE ${WARNING_FLAGS})
target_compile_options(archives PRIVATE ${USE_CXX_FOR_C_COMPILE_OPTIONS})

## mounting
add_executable(mounting examples/mounting.c)
target_link_libraries(mounting PRIVATE
    fs
    fszip
)
target_compile_options(mounting PRIVATE ${WARNING_FLAGS})
target_compile_options(mounting PRIVATE ${USE_CXX_FOR_C_COMPILE_OPTIONS})


# TODO: Always link with "-static" on Windows when compiling with GCC and Clang, and /MTd or /MT for MSVC. If MSVC, have option "STATIC_CRT" like what libgit2 does.