# gui/ — Qt 6 GUI application for openEB.

# Library sources (everything except main.cpp). Built once as a static library
# so the unit tests in tests/ can link the same code without recompiling it.
set(GUI_LIB_SOURCES
    main_window.cpp
    app/camera_controller.cpp
    app/file_converter.cpp
    app/file_frame_generator.cpp
    app/frame_pipeline.cpp
    app/icon_provider.cpp
    app/statistics_controller.cpp
    app/startup_environment.cpp
    app/theme_controller.cpp
    algo_bridge/algo_bridge.cpp
    algo_bridge/backends/cv_backends.cpp
    algo_bridge/backends/cv_vector_backends.cpp
    algo_bridge/backends/analytics_backends.cpp
    algo_bridge/backends/analytics_extra_backends.cpp
    algo_bridge/backends/display_backends.cpp
    algo_bridge/backends/filter_backends.cpp
    algo_bridge/backends/backend_factory.cpp
    algo_bridge/backends/backend_common.cpp
    algo_bridge/filter_chain.cpp
    config/config_manager.cpp
    display/event_display_widget.cpp
    display/display_strategy.cpp
    exporter/exporter_controller.cpp
    exporter/export_dialog.cpp
    panels/abstract_panel.cpp
    panels/algorithms_panel.cpp
    panels/biases_panel.cpp
    panels/devices_panel.cpp
    panels/display_panel.cpp
    panels/esp_panel.cpp
    panels/file_tools_panel.cpp
    panels/file_op_dialog.cpp
    panels/file_info_dialog.cpp
    recorder/record_dialog.cpp
    panels/information_panel.cpp
    panels/preprocessing_panel.cpp
    panels/roi_panel.cpp
    panels/settings_panel.cpp
    panels/statistics_panel.cpp
    panels/trigger_panel.cpp
    recorder/playback_controller.cpp
    recorder/playback_controls.cpp
    recorder/recorder_controller.cpp
)

# Phase 9 / Phase 4 — calibration wizard + tools (built when sources are present).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/calibration/calibration_wizard.cpp")
    list(APPEND GUI_LIB_SOURCES
        calibration/calibration_wizard.cpp
        calibration/circle_grid_display.cpp
        calibration/calibration_event_tap.cpp
        calibration/calibration_worker.cpp
        calibration/focus_dialog.cpp
    )
endif()

# Phase 10 — layout manager.
list(APPEND GUI_LIB_SOURCES
    widgets/custom_title_bar.cpp
    widgets/activity_bar.cpp
    config/layout_manager.cpp
)

# Phase 11 — jAER-inspired display / widgets (design §1.6).
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/display/space_time_display.cpp")
    list(APPEND GUI_LIB_SOURCES
        widgets/algo_window.cpp
        widgets/unified_roi_dialog.cpp
        display/frame_annotator.cpp
        display/space_time_display.cpp
    )
endif()

# Static library shared by the application and the unit tests.
add_library(gui_core STATIC ${GUI_LIB_SOURCES})
target_include_directories(gui_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gui_core PUBLIC
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    gui_algo
)

# Phase 2 public headers expose HAL facility types directly. Prefer the OpenEB
# 5.2 target name while retaining compatibility with alternate Linux exports.
if(TARGET Metavision::HAL)
    set(_gui_hal_target Metavision::HAL)
elseif(TARGET MetavisionSDK::hal)
    set(_gui_hal_target MetavisionSDK::hal)
else()
    message(FATAL_ERROR
        "The Metavision package does not export a supported HAL target "
        "(Metavision::HAL or MetavisionSDK::hal).")
endif()
target_link_libraries(gui_core PUBLIC ${_gui_hal_target})
unset(_gui_hal_target)

# Main executable — entry point only; all logic lives in gui_core.
add_executable(gui_for_openeb main.cpp)

# Qt resources — theme QSS template + Lucide-style SVG icons (design §3.1/§3.2).
# AUTORCC compiles these into the binary so the code can load ":/theme/..." and
# ":/icons/..." at runtime.
target_sources(gui_for_openeb PRIVATE
    resources/theme.qrc
    resources/icons.qrc
)

target_link_libraries(gui_for_openeb PRIVATE gui_core)

