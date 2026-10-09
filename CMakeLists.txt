# NOVA GPU - CPU SOFTWARE RENDERER
# Cross-platform build: MSVC (Visual Studio), MinGW-w64, GCC and Clang.
cmake_minimum_required(VERSION 3.16)

project(nova_gpu
    VERSION 0.1.0
    DESCRIPTION "NOVA GPU - CPU software renderer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Single-config generators default to Release so that the demos and the
# benchmark are measured with optimizations on (a Debug build is ~10-20x slower).
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

option(NOVA_BUILD_TESTS "Build the unit tests" ON)
option(NOVA_BUILD_EXAMPLES "Build the examples" ON)
option(NOVA_BUILD_BENCHMARKS "Build the benchmark" ON)
option(NOVA_ENABLE_NATIVE "Optimize for the host CPU (-march=native / /arch:AVX2)" OFF)
option(NOVA_FAST_MATH "Enable fast-math style flags (off by default: it can break NaN tests)" OFF)

# --- renderer library --------------------------------------------------------
add_library(nova_core STATIC
    src/core/framebuffer.cpp
    src/core/thread_pool.cpp
    src/gpu/device.cpp
    src/gpu/command_processor.cpp
    src/camera/camera.cpp
    src/pipeline/vertex_processor.cpp
    src/pipeline/clipper.cpp
    src/pipeline/triangle_setup.cpp
    src/rasterizer/rasterizer.cpp
    src/lighting/lighting.cpp
    src/texture/texture.cpp
    src/texture/image_io.cpp
    src/debug/overlay.cpp
    src/debug/report.cpp
    src/platform/ppm_writer.cpp
    src/platform/console.cpp
)

target_include_directories(nova_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

find_package(Threads REQUIRED)
target_link_libraries(nova_core PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(nova_core PRIVATE /W4 /permissive- /utf-8)
    # The renderer is single-precision maths; /fp:fast is intentionally not used
    # because the pipeline relies on strict comparisons for depth and culling.
else()
    target_compile_options(nova_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(NOVA_ENABLE_NATIVE)
    if(MSVC)
        target_compile_options(nova_core PRIVATE /arch:AVX2)
    else()
        target_compile_options(nova_core PRIVATE -march=native)
    endif()
endif()

if(NOVA_FAST_MATH)
    if(NOT MSVC)
        target_compile_options(nova_core PRIVATE -ffast-math)
    endif()
endif()

# Keep every executable in one place so PPM output paths stay predictable.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/bin")
endforeach()

# --- tests -------------------------------------------------------------------
if(NOVA_BUILD_TESTS)
    enable_testing()
    add_executable(nova_tests
        tests/test_framework.cpp
        tests/test_math.cpp
        tests/test_barycentric.cpp
        tests/test_framebuffer.cpp
        tests/test_scene.cpp
        tests/test_frustum.cpp
        tests/test_debug.cpp
        tests/test_pipeline.cpp
        tests/test_rasterizer.cpp
        tests/test_clipping.cpp
        tests/test_shaders.cpp
        tests/test_texture.cpp
        tests/test_lighting.cpp
    )
    target_link_libraries(nova_tests PRIVATE nova_core)
    target_include_directories(nova_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_test(NAME nova_unit_tests COMMAND nova_tests)
endif()

# --- examples ----------------------------------------------------------------
if(NOVA_BUILD_EXAMPLES)
    add_executable(nova_triangle_demo examples/triangle_demo.cpp)
    target_link_libraries(nova_triangle_demo PRIVATE nova_core)

    add_executable(nova_cube_demo examples/cube_demo.cpp)
    target_link_libraries(nova_cube_demo PRIVATE nova_core)

    add_executable(nova_shader_demo examples/shader_demo.cpp)
    target_link_libraries(nova_shader_demo PRIVATE nova_core)

    add_executable(nova_texture_demo examples/texture_demo.cpp)
    target_link_libraries(nova_texture_demo PRIVATE nova_core)

    add_executable(nova_lighting_demo examples/lighting_demo.cpp)
    target_link_libraries(nova_lighting_demo PRIVATE nova_core)
endif()

# --- benchmarks --------------------------------------------------------------
if(NOVA_BUILD_BENCHMARKS)
    add_executable(nova_benchmark benchmarks/benchmark_nova.cpp)
    target_link_libraries(nova_benchmark PRIVATE nova_core)

    add_executable(nova_benchmark_vertex benchmarks/benchmark_vertex.cpp)
    target_link_libraries(nova_benchmark_vertex PRIVATE nova_core)
endif()

# --- install / package (Phase 7) ---------------------------------------------
# Everything the demos need at runtime is installed into one tree, so the
# packaged archive is directly usable after extracting it:
#
#     bin/      demos, benchmarks and the test runner
#     docs/     the documentation set
#     assets/   checked-in demo textures
#     README.md
#
# Build it with:   cmake --build <dir> --target package
# or end to end:   tools/package.sh   (Release build -> tests -> archive)
include(GNUInstallDirs)

set(NOVA_INSTALL_TARGETS "")
if(NOVA_BUILD_TESTS)
    list(APPEND NOVA_INSTALL_TARGETS nova_tests)
endif()
if(NOVA_BUILD_EXAMPLES)
    list(APPEND NOVA_INSTALL_TARGETS nova_triangle_demo nova_cube_demo nova_shader_demo)
    list(APPEND NOVA_INSTALL_TARGETS nova_texture_demo nova_lighting_demo)
endif()
if(NOVA_BUILD_BENCHMARKS)
    list(APPEND NOVA_INSTALL_TARGETS nova_benchmark nova_benchmark_vertex)
endif()

if(NOVA_INSTALL_TARGETS)
    install(TARGETS ${NOVA_INSTALL_TARGETS}
            RUNTIME DESTINATION bin
            BUNDLE DESTINATION bin)
endif()

install(FILES README.md DESTINATION .)
install(DIRECTORY docs/ DESTINATION docs
        FILES_MATCHING PATTERN "*.md")
install(DIRECTORY assets/ DESTINATION assets
        FILES_MATCHING PATTERN "*.ppm")

# --- CPack: the downloadable archive -----------------------------------------
set(CPACK_PACKAGE_NAME "nova-gpu")
set(CPACK_PACKAGE_VENDOR "NOVA GPU")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/hakarifenix/GPU-")
set(CPACK_PACKAGE_FILE_NAME
    "nova-gpu-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_STRIP_FILES ON)   # no debug symbols in a download
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_PACKAGE_CHECKSUM "SHA256")

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
