# Architecture

EB plus is split into two top-level layers: the **GUI application** (`gui/`) and the **self-developed algorithm library** (`algo/`). The openEB SDK (`openeb/`, Apache 2.0) provides the camera HAL and event decoding; the current `AlgoBridge` registry has 7 OpenEB FilterChain transforms, not the historical 30-wrapper catalog.

```
┌─────────────────────────────────────────────────────────┐
│                       gui/  (Qt 6)                       │
│  main_window ── widgets ── panels ── display ── recorder │
│        │                                  │              │
│        └──────── algo_bridge ─────────────┘              │
│                  │                                       │
│                  │ AlgoBackend (abstract)                │
│                  ├─ backends/*.cpp (self + openeb)       │
│                  └─ filter_chain                         │
└──────────────────┼──────────────────────────────────────┘
                   │
        ┌──────────┴──────────┐
        │    algo/ (C++17)     │
        │  cv / analytics /    │
        │  calibration / common│
        └──────────┬──────────┘
                   │
        ┌──────────┴──────────┐
        │  openeb/ SDK v5.2.0  │
        │  HAL · Core · Base   │
        └─────────────────────┘
```

## Directory Layout

```
GUI-for-openEB/
├── gui/                  # Qt 6 application
│   ├── main.cpp              # entry point; env-var defaults, OpenGL format, font
│   ├── main_window.*         # main window: title-bar menus, docks, signal wiring
│   ├── widgets/              # CustomTitleBar, ActivityBar, AlgoWindow,
│   │                         #   UnifiedRoiDialog
│   ├── panels/               # 11 sidebar panels (AbstractPanel base)
│   │   ├── abstract_panel.*      # base class: camera lifecycle decoupling
│   │   ├── settings_panel.*      # ActivityBar + QStackedWidget container
│   │   ├── devices_panel.*       # camera discovery/connection
│   │   ├── information_panel.*   # sensor metadata
│   │   ├── display_panel.*       # accumulation/FPS/palette
│   │   ├── statistics_panel.*    # event rate / drop rate / FPS
│   │   ├── biases_panel.*        # LL-bias sliders
│   │   ├── roi_panel.*           # unified ROI/RONI controls
│   │   ├── esp_panel.*           # Anti-Flicker / Trail / ERC
│   │   ├── trigger_panel.*       # Trigger In / Out
│   │   ├── preprocessing_panel.* # seven-transform FilterChain
│   │   ├── algorithms_panel.*    # algorithm selection + shared preproc + params
│   │   └── file_tools_panel.*    # recording / conversion / export
│   ├── display/              # OpenGL rendering
│   │   ├── event_display_widget.* # QOpenGLWidget, GLSL 3.30 core
│   │   ├── display_strategy.*     # IDisplayStrategy: Passive/Overlay/Replace/Standalone
│   │   ├── frame_annotator.*      # bbox/ID/trajectory/arrow overlays
│   │   └── space_time_display.*   # XYT 3D point cloud (VBO + GLSL)
│   ├── app/                  # controllers
│   │   ├── camera_controller.*    # camera lifecycle, HAL facility access
│   │   ├── frame_pipeline.*       # CD events → QImage rendering
│   │   ├── file_frame_generator.* # file-source frame generation, playback and ROI
│   │   ├── statistics_controller.*# event-rate computation
│   │   ├── file_converter.*       # background RAW/HDF5/CSV conversion
│   │   ├── icon_provider.*        # SVG icon cache (theme-adaptive)
│   │   ├── startup_environment.*  # platform startup defaults
│   │   └── theme_controller.*     # 5 colors × 3 modes
│   ├── algo_bridge/          # algorithm registry + filter chain
│   │   ├── algo_bridge.*          # AlgoBridge: registry and live instances
│   │   ├── algo_backend.h         # AlgoBackend base + AlgoResult + AlgoInfo
│   │   ├── filter_chain.*         # thread-safe seven-transform pipeline
│   │   └── backends/              # backend implementations
│   │       ├── backend_registry.h     # factory map
│   │       ├── backend_factory.cpp    # factory wiring
│   │       ├── backend_common.h       # shared param helpers (pint/pfloat/penum/...)
│   │       ├── cv_backends.cpp        # self CV algorithm backends
│   │       ├── cv_vector_backends.cpp # vector-output CV backends
│   │       ├── analytics_backends.cpp # analytics backends
│   │       ├── analytics_extra_backends.cpp
│   │       ├── display_backends.cpp   # display-mode wiring
│   │       └── filter_backends.cpp    # self filter backends
│   ├── recorder/             # RAW recording & playback
│   │   ├── recorder_controller.*
│   │   ├── playback_controller.*
│   │   └── playback_controls.*
│   ├── exporter/             # HDF5/AVI source-event export
│   ├── calibration/          # intrinsic wizard
│   ├── config/               # JSON config + layout persistence
│   │   ├── config_manager.*
│   │   └── layout_manager.*
│   ├── resources/            # Qt resources (compiled in)
│   │   ├── theme/            #   tokens.h + base.qss.in
│   │   ├── icons/            #   Lucide-style SVG icons
│   │   ├── theme.qrc
│   │   └── icons.qrc
│   └── tests/                # GUI unit tests (GTest + CTest)
├── algo/                  # self-developed algorithm library (26 registered)
│   ├── common/               # event packets, frame generator, filters, Kalman, LIF, ...
│   ├── cv/                   # 19 CV algorithms + noise_filter (9 modes)
│   ├── analytics/            # 7 analytics algorithms + e2vid/ ONNX inference
│   ├── calibration/          # intrinsic calibration
│   └── tests/                # algorithm tests (GTest + CTest)
├── openeb/                # openEB SDK (Apache 2.0, v5.2.0)
├── models/                # E2VID PyTorch → ONNX conversion (convert_to_onnx.py)
├── third_party/           # ONNX Runtime (user-installed, git-ignored)
├── devlog/                # design spec + compile guide + diagnostic reports
├── wiki/                  # this wiki
├── pic/                   # screenshots
├── run.sh                 # launcher (env vars)
├── CMakeLists.txt         # v1.9.0
├── LICENSE                # MIT (original code)
├── README.md              # English
└── README_CN.md           # Chinese
```