if(APPLE)
    # Keep the normal Linux executable/install contract below. On macOS the
    # application is a real bundle so that its runtime closure can live under
    # Contents/Frameworks instead of relying on development prefixes.
    set_target_properties(gui_for_openeb PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "GUI for openEB"
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.ebplus.gui-for-openeb"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "GUI for openEB ${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        INSTALL_RPATH "@executable_path/../Frameworks")

    function(_ebplus_get_imported_location output target)
        set(_location "")
        if(TARGET "${target}")
            foreach(_property IN ITEMS
                    IMPORTED_LOCATION_RELEASE
                    IMPORTED_LOCATION_RELWITHDEBINFO
                    IMPORTED_LOCATION_MINSIZEREL
                    IMPORTED_LOCATION_DEBUG
                    IMPORTED_LOCATION)
                get_target_property(_candidate "${target}" "${_property}")
                if(_candidate AND NOT _candidate MATCHES "-NOTFOUND$")
                    set(_location "${_candidate}")
                    break()
                endif()
            endforeach()
        endif()
        set(${output} "${_location}" PARENT_SCOPE)
    endfunction()

    # Dynamic OpenEB plugins are discovered at runtime, so they are not all
    # visible from gui_for_openeb's direct link line. Derive their selected
    # prefix from the imported package target; no developer path is recorded in
    # tracked CMake source.
    _ebplus_get_imported_location(_ebplus_openeb_hal_location Metavision::HAL)
    if(NOT _ebplus_openeb_hal_location)
        message(FATAL_ERROR
            "The macOS bundle requires an imported Metavision::HAL library location.")
    endif()
    get_filename_component(
        EBPLUS_MACOS_OPENEB_LIBRARY_DIR
        "${_ebplus_openeb_hal_location}"
        DIRECTORY)

    set(EBPLUS_MACOS_BUNDLE_SEARCH_DIRECTORIES
        "${EBPLUS_MACOS_OPENEB_LIBRARY_DIR}"
        "${EBPLUS_MACOS_OPENEB_LIBRARY_DIR}/metavision/hal/plugins"
        "${EBPLUS_MACOS_OPENEB_LIBRARY_DIR}/hdf5/plugin")
    set(EBPLUS_MACOS_BUNDLE_RUNTIME_SEED_LIBRARIES "")
    foreach(_runtime_target IN ITEMS
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::OpenGL
            Qt6::OpenGLWidgets
            MetavisionSDK::base
            MetavisionSDK::core
            MetavisionSDK::stream
            Metavision::HAL
            Metavision::HAL_discovery
            hdf5_ecf_codec
            ONNXRuntime::ONNXRuntime)
        _ebplus_get_imported_location(_runtime_location "${_runtime_target}")
        if(_runtime_location)
            get_filename_component(_runtime_directory "${_runtime_location}" DIRECTORY)
            list(APPEND EBPLUS_MACOS_BUNDLE_SEARCH_DIRECTORIES
                "${_runtime_directory}")
            list(APPEND EBPLUS_MACOS_BUNDLE_RUNTIME_SEED_LIBRARIES
                "${_runtime_location}")
        endif()
    endforeach()
    foreach(_opencv_library IN LISTS OpenCV_LIBS)
        set(_opencv_library_location "")
        if(TARGET "${_opencv_library}")
            _ebplus_get_imported_location(
                _opencv_library_location "${_opencv_library}")
        elseif(IS_ABSOLUTE "${_opencv_library}")
            set(_opencv_library_location "${_opencv_library}")
        endif()
        if(_opencv_library_location)
            get_filename_component(_opencv_library_directory
                "${_opencv_library_location}" DIRECTORY)
            list(APPEND EBPLUS_MACOS_BUNDLE_SEARCH_DIRECTORIES
                "${_opencv_library_directory}")
            list(APPEND EBPLUS_MACOS_BUNDLE_RUNTIME_SEED_LIBRARIES
                "${_opencv_library_location}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES EBPLUS_MACOS_BUNDLE_SEARCH_DIRECTORIES)
    list(REMOVE_DUPLICATES EBPLUS_MACOS_BUNDLE_RUNTIME_SEED_LIBRARIES)

    if(NOT DEFINED QT6_INSTALL_PREFIX OR NOT DEFINED QT6_INSTALL_PLUGINS)
        message(FATAL_ERROR
            "The macOS bundle requires Qt6 install-prefix and plugin metadata.")
    endif()
    get_filename_component(_ebplus_qtbase_prefix
        "${QT6_INSTALL_PREFIX}" REALPATH)
    set(EBPLUS_MACOS_QT_PLUGIN_DIRECTORY
        "${_ebplus_qtbase_prefix}/${QT6_INSTALL_PLUGINS}")
    set(_ebplus_qt_cocoa_plugin_candidate
        "${EBPLUS_MACOS_QT_PLUGIN_DIRECTORY}/platforms/libqcocoa.dylib")
    set(_ebplus_qt_mac_style_plugin_candidate
        "${EBPLUS_MACOS_QT_PLUGIN_DIRECTORY}/styles/libqmacstyle.dylib")
    set(_ebplus_qt_gif_plugin_candidate
        "${EBPLUS_MACOS_QT_PLUGIN_DIRECTORY}/imageformats/libqgif.dylib")
    set(_ebplus_qt_ico_plugin_candidate
        "${EBPLUS_MACOS_QT_PLUGIN_DIRECTORY}/imageformats/libqico.dylib")
    set(_ebplus_qt_jpeg_plugin_candidate
        "${EBPLUS_MACOS_QT_PLUGIN_DIRECTORY}/imageformats/libqjpeg.dylib")
    get_filename_component(EBPLUS_MACOS_QT_COCOA_PLUGIN
        "${_ebplus_qt_cocoa_plugin_candidate}" REALPATH)
    get_filename_component(EBPLUS_MACOS_QT_MAC_STYLE_PLUGIN
        "${_ebplus_qt_mac_style_plugin_candidate}" REALPATH)
    get_filename_component(EBPLUS_MACOS_QT_GIF_PLUGIN
        "${_ebplus_qt_gif_plugin_candidate}" REALPATH)
    get_filename_component(EBPLUS_MACOS_QT_ICO_PLUGIN
        "${_ebplus_qt_ico_plugin_candidate}" REALPATH)
    get_filename_component(EBPLUS_MACOS_QT_JPEG_PLUGIN
        "${_ebplus_qt_jpeg_plugin_candidate}" REALPATH)
    foreach(_qtbase_plugin IN ITEMS
            "${EBPLUS_MACOS_QT_COCOA_PLUGIN}"
            "${EBPLUS_MACOS_QT_MAC_STYLE_PLUGIN}"
            "${EBPLUS_MACOS_QT_GIF_PLUGIN}"
            "${EBPLUS_MACOS_QT_ICO_PLUGIN}"
            "${EBPLUS_MACOS_QT_JPEG_PLUGIN}")
        if(NOT EXISTS "${_qtbase_plugin}")
            message(FATAL_ERROR
                "The selected Qt runtime plugin is missing: ${_qtbase_plugin}")
        endif()
    endforeach()
    list(APPEND EBPLUS_MACOS_BUNDLE_SEARCH_DIRECTORIES
        "${_ebplus_qtbase_prefix}/lib")

    if(NOT DEFINED Qt6Svg_DIR OR Qt6Svg_DIR STREQUAL "")
        message(FATAL_ERROR
            "The macOS bundle requires the Qt6Svg package directory.")
    endif()
    # QtSvg is commonly supplied by a separate package from QtBase. Resolve
    # its CMake package directory rather than deriving a plugin directory from
    # an /opt-style framework symlink. This preserves the selected QtSvg
    # package even when its formula uses a distinct prefix.
    get_filename_component(_ebplus_qtsvg_cmake_directory
        "${Qt6Svg_DIR}" REALPATH)
    get_filename_component(_ebplus_qtsvg_cmake_parent
        "${_ebplus_qtsvg_cmake_directory}" DIRECTORY)
    get_filename_component(_ebplus_qtsvg_library_directory
        "${_ebplus_qtsvg_cmake_parent}" DIRECTORY)
    get_filename_component(EBPLUS_MACOS_QTSVG_PREFIX
        "${_ebplus_qtsvg_library_directory}" DIRECTORY)
    set(_ebplus_qtsvg_image_plugin_candidate
        "${EBPLUS_MACOS_QTSVG_PREFIX}/share/qt/plugins/imageformats/libqsvg.dylib")
    set(_ebplus_qtsvg_icon_plugin_candidate
        "${EBPLUS_MACOS_QTSVG_PREFIX}/share/qt/plugins/iconengines/libqsvgicon.dylib")
    # The Homebrew-facing image plugin can itself be a symlink into the QtSvg
    # formula. Stage its resolved file so the bundle never retains that link.
    get_filename_component(EBPLUS_MACOS_QTSVG_IMAGE_PLUGIN
        "${_ebplus_qtsvg_image_plugin_candidate}" REALPATH)
    get_filename_component(EBPLUS_MACOS_QTSVG_ICON_PLUGIN
        "${_ebplus_qtsvg_icon_plugin_candidate}" REALPATH)
    foreach(_qtsvg_plugin IN ITEMS
            "${EBPLUS_MACOS_QTSVG_IMAGE_PLUGIN}"
            "${EBPLUS_MACOS_QTSVG_ICON_PLUGIN}")
        if(NOT EXISTS "${_qtsvg_plugin}")
            message(FATAL_ERROR
                "The selected Qt SVG runtime plugin is missing: ${_qtsvg_plugin}")
        endif()
    endforeach()
    list(APPEND EBPLUS_MACOS_BUNDLE_SEARCH_DIRECTORIES
        "${_ebplus_qtsvg_library_directory}")

    # Build-tree RPATH entries can identify selected private runtimes that
    # imported target metadata does not expose. Keep only real producer
    # directories; install RPATH remains the bundle-relative contract above.
    foreach(_ebplus_build_rpath_entry IN LISTS CMAKE_BUILD_RPATH)
        if("${_ebplus_build_rpath_entry}" STREQUAL "" OR
           "${_ebplus_build_rpath_entry}" MATCHES "\\$<" OR
           NOT IS_ABSOLUTE "${_ebplus_build_rpath_entry}" OR
           NOT IS_DIRECTORY "${_ebplus_build_rpath_entry}")
            continue()
        endif()
        get_filename_component(_ebplus_build_rpath_directory
            "${_ebplus_build_rpath_entry}" REALPATH)
        if(IS_DIRECTORY "${_ebplus_build_rpath_directory}")
            list(APPEND EBPLUS_MACOS_BUNDLE_SEARCH_DIRECTORIES
                "${_ebplus_build_rpath_directory}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES EBPLUS_MACOS_BUNDLE_SEARCH_DIRECTORIES)

    get_target_property(_ebplus_bundle_output_name gui_for_openeb OUTPUT_NAME)
    if(NOT _ebplus_bundle_output_name OR
       _ebplus_bundle_output_name MATCHES "-NOTFOUND$")
        set(_ebplus_bundle_output_name "gui_for_openeb")
    endif()
    set(EBPLUS_MACOS_BUNDLE_DIRECTORY_NAME
        "${_ebplus_bundle_output_name}.app")
    set(EBPLUS_MACOS_BUNDLE_EXECUTABLE_NAME
        "${_ebplus_bundle_output_name}")

    set(_ebplus_bundle_verifier
        "${CMAKE_CURRENT_BINARY_DIR}/verify_macos_bundle.cmake")
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/verify_macos_bundle.cmake.in"
        "${_ebplus_bundle_verifier}"
        @ONLY)
    set(EBPLUS_MACOS_BUNDLE_VERIFY_SCRIPT "${_ebplus_bundle_verifier}")

    set(_ebplus_bundle_fixup_script
        "${CMAKE_CURRENT_BINARY_DIR}/macos_bundle_fixup.cmake")
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/macos_bundle_fixup.cmake.in"
        "${_ebplus_bundle_fixup_script}"
        @ONLY)

    install(TARGETS gui_for_openeb BUNDLE DESTINATION .)
    install(SCRIPT "${_ebplus_bundle_fixup_script}")

    unset(_ebplus_bundle_fixup_script)
    unset(_ebplus_bundle_output_name)
    unset(_ebplus_bundle_verifier)
    unset(_ebplus_openeb_hal_location)
    unset(_ebplus_qtbase_prefix)
    unset(_ebplus_qtsvg_library_directory)
    unset(_ebplus_qtsvg_cmake_directory)
    unset(_ebplus_qtsvg_cmake_parent)
    unset(_ebplus_qtsvg_image_plugin_candidate)
    unset(_ebplus_qtsvg_icon_plugin_candidate)
    unset(_ebplus_build_rpath_directory)
    unset(_ebplus_build_rpath_entry)
    unset(_opencv_library)
    unset(_opencv_library_directory)
    unset(_opencv_library_location)
    unset(_qtbase_plugin)
    unset(_ebplus_qt_cocoa_plugin_candidate)
    unset(_ebplus_qt_gif_plugin_candidate)
    unset(_ebplus_qt_ico_plugin_candidate)
    unset(_ebplus_qt_jpeg_plugin_candidate)
    unset(_ebplus_qt_mac_style_plugin_candidate)
    unset(_qtsvg_plugin)
    unset(_runtime_directory)
    unset(_runtime_location)
    unset(_runtime_target)
    unset(_ebplus_get_imported_location)
elseif(UNIX AND NOT APPLE)
    # Preserve the existing bin/../lib install layout intent on the final
    # executable. The previous STATIC-library RPATH could not affect runtime.
    set_target_properties(gui_for_openeb PROPERTIES
        BUILD_RPATH_USE_ORIGIN TRUE
        INSTALL_RPATH [[$ORIGIN/../lib]])
    install(TARGETS gui_for_openeb RUNTIME DESTINATION bin)
else()
    install(TARGETS gui_for_openeb RUNTIME DESTINATION bin)
endif()

# --- Unit tests (design §3.11) ---------------------------------------------
option(GUI_BUILD_TESTS "Build GUI unit tests" ON)
if(BUILD_TESTING AND GUI_BUILD_TESTS)
    add_subdirectory(tests)
endif()
