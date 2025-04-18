# Copyright(c) 2019 spdlog authors
# Distributed under the MIT License (http://opensource.org/licenses/MIT)

cmake_minimum_required(VERSION 3.5)

#---------------------------------------------------------------------------------------
# Start spdlog project
#---------------------------------------------------------------------------------------
include(GNUInstallDirs)
include(cmake/utils.cmake)
include(cmake/ide.cmake)

spdlog_extract_version()

#---------------------------------------------------------------------------------------
# Set default build to release
#---------------------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
endif()

project(spdlog VERSION ${SPDLOG_VERSION} LANGUAGES CXX)
message(STATUS "Build spdlog: ${SPDLOG_VERSION}")

#---------------------------------------------------------------------------------------
# Compiler config
#---------------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#---------------------------------------------------------------------------------------
# Set SPDLOG_MASTER_PROJECT to ON if we are building spdlog
#---------------------------------------------------------------------------------------
# Check if spdlog is being used directly or via add_subdirectory, but allow overriding
if (NOT DEFINED SPDLOG_MASTER_PROJECT)
    if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(SPDLOG_MASTER_PROJECT ON)
    else()
        set(SPDLOG_MASTER_PROJECT OFF)
    endif()
endif ()

# build shared option
if(NOT WIN32)
    option(SPDLOG_BUILD_SHARED "Build shared library" OFF)
endif()

# example options
option(SPDLOG_BUILD_EXAMPLE "Build example" ${SPDLOG_MASTER_PROJECT})
option(SPDLOG_BUILD_EXAMPLE_HO "Build header only example" OFF)

# testing options
option(SPDLOG_BUILD_TESTS "Build tests" ${SPDLOG_MASTER_PROJECT})
option(SPDLOG_BUILD_TESTS_HO "Build tests using the header only version" OFF)

# bench options
option(SPDLOG_BUILD_BENCH "Build benchmarks (Requires https://github.com/google/benchmark.git to be installed)" OFF)

# sanitizer options
option(SPDLOG_SANITIZE_ADDRESS "Enable address sanitizer in tests" OFF)

# install options
option(SPDLOG_INSTALL "Generate the install target" ${SPDLOG_MASTER_PROJECT})
option(SPDLOG_FMT_EXTERNAL "Use external fmt library instead of bundled" OFF)

if(WIN32)
	option(SPDLOG_WCHAR_SUPPORT "Support wchar api" OFF)
endif()

option(SPDLOG_NO_EXCEPTIONS "Compile with -fno-exceptions. Call abort() on any spdlog exceptions" OFF)


find_package(Threads REQUIRED)

message(STATUS "Build type: " ${CMAKE_BUILD_TYPE})

#---------------------------------------------------------------------------------------
# Static/Shared library (shared not supported in windows yet)
#---------------------------------------------------------------------------------------
set(SPDLOG_SRCS
        src/spdlog.cpp
        src/stdout_sinks.cpp
        src/fmt.cpp
        src/color_sinks.cpp
        src/file_sinks.cpp
        src/async.cpp)

set(SPDLOG_CFLAGS "${PROJECT_NAME}")

if (SPDLOG_BUILD_SHARED)
    if(WIN32)
        message(FATAL_ERROR "spdlog shared lib is not yet supported under windows")
    endif()
    add_library(spdlog SHARED ${SPDLOG_SRCS} ${SPDLOG_ALL_HEADERS})
else()
    add_library(spdlog STATIC ${SPDLOG_SRCS} ${SPDLOG_ALL_HEADERS})
endif()

add_library(spdlog::spdlog ALIAS spdlog)

