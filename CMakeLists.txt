cmake_minimum_required(VERSION 3.25)
project(aotr60 LANGUAGES CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "AotR60 patches a 32-bit game and must be built for Win32 (x86).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT: the proxy must not depend on VCRUNTIME/MSVCP DLLs next to the game.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(AOTR60_COMMON_OPTIONS /W4 /permissive- /arch:SSE2 /EHsc /utf-8)

# Code shared by the DLL and the tests (no Windows loader assumptions).
add_library(aotr60_core STATIC
    src/build_id.cpp
    src/config.cpp
    src/crc32.cpp
)
target_include_directories(aotr60_core PUBLIC src)
target_compile_options(aotr60_core PRIVATE ${AOTR60_COMMON_OPTIONS})

# The injected proxy: rotwk\dinput8.dll
add_library(aotr60 SHARED
    src/dllmain.cpp
    src/log.cpp
    src/paths.cpp
    src/proxy.cpp
    src/dinput8.def
)
target_link_libraries(aotr60 PRIVATE aotr60_core)
target_compile_options(aotr60 PRIVATE ${AOTR60_COMMON_OPTIONS})
set_target_properties(aotr60 PROPERTIES OUTPUT_NAME dinput8)

enable_testing()
add_executable(aotr60_tests tests/test_main.cpp tests/test_core.cpp)
target_link_libraries(aotr60_tests PRIVATE aotr60_core)
target_compile_options(aotr60_tests PRIVATE ${AOTR60_COMMON_OPTIONS})
target_compile_definitions(aotr60_tests PRIVATE AOTR60_GAME_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
add_test(NAME aotr60_tests COMMAND aotr60_tests)