## Key Abstractions

### AlgoBridge

The central algorithm registry (`gui/algo_bridge/algo_bridge.cpp`) holds a
`std::unordered_map<std::string, AlgoInfo>` of 33 current entries: 26
self-developed (19 CV + 7 analytics) plus 7 OpenEB FilterChain transforms.
Unified ROI/RONI, the Devices Sensor Self-Test and the Tools calibration
workflow are intentionally outside this registry. Each entry has:

- `name` — registry key (e.g. `"object_tracker"`)
- `display_name` — UI label (e.g. `"Object Tracker"`)
- `category` — current entries use `"cv"`, `"analytics"` or `"openeb_filter"`
- `source` — `"self"` or `"openeb"`
- `display_mode` — `Passive` / `Overlay` / `Replace` / `Standalone`
- `params` — parameter metadata (name, label, type, default, min, max, enum options, mode_filter)

`list_algos()` enumerates definitions. The Algorithms panel obtains a live
instance with `find_or_create()` and toggles it through
`AlgoInstance::set_enabled()`; it reads and writes parameters with `set_param`
and `get_param`.

### AlgoBackend

Abstract base (`algo_backend.h`) implemented by each backend in `backends/`.
It defines `set_param` / `get_param` / `process` / `reset` / `result`. The
bridge owns mutex-protected `AlgoInstance` objects: SDK data-thread callbacks
push event batches while GUI-side parameter, enable and result calls
synchronize with them. A flood guard can auto-disable a persistently
overloaded instance.

### IDisplayStrategy

Four strategies (`display_strategy.h`) controlling how an algorithm's `AlgoResult` reaches the display: `Passive` (nothing), `Overlay` (annotate the live frame), `Replace` (swap the frame), `Standalone` (open an `AlgoWindow`).

### FilterChain and unified ROI

`FilterChain` is a thread-safe seven-transform event pipeline
(`filter_chain.h`) applied at render time to the display path. Unified ROI is
separate: it uses software crop/RONI for file playback and hardware `I_ROI`
for a live camera, then communicates the effective source state to algorithms.

### AbstractPanel

Base class for all sidebar panels (`panels/abstract_panel.*`). Decouples panels from camera lifecycle — panels react to camera start/stop signals rather than holding direct camera references, so they work correctly across camera/file-mode switches.

## Data Flow

### Online camera mode

```
Camera (HAL) → I_EventsStream callback → FramePipeline
    → FilterChain (display path) → EventDisplayWidget (OpenGL)
    → AlgoBridge.process(events) → AlgoResult → IDisplayStrategy → display/AlgoWindow
```

- Live callbacks push filtered batches directly to mutex-protected live
  `AlgoInstance` objects; the flood guard can auto-disable an overloaded
  instance.
- Event drop rate (`total_dropped / total_pushed`) is computed per algorithm
  instance and shown in the Information/Statistics panel.

### File playback mode

```
Event file → FileFrameGenerator → FramePipeline
    → FilterChain (applied to window_events) → display + AlgoBridge
```

- Playback accepts `.raw`, `.hdf5`, `.h5` and `.dat` file selectors; it uses
  buffered `FileFrameGenerator` playback with seek/pause/resume/loop support.
- Loop/seek/source-state handlers reset temporal algorithm state.
- Unified file ROI and FilterChain are distinct operations; neither statement
  replaces numerical or all-algorithm validation.

## Threading Model

- **GUI thread** — panel interaction, display rendering, and algorithm
  parameter/enable/result operations.
- **SDK data thread** — the openEB event-stream callback applies the
  mutex-protected `FilterChain` and pushes batches to mutex-protected live
  algorithm instances.
- **File converter thread** — background RAW/HDF5/CSV conversion (`file_converter.cpp`).

## Configuration & Persistence

- `ConfigManager` (`config/config_manager.*`) — JSON serialization for algo params and camera config; versioned schema (`"version": 1`).
- `LayoutManager` (`config/layout_manager.*`) — dock/window geometry to JSON.
- `QSettings` — theme color/mode, sidebar state, recent files.
- `.bias` files — camera bias presets.

## Build System

- `CMakeLists.txt` (root) — project version 1.9.0, C++17, GCC 15 `<cstdint>` fix.
- `find_package` for Qt6, MetavisionSDK 5.2.0, OpenCV.
- ONNX Runtime is optional; a portable runtime/model pair and inference remain
  separate qualification work.
- `enable_testing()` before `add_subdirectory` so GUI/algo tests register with CTest.
- `gui/tests/` and `algo/tests/` use `gtest_discover_tests()`.