target_compile_definitions(spdlog PUBLIC SPDLOG_COMPILED_LIB)
target_include_directories(spdlog PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_link_libraries(spdlog PUBLIC Threads::Threads)
spdlog_enable_warnings(spdlog)

set_target_properties(spdlog PROPERTIES VERSION ${SPDLOG_VERSION} SOVERSION ${SPDLOG_VERSION_MAJOR})
set_target_properties(spdlog PROPERTIES DEBUG_POSTFIX d)

#---------------------------------------------------------------------------------------
# Header only version
#---------------------------------------------------------------------------------------
add_library(spdlog_header_only INTERFACE)
add_library(spdlog::spdlog_header_only ALIAS spdlog_header_only)

target_include_directories(spdlog_header_only INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_link_libraries(spdlog_header_only INTERFACE Threads::Threads)


#---------------------------------------------------------------------------------------
# Use fmt package if using exertnal fmt
#---------------------------------------------------------------------------------------
if(SPDLOG_FMT_EXTERNAL)
    if (NOT TARGET fmt::fmt)
        find_package(fmt REQUIRED)
    endif ()

    set(SPDLOG_CFLAGS "${SPDLOG_CFLAGS} -DSPDLOG_FMT_EXTERNAL")

    target_compile_definitions(spdlog PUBLIC SPDLOG_FMT_EXTERNAL)
    target_link_libraries(spdlog PUBLIC fmt::fmt)

    target_compile_definitions(spdlog_header_only INTERFACE SPDLOG_FMT_EXTERNAL)
    target_link_libraries(spdlog_header_only INTERFACE fmt::fmt)
endif()

if(SPDLOG_WCHAR_SUPPORT)
	target_compile_definitions(spdlog PUBLIC SPDLOG_WCHAR_TO_UTF8_SUPPORT)
	target_compile_definitions(spdlog_header_only INTERFACE SPDLOG_WCHAR_TO_UTF8_SUPPORT)
 endif()

 if(SPDLOG_NO_EXCEPTIONS)
	target_compile_definitions(spdlog PUBLIC SPDLOG_NO_EXCEPTIONS)	

	target_compile_definitions(spdlog_header_only INTERFACE SPDLOG_NO_EXCEPTIONS)

    if(NOT MSVC)
        target_compile_options(spdlog PRIVATE -fno-exceptions)
    endif()
endif()


#---------------------------------------------------------------------------------------
# Build binaries
#---------------------------------------------------------------------------------------
if(SPDLOG_BUILD_EXAMPLE OR SPDLOG_BUILD_EXAMPLE_HO)
    message(STATUS "Generating examples")
    add_subdirectory(example)
endif()

if(SPDLOG_BUILD_TESTS OR SPDLOG_BUILD_TESTS_HO)
    message(STATUS "Generating tests")
    enable_testing()
    add_subdirectory(tests)
endif()

if(SPDLOG_BUILD_BENCH)
    message(STATUS "Generating benchmarks")
    add_subdirectory(bench)
endif()

#---------------------------------------------------------------------------------------
# Install
#---------------------------------------------------------------------------------------
if (SPDLOG_INSTALL)
    message(STATUS "Generating install")
    set(project_config_in "${CMAKE_CURRENT_LIST_DIR}/cmake/spdlogConfig.cmake.in")
    set(project_config_out "${CMAKE_CURRENT_BINARY_DIR}/spdlogConfig.cmake")
    set(config_targets_file "spdlogConfigTargets.cmake")
    set(version_config_file "${CMAKE_CURRENT_BINARY_DIR}/spdlogConfigVersion.cmake")
    set(export_dest_dir "${CMAKE_INSTALL_LIBDIR}/spdlog/cmake")
    set(pkgconfig_install_dir "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
    set(pkg_config "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc")

    #---------------------------------------------------------------------------------------
    # Include files
    #---------------------------------------------------------------------------------------
    install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" PATTERN "fmt/bundled" EXCLUDE)
    install(TARGETS spdlog spdlog_header_only EXPORT spdlog DESTINATION "${CMAKE_INSTALL_LIBDIR}/spdlog")

    if(NOT SPDLOG_FMT_EXTERNAL)
        install(DIRECTORY include/${PROJECT_NAME}/fmt/bundled/
                DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/fmt/bundled/")
    endif()

    #---------------------------------------------------------------------------------------
    # Package and version files
    #---------------------------------------------------------------------------------------
    configure_file("cmake/${PROJECT_NAME}.pc.in" "${pkg_config}" @ONLY)
    install(FILES "${pkg_config}" DESTINATION "${pkgconfig_install_dir}")

    install(EXPORT spdlog
            DESTINATION ${export_dest_dir}
            NAMESPACE spdlog::
            FILE ${config_targets_file})

    include(CMakePackageConfigHelpers)
    configure_file("${project_config_in}" "${project_config_out}" @ONLY)
    write_basic_package_version_file("${version_config_file}" COMPATIBILITY SameMajorVersion)
    install(FILES
            "${project_config_out}"
            "${version_config_file}" DESTINATION "${export_dest_dir}")

    #---------------------------------------------------------------------------------------
    # Support creation of installable packages
    #---------------------------------------------------------------------------------------
    include(cmake/spdlogCPack.cmake)

endif ()
