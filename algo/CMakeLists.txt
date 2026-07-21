# algo/ — self-developed event processing algorithms (C++17, no Qt dependency).
# Linked by gui/ via the gui_algo target.

add_library(gui_algo STATIC
    common/frame_generator.cpp
    calibration/intrinsic.cpp
)

target_include_directories(gui_algo PUBLIC
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(gui_algo PUBLIC
    MetavisionSDK::base
    MetavisionSDK::core
    MetavisionSDK::stream
    ${OpenCV_LIBS}
)

# Optional: ONNX Runtime for E2VID neural-network inference.
# AUTO preserves the heuristic fallback, ON requires a complete installation,
# and OFF skips discovery entirely.
set(EBPLUS_ONNXRUNTIME_MODE "AUTO" CACHE STRING
    "ONNX Runtime support mode: AUTO, ON, or OFF")
set_property(CACHE EBPLUS_ONNXRUNTIME_MODE PROPERTY STRINGS AUTO ON OFF)
set(EBPLUS_ONNXRUNTIME_ROOT "" CACHE PATH
    "Optional ONNX Runtime installation prefix")

set(_ebplus_onnxruntime_modes AUTO ON OFF)
if(NOT EBPLUS_ONNXRUNTIME_MODE IN_LIST _ebplus_onnxruntime_modes)
    message(FATAL_ERROR
        "Invalid EBPLUS_ONNXRUNTIME_MODE='${EBPLUS_ONNXRUNTIME_MODE}'. "
        "Allowed values are AUTO, ON, and OFF.")
endif()
unset(_ebplus_onnxruntime_modes)

function(_ebplus_find_onnxruntime_pair root output_include output_library)
    get_filename_component(_root "${root}" ABSOLUTE
        BASE_DIR "${CMAKE_SOURCE_DIR}")

    set(_include_paths
        "${_root}/include"
        "${_root}/include/onnxruntime"
        "${_root}/include/onnxruntime/core/session")
    set(_library_paths "")
    if(CMAKE_LIBRARY_ARCHITECTURE)
        list(APPEND _library_paths
            "${_root}/lib/${CMAKE_LIBRARY_ARCHITECTURE}")
    endif()
    list(APPEND _library_paths
        "${_root}/lib64"
        "${_root}/lib")

    unset(_ebplus_onnxruntime_probe_include)
    unset(_ebplus_onnxruntime_probe_include CACHE)
    unset(_ebplus_onnxruntime_probe_library)
    unset(_ebplus_onnxruntime_probe_library CACHE)
    find_path(_ebplus_onnxruntime_probe_include
        NAMES onnxruntime_cxx_api.h
        PATHS ${_include_paths}
        NO_DEFAULT_PATH)
    if(_ebplus_onnxruntime_probe_include AND
       NOT EXISTS "${_ebplus_onnxruntime_probe_include}/onnxruntime_c_api.h")
        set(_ebplus_onnxruntime_probe_include
            "_ebplus_onnxruntime_probe_include-NOTFOUND")
    endif()
    find_library(_ebplus_onnxruntime_probe_library
        NAMES onnxruntime
        PATHS ${_library_paths}
        NO_DEFAULT_PATH)

    set(${output_include} "${_ebplus_onnxruntime_probe_include}" PARENT_SCOPE)
    set(${output_library} "${_ebplus_onnxruntime_probe_library}" PARENT_SCOPE)
    unset(_ebplus_onnxruntime_probe_include)
    unset(_ebplus_onnxruntime_probe_include CACHE)
    unset(_ebplus_onnxruntime_probe_library)
    unset(_ebplus_onnxruntime_probe_library CACHE)
endfunction()

set(_ebplus_onnxruntime_target "")
set(_ebplus_onnxruntime_include "")
set(_ebplus_onnxruntime_library "")
set(_ebplus_onnxruntime_root "")
set(_ebplus_onnxruntime_partial_roots "")

if(EBPLUS_ONNXRUNTIME_MODE STREQUAL "OFF")
    message(STATUS
        "ONNX Runtime support is OFF; E2VID will use heuristic fallback.")
else()
    if(TARGET ONNXRuntime::ONNXRuntime)
        if(EBPLUS_ONNXRUNTIME_ROOT)
            message(FATAL_ERROR
                "ONNXRuntime::ONNXRuntime already exists, so "
                "EBPLUS_ONNXRUNTIME_ROOT cannot also select a different installation.")
        endif()
        set(_ebplus_onnxruntime_target ONNXRuntime::ONNXRuntime)
        message(STATUS
            "Using pre-existing ONNXRuntime::ONNXRuntime target.")
    elseif(EBPLUS_ONNXRUNTIME_ROOT)
        _ebplus_find_onnxruntime_pair(
            "${EBPLUS_ONNXRUNTIME_ROOT}"
            _ebplus_onnxruntime_include
            _ebplus_onnxruntime_library)
        if(_ebplus_onnxruntime_include AND _ebplus_onnxruntime_library)
            set(_ebplus_onnxruntime_root "${EBPLUS_ONNXRUNTIME_ROOT}")
        elseif(EBPLUS_ONNXRUNTIME_MODE STREQUAL "ON")
            message(FATAL_ERROR
                "EBPLUS_ONNXRUNTIME_ROOT does not contain the required "
                "ONNX Runtime headers and library: "
                "${EBPLUS_ONNXRUNTIME_ROOT}")
        else()
            message(STATUS
                "EBPLUS_ONNXRUNTIME_ROOT does not contain a complete "
                "ONNX Runtime header/library pair; E2VID will use heuristic fallback.")
        endif()
    else()
        set(_ebplus_onnxruntime_candidate_roots
            "${CMAKE_SOURCE_DIR}/third_party/onnxruntime")
        list(APPEND _ebplus_onnxruntime_candidate_roots
            ${CMAKE_PREFIX_PATH})
        if(UNIX AND NOT APPLE)
            list(APPEND _ebplus_onnxruntime_candidate_roots
                /opt/onnxruntime
                /usr/local
                /usr)
        endif()
        list(REMOVE_DUPLICATES _ebplus_onnxruntime_candidate_roots)

        foreach(_candidate_root IN LISTS _ebplus_onnxruntime_candidate_roots)
            if(_candidate_root STREQUAL "")
                continue()
            endif()
            _ebplus_find_onnxruntime_pair(
                "${_candidate_root}"
                _candidate_include
                _candidate_library)
            if(_candidate_include AND _candidate_library)
                set(_ebplus_onnxruntime_include "${_candidate_include}")
                set(_ebplus_onnxruntime_library "${_candidate_library}")
                set(_ebplus_onnxruntime_root "${_candidate_root}")
                break()
            elseif(_candidate_include OR _candidate_library)
                list(APPEND _ebplus_onnxruntime_partial_roots
                    "${_candidate_root}")
            endif()
        endforeach()

        unset(_candidate_root)
        unset(_candidate_include)
        unset(_candidate_library)
        unset(_ebplus_onnxruntime_candidate_roots)

        if(NOT _ebplus_onnxruntime_include OR
           NOT _ebplus_onnxruntime_library)
            if(EBPLUS_ONNXRUNTIME_MODE STREQUAL "ON")
                message(FATAL_ERROR
                    "ONNX Runtime mode is ON, but no complete header/library "
                    "pair was found in a single candidate prefix. Partial roots: "
                    "${_ebplus_onnxruntime_partial_roots}")
            else()
                message(STATUS
                    "No complete ONNX Runtime header/library pair was found; "
                    "E2VID will use heuristic fallback.")
            endif()
        endif()
    endif()

    if(_ebplus_onnxruntime_include AND _ebplus_onnxruntime_library)
        add_library(ONNXRuntime::ONNXRuntime UNKNOWN IMPORTED GLOBAL)
        set_target_properties(ONNXRuntime::ONNXRuntime PROPERTIES
            IMPORTED_LOCATION "${_ebplus_onnxruntime_library}"
            INTERFACE_INCLUDE_DIRECTORIES "${_ebplus_onnxruntime_include}")
        set(_ebplus_onnxruntime_target ONNXRuntime::ONNXRuntime)
        message(STATUS "ONNX Runtime found in: ${_ebplus_onnxruntime_root}")
        message(STATUS "  include: ${_ebplus_onnxruntime_include}")
        message(STATUS "  library: ${_ebplus_onnxruntime_library}")
    endif()

    if(_ebplus_onnxruntime_target)
        target_compile_definitions(gui_algo PUBLIC GUI_ALGO_HAS_ONNXRUNTIME)
        target_link_libraries(gui_algo PUBLIC ${_ebplus_onnxruntime_target})
        message(STATUS "E2VID ONNX Runtime support is enabled.")
    endif()
endif()

unset(_ebplus_onnxruntime_target)
unset(_ebplus_onnxruntime_include)
unset(_ebplus_onnxruntime_library)
unset(_ebplus_onnxruntime_root)
unset(_ebplus_onnxruntime_partial_roots)
