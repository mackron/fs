cmake_minimum_required(VERSION 3.10)
project(fs
    VERSION 1.0
)

# Options
option(FS_BUILD_EXAMPLES             "Build typo examples"          OFF)
option(FS_BUILD_TESTS                "Build typo tests"             OFF)
option(FS_BUILD_TOOLS                "Build typo tools"             OFF)
option(FS_FORCE_CXX                  "Force compilation as C++"     OFF)
option(FS_FORCE_C89                  "Force compilation as C89"     OFF)
option(FS_ENABLE_OPENED_FILES_ASSERT "Enable an assert for when a file is left open" OFF)


# Construct compiler options.
set(COMPILE_OPTIONS)

if(FS_FORCE_CXX AND FS_FORCE_C89)
    message(FATAL_ERROR "FS_FORCE_CXX and FS_FORCE_C89 cannot be enabled at the same time.")
endif()

if(FS_FORCE_CXX)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Compiling as C++ (GNU/Clang)")
        list(APPEND COMPILE_OPTIONS -x c++)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(STATUS "Compiling as C++ (MSVC)")
        list(APPEND COMPILE_OPTIONS /TP)
    else()
        message(WARNING "FS_FORCE_CXX is enabled but the compiler does not support it. Ignoring.")
    endif()
endif()

if(FS_FORCE_C89)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(STATUS "Compiling as C89")
        list(APPEND COMPILE_OPTIONS -std=c89)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(WARNING "MSVC does not support forcing C89. FS_FORCE_C89 ignored.")
    else()
        message(WARNING "FS_FORCE_C89 is enabled but the compiler does not support it. Ignoring.")
    endif()
endif()

# Warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND COMPILE_OPTIONS -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    #list(APPEND COMPILE_OPTIONS /W4)
endif()



# Static Libraries
add_library(fs STATIC
    fs.c
    fs.h
)

target_include_directories(fs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

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

add_library(fspak STATIC
    extras/backends/pak/fs_pak.c
    extras/backends/pak/fs_pak.h
)
target_compile_features(fspak PRIVATE c_std_90)
target_compile_options(fspak PRIVATE ${WARNING_FLAGS})
target_compile_options(fspak PRIVATE ${USE_CXX_FOR_C_COMPILE_OPTIONS})

add_library(fssubfs STATIC
    extras/backends/subfs/fs_subfs.c
    extras/backends/subfs/fs_subfs.h
)
target_compile_features(fssubfs PRIVATE c_std_90)
target_compile_options(fssubfs PRIVATE ${WARNING_FLAGS})
target_compile_options(fssubfs PRIVATE ${USE_CXX_FOR_C_COMPILE_OPTIONS})


# Tests
if(FS_BUILD_TESTS)
    enable_testing()

    add_executable(fstest tests/fstest.c)
    target_link_libraries(fstest PRIVATE
        fs
        fszip
        fspak
        fssubfs
    )
    target_compile_options(fstest PRIVATE ${WARNING_FLAGS})
    target_compile_options(fstest PRIVATE ${USE_CXX_FOR_C_COMPILE_OPTIONS})

    add_test(NAME fstest COMMAND fstest)
endif()


# Examples
if(FS_BUILD_EXAMPLES)
    # hello_world
    add_executable(hello_world examples/hello_world.c)
    target_link_libraries(hello_world PRIVATE
        fs
    )
    target_compile_options(hello_world PRIVATE ${WARNING_FLAGS})
    target_compile_options(hello_world PRIVATE ${USE_CXX_FOR_C_COMPILE_OPTIONS})

    # archives
    add_executable(archives examples/archives.c)
    target_link_libraries(archives PRIVATE
        fs
        fszip
        fspak
    )
    target_compile_options(archives PRIVATE ${WARNING_FLAGS})
    target_compile_options(archives PRIVATE ${USE_CXX_FOR_C_COMPILE_OPTIONS})

    # mounting
    add_executable(mounting examples/mounting.c)
    target_link_libraries(mounting PRIVATE
        fs
        fszip
        fspak
    )
    target_compile_options(mounting PRIVATE ${WARNING_FLAGS})
    target_compile_options(mounting PRIVATE ${USE_CXX_FOR_C_COMPILE_OPTIONS})
endif()


# TODO: Always link with "-static" on Windows when compiling with GCC and Clang, and /MTd or /MT for MSVC. If MSVC, have option "STATIC_CRT" like what libgit2 does.
