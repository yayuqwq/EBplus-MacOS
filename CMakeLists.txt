cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported. "
        "Use a build directory under the repository .build/ directory.")
endif()

project(GUI_for_openEB LANGUAGES CXX VERSION 1.9.0)

if(APPLE AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(FATAL_ERROR
        "The macOS install prefix must be set explicitly. "
        "Use a repository-local CMAKE_INSTALL_PREFIX.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# GCC 15+ no longer implicitly includes <cstdint>; add it globally so that
# uint8_t / uint16_t etc. used throughout the OpenEB headers stay available.
# See doc/compile.md for details.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "15")
    add_compile_options("-include;cstdint")
endif()

# --- External dependencies -------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
find_package(
    MetavisionSDK 5.2.0
    CONFIG
    REQUIRED
    COMPONENTS base core stream
)

foreach(_metavision_target IN ITEMS
        MetavisionSDK::base
        MetavisionSDK::core
        MetavisionSDK::stream)
    if(NOT TARGET ${_metavision_target})
        message(FATAL_ERROR
            "The MetavisionSDK package is missing required target "
            "'${_metavision_target}'.")
    endif()
endforeach()
unset(_metavision_target)

# The isolated macOS profile is validated specifically against OpenEB 5.2.0.
# Linux keeps the package's normal minimum-version compatibility semantics.
if(APPLE)
    if(NOT DEFINED MetavisionSDK_VERSION OR MetavisionSDK_VERSION STREQUAL "")
        message(FATAL_ERROR
            "The macOS OpenEB package did not report MetavisionSDK_VERSION; "
            "version 5.2.0 is required.")
    elseif(NOT MetavisionSDK_VERSION VERSION_EQUAL "5.2.0")
        message(FATAL_ERROR
            "The macOS OpenEB profile requires MetavisionSDK 5.2.0, "
            "but found ${MetavisionSDK_VERSION}.")
    endif()
endif()

find_package(OpenCV REQUIRED COMPONENTS
    core
    imgproc
    imgcodecs
    calib3d
    videoio
)

# Qt MOC / UIC / RCC auto-processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# --- Testing ---------------------------------------------------------------
# CTest defines BUILD_TESTING (ON by default) and enables test registration
# before gui/tests calls gtest_discover_tests().
include(CTest)

if(BUILD_TESTING)
    set(
        EBPLUS_GTEST_DISCOVERY_TIMEOUT
        30
        CACHE STRING
        "Maximum seconds allowed for GoogleTest test enumeration"
    )

    if(NOT "${EBPLUS_GTEST_DISCOVERY_TIMEOUT}" MATCHES "^[1-9][0-9]*$")
        message(FATAL_ERROR
            "EBPLUS_GTEST_DISCOVERY_TIMEOUT must be a positive integer")
    endif()
endif()

if(APPLE)
    # Keep headers from explicitly selected imported packages ahead of
    # compiler-provided system include paths on macOS.
    set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
endif()

add_subdirectory(algo)
add_subdirectory(gui)
if(BUILD_TESTING)
    add_subdirectory(algo/tests)
endif()
