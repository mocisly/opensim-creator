include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/osc_strict_compiler_options.cmake)  # OSC_STRICT_COMPILER_OPTIONS

set(
    OSC_FORCE_ASSERTS_ENABLED ON
    CACHE BOOL
    "force-enable/-disable OSC's runtime assertions - even if building a release build"
)
set(OSC_RUNTIME_PERF_MEASUREMENTS_ENABLED ON
    CACHE BOOL
    "enable/disable whether performance counters are collected at runtime whenever the `OSC_PERF` macro is used in the source code"
)
set(OSC_EMSCRIPTEN OFF
    CACHE BOOL
    "enable special build parameters for emscripten (emsdk) - don't try this unless you know what you're doing ;)"
)
mark_as_advanced(OSC_EMSCRIPTEN)

# find library packages
if (NOT ${OSC_EMSCRIPTEN})
    # These libraries are either provided by emscripten later on, or aren't
    # supported by the emscripten build (they're #ifdef'd out in `oscar`'s
    # source code).
    find_package(glad REQUIRED CONFIG)
    find_package(SDL3 REQUIRED CONFIG)
endif()
find_package(imgui REQUIRED CONFIG)
find_package(implot REQUIRED CONFIG)
find_package(tomlplusplus REQUIRED CONFIG)
find_package(stb REQUIRED CONFIG)
find_package(lunasvg REQUIRED CONFIG)
find_package(unordered_dense REQUIRED CONFIG)
find_package(Threads REQUIRED)  # implicitly required by SDL3

# gather source code, exclude tests
file(GLOB_RECURSE OSC_SOURCE_AND_HEADER_FILES
    LIST_DIRECTORIES FALSE
    "*.cpp"
)
list(FILTER OSC_SOURCE_AND_HEADER_FILES EXCLUDE REGEX "testing")
list(FILTER OSC_SOURCE_AND_HEADER_FILES EXCLUDE REGEX "\.tests\.cpp")

add_library(oscar STATIC
    ${OSC_SOURCE_AND_HEADER_FILES}
)
set_target_properties(oscar PROPERTIES
    CXX_EXTENSIONS OFF
)
target_compile_features(oscar PUBLIC
    cxx_std_23
)
target_compile_options(oscar PRIVATE
    ${OSC_STRICT_COMPILER_OPTIONS}
)
target_compile_definitions(oscar
PUBLIC
    $<$<BOOL:${OSC_FORCE_ASSERTS_ENABLED}>:OSC_FORCE_ASSERTS_ENABLED>
    $<$<BOOL:${OSC_RUNTIME_PERF_MEASUREMENTS_ENABLED}>:OSC_RUNTIME_PERF_MEASUREMENTS_ENABLED>
)
target_include_directories(oscar
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..  # so that `#include <liboscar/HEADER.h>` works
)
target_link_libraries(oscar
PRIVATE
    # these libraries are either provided by emscripten later on, or aren't
    # supported by the emscripten build (they're #ifdef'd out)
    $<$<NOT:$<BOOL:${OSC_EMSCRIPTEN}>>:glad>
    $<$<NOT:$<BOOL:${OSC_EMSCRIPTEN}>>:SDL3::SDL3>

    unordered_dense::unordered_dense
    tomlplusplus::tomlplusplus
    imgui
    implot
    stb
    lunasvg::lunasvg
    Threads::Threads  # implicitly required by SDL3
)

if(BUILD_TESTING)
    add_subdirectory(testing)
endif()
