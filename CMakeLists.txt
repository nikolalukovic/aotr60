cmake_minimum_required(VERSION 3.25)
project(aotr60 LANGUAGES CXX ASM_MASM)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "AotR60 patches a 32-bit game and must be built for Win32 (x86).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT: the proxy must not depend on VCRUNTIME/MSVCP DLLs next to the game.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# x86 images are linked with /SAFESEH; the stub object must declare that it has no handlers.
set(CMAKE_ASM_MASM_FLAGS "${CMAKE_ASM_MASM_FLAGS} /safeseh")

set(AOTR60_COMMON_OPTIONS /W4 /permissive- /arch:SSE2 /EHsc /utf-8)

# Pure logic shared by the DLL and the tests (no game memory access).
add_library(aotr60_core STATIC
    src/build_id.cpp
    src/camera_math.cpp
    src/config.cpp
    src/crc32.cpp
    src/frame_state.cpp
    src/patcher.cpp
)
target_include_directories(aotr60_core PUBLIC src)
target_compile_options(aotr60_core PRIVATE ${AOTR60_COMMON_OPTIONS})

# Runtime: stubs and the C++ code they call. Linked into the tests too (never executed there) so the symbol
# table and the site installer can be checked.
add_library(aotr60_runtime STATIC
    src/c5_physics.cpp
    src/camera.cpp
    src/frame_ctl.cpp
    src/installer.cpp
    src/log.cpp
    src/pacer.cpp
    src/paths.cpp
    src/runtime.cpp
    src/symbols.cpp
    src/telemetry.cpp
    src/stubs/stubs.asm
)
target_include_directories(aotr60_runtime PUBLIC src)
target_link_libraries(aotr60_runtime PUBLIC aotr60_core winmm)
target_compile_options(aotr60_runtime PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${AOTR60_COMMON_OPTIONS}>)
set_source_files_properties(src/stubs/stubs.asm PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src/stubs")

# The injected proxy: rotwk\dinput8.dll
add_library(aotr60 SHARED
    src/dllmain.cpp
    src/proxy.cpp
    src/dinput8.def
)
target_link_libraries(aotr60 PRIVATE aotr60_runtime)
target_compile_options(aotr60 PRIVATE ${AOTR60_COMMON_OPTIONS})
set_target_properties(aotr60 PROPERTIES OUTPUT_NAME dinput8)

enable_testing()
add_executable(aotr60_tests
    tests/test_main.cpp
    tests/test_core.cpp
    tests/test_patcher.cpp
    tests/test_frame_state.cpp
    tests/test_sites.cpp
    tests/test_camera_math.cpp
)
target_link_libraries(aotr60_tests PRIVATE aotr60_runtime)
target_compile_options(aotr60_tests PRIVATE ${AOTR60_COMMON_OPTIONS})
target_compile_definitions(aotr60_tests PRIVATE AOTR60_GAME_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/..")
add_test(NAME aotr60_tests COMMAND aotr60_tests)
