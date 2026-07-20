# Linux/macOS 平台功能对齐矩阵

## 1. 使用说明

本矩阵是 Milestone 1 对当前 tracked source 的静态审计结果，用于后续 macOS 功能对齐。审计基线为分支 `docs/linux-baseline-inventory`、提交 `d80642817d2581687aa0b2a55f171c0c1e84901a`，审计日期为 2026-07-18。

- 本轮未执行 CMake configure、构建、GUI 启动、RAW 回放、真实相机、算法、模型或导出测试。
- `Linux source status` 描述当前源码和应用接线；`Linux runtime status` 描述运行验证状态。代码存在或已接入应用不等于运行验证通过。
- 本轮所有可运行能力均保持 `Requires verification`；证据不足时使用 `Unknown`。
- macOS 尚未进入对应实施阶段的项目统一标为 `Not started`，只有发现明确且已证实的阻塞条件时才使用 `Blocked`。
- 静态分析无法证明必然错误的事项统一写为“潜在风险，需要运行时验证”，不视为已经复现的 Bug。

状态值限定为：`Implemented in source`、`Wired into application`、`Documented only`、`Previously reported`、`Requires verification`、`Unknown`、`Not started`、`In progress`、`Blocked`、`Verified`、`Not applicable`。

## 2. 算法注册计数

从当前 HEAD 的 `AlgoBridge::AlgoBridge()` 及七个注册函数重新统计：

| Category | Unique registered names |
| --- | ---: |
| OpenEB filters | 10 |
| OpenEB frame modes | 7 |
| OpenEB preprocessors | 7 |
| OpenEB utilities | 6 |
| Self-developed CV | 21 |
| Self-developed analytics | 8 |
| Self-developed calibration | 1 |
| Total | 60 |

统计依据为最终写入 `AlgoBridge::registry_` 的唯一 `name`：59 个 `add({...})` 调用，加上手动写入的 `sensor_self_test`。当前没有重复注册名。测试名称、backend 类名、注释和文件名均未计入。源码顶部仍写“29 个自研模块 + 30 个 OpenEB 能力 = 59 项”，与当前实际 30 个自研、30 个 OpenEB、共 60 项不一致。

## 3. Build and launch

| ID | Feature area | Feature | Linux source status | Linux runtime status | macOS status | Evidence | Required verification | Target milestone | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| BLD-001 | Build | CMake configure | Implemented in source | Requires verification | Not started | `CMakeLists.txt`：`cmake_minimum_required(VERSION 3.16)`，加入 `algo`、`gui`、`algo/tests` | 在干净的 Linux 与 macOS build tree 配置并保存日志 | M3 | 当前未执行 configure |
| BLD-002 | Build | C++17 compile | Implemented in source | Requires verification | Not started | `CMakeLists.txt`：`CMAKE_CXX_STANDARD 17`、required、无扩展 | Linux 和 macOS arm64 完整编译 | M3 | GCC 15+ 另有全局 `-include;cstdint` 条件 |
| BLD-003 | Build | Qt6 discovery | Implemented in source | Requires verification | Not started | `find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)` | 验证 Linux Qt 平台插件与 macOS Cocoa/OpenGL | M3 | macOS Qt Cocoa 尚未配置或验证 |
| BLD-004 | Build | MetavisionSDK 5.2.0 discovery | Implemented in source | Requires verification | Verified | OpenEB 5.2 arm64 已在隔离 build/install prefix 完成 configure、full build 和 install；CMake、target artifacts 与 `otool` 审核未发现 5.1.1 污染 | 在 M3 继续验证 EBplus 顶层 consumer configure 和 target 接线 | M2/M3 | Repository-local OpenEB 5.2 SDK 基础 discovery/build/install 已验证；`gui_core` consumer 行为仍属于 EBplus configure 范围 |
| BLD-005 | Build | OpenCV discovery | Implemented in source | Requires verification | Not started | `find_package(OpenCV REQUIRED)`；`gui_algo` 链接 `${OpenCV_LIBS}` | 验证 arm64 headers、libraries 和 codec 能力 | M3 | 未限定具体 OpenCV 组件或版本 |
| BLD-006 | Build | GTest discovery | Implemented in source | Requires verification | Not started | `algo/tests/CMakeLists.txt` 和 `gui/tests/CMakeLists.txt`：`find_package(GTest REQUIRED)` | 配置默认 tests 开关并运行 CTest | M3 | `GUI_BUILD_TESTS` 只控制 GUI tests；`algo/tests` 顶层无条件加入 |
| BLD-007 | Build | ONNX Runtime discovery | Implemented in source | Requires verification | Not started | `algo/CMakeLists.txt`：`find_path`、`find_library`，搜索 bundled、`/usr`、`/usr/local`、`/opt/onnxruntime` | 验证明确 prefix、arm64 library 和 E2VID session | M3/M7 | 当前搜索路径和 `libonnxruntime.so` 注释具有 Linux 假设 |
| BLD-008 | Build | Main executable target | Implemented in source | Requires verification | Not started | `gui/CMakeLists.txt`：STATIC `gui_algo`、STATIC `gui_core`、executable `gui_for_openeb` | 构建并确认实际输出路径 | M3 | `install(TARGETS ... RUNTIME DESTINATION bin)` |
| BLD-009 | Launch | Linux launcher | Wired into application | Requires verification | Not applicable | `run.sh`：执行 `build/gui/gui_for_openeb` | Linux shell launch smoke test | M1/M4 | Bash、Wayland/XCB、`LD_LIBRARY_PATH` 与 `/usr/local` 专用假设 |
| BLD-010 | Runtime integration | Dynamic library resolution | Implemented in source | Requires verification | In progress | 三个要求的 OpenEB CLI 已在无 `DYLD_LIBRARY_PATH`/`DYLD_FALLBACK_LIBRARY_PATH` 环境直接运行，并完成全部安装 Mach-O 的 `otool -L/-l` 审核 | 验证 EBplus GUI executable、STATIC `gui_algo` 的 RPATH 传递和 ONNX Runtime linkage | M3/M8 | OpenEB 三 CLI 与 SDK/HAL/plugin 链路已验证；EBplus GUI 和 ONNX 仍 pending |
| BLD-011 | Runtime integration | HAL plugin discovery | Wired into application | Requires verification | Verified | Repository-local OpenEB 5.2 同时加载 `hal_plugin_prophesee` 与 `hal_plugin_centuryarks`；一台 `31f7:0003` 相机由 CenturyArks plugin claim，成功 open/reopen 并通过 bounded CD event delivery，未回退到 5.1.1 | 在 M6 验证 EBplus `CameraController`、GUI live path、facilities、shutdown 和 physical reconnect | M2/M6 | OpenEB HAL discovery/live delivery 已验证；`CAM-001` 至 `CAM-018` 的 EBplus 应用路径状态保持不变 |
| BLD-012 | Runtime integration | HDF5 plugin discovery | Wired into application | Requires verification | In progress | Repository-local `HDF5_PLUGIN_PATH` 下 RAW→HDF5、ECF 回读及外部 `h5dump` 解码已通过 | 验证 EBplus GUI 的 HDF5 打开、转换和导出路径 | M2/M5/M7 | OpenEB CLI 路径已验证；EBplus GUI 路径仍 pending |

## 4. GUI

| ID | Feature area | Feature | Linux source status | Linux runtime status | macOS status | Evidence | Required verification | Target milestone | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| GUI-001 | GUI | Application entry and main window | Wired into application | Requires verification | Not started | `gui/main.cpp::main()` 创建 `QApplication` 和 `gui::MainWindow` | GUI launch、窗口显示和退出 | M4 | 请求 OpenGL 3.3 core profile |
| GUI-002 | GUI | Main event display | Wired into application | Requires verification | Not started | `MainWindow` 将 `EventDisplayWidget` 设为 central widget；`FramePipeline::frame_ready` 更新画面 | live 与 RAW 帧显示 | M4/M5/M6 | OpenGL context 行为是潜在风险，需要运行时验证 |
| GUI-003 | GUI | Menus | Wired into application | Requires verification | Not started | `MainWindow::build_menus()`：File、View、Theme、Tools、Help | 检查每个 action、快捷键和 modal dialog | M4 | 菜单位于 `CustomTitleBar`，不是原生 `QMenuBar` |
| GUI-004 | GUI | File picker and recent files | Wired into application | Requires verification | Not started | `MainWindow::on_open_file()`、`on_open_recent_file()`、`QSettings("recentFiles")` | 选择、重开、缺失文件和路径字符测试 | M4/M5 | Recent files 会写入平台默认 QSettings 位置 |
| GUI-005 | GUI | Camera selection | Wired into application | Requires verification | Not started | `DevicesPanel` signals → `MainWindow` → `CameraController::connect_first_available/connect_serial` | 枚举、第一台设备和指定 serial | M6 | facility 可用性依具体设备，需运行时验证 |
| GUI-006 | GUI | Settings sidebar and panels | Wired into application | Requires verification | Not started | `SettingsPanel` 聚合 Devices、Information、Statistics、Display、Biases、ROI、ESP、Trigger、Preprocessing、Algorithms、File Tools | 逐 panel 交互和状态切换 | M4/M6/M7 | 侧栏活动组通过 QSettings 持久化 |
| GUI-007 | GUI | Status display | Wired into application | Requires verification | Not started | `MainWindow::build_status_bar()`；connection、event rate、timestamp、recording 状态信号接线 | live、file、error、recording 状态更新 | M4/M5/M6 | 未运行验证跨线程更新时序 |
| GUI-008 | GUI | Camera parameter controls | Wired into application | Requires verification | Not started | Biases、ROI、ESP、Trigger panels 绑定 `CameraController` facilities | 在支持和不支持 facility 的设备上测试 | M6 | 潜在风险，需要运行时验证 |
| GUI-009 | GUI | Playback controls | Wired into application | Requires verification | Not started | `PlaybackControls` 连接 `PlaybackController` 的 play/pause/step/seek/loop/rate | 完整 RAW transport smoke test | M5 | 仅 file source 显示 playback dock |
| GUI-010 | GUI | 2D visualization and overlays | Wired into application | Requires verification | Not started | `EventDisplayWidget`、`FrameAnnotator`、`DisplayStrategy` | palette、ROI、bbox、trajectory、line/circle overlay | M4/M5/M7 | 算法输出可见性是潜在风险，需要运行时验证 |
| GUI-011 | GUI | XYT 3D visualization | Wired into application | Requires verification | Not started | `SpaceTimeDisplay`、`MainWindow::on_open_xyt_view()`、`xyt_visualizer` | OpenGL point cloud、live/RAW 时间轴与清理 | M4/M5/M7 | QOpenGLWidget 和 macOS core profile 兼容性未验证 |
| GUI-012 | GUI | Standalone algorithm windows | Wired into application | Requires verification | Not started | `MainWindow::on_open_algo_window()` 创建 `AlgoWindow`，按 display mode 路由 | 每个 Standalone/Overlay/Replace 模式 | M7 | 处理结果和关闭时 disable 行为需运行时验证 |
| GUI-013 | GUI | Error and information dialogs | Wired into application | Requires verification | Not started | `QMessageBox` 用于 camera、file、record、config、layout、self-test 等结果 | 常见失败路径及非阻塞状态信息 | M4-M7 | catch-all 路径可能只显示通用信息 |
| GUI-014 | GUI | Theme and layout persistence | Wired into application | Requires verification | Not started | `ThemeController` 使用 `QSettings`；`LayoutManager` 使用 JSON 和 `QStandardPaths::AppConfigLocation` | 重启持久化、损坏文件和 macOS 路径 | M4 | 默认布局会在仓库外平台配置目录创建文件，需与 workspace policy 协调 |
| GUI-015 | GUI | Clean exit and resource release | Implemented in source | Requires verification | Not started | `MainWindow::closeEvent()`、`CameraController::teardown()`、controller destructors | live、RAW、export、recording、algo window 并发退出 | M4-M7 | 潜在风险，需要运行时验证 |

## 5. RAW and file playback

| ID | Feature area | Feature | Linux source status | Linux runtime status | macOS status | Evidence | Required verification | Target milestone | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| RAW-001 | RAW | File picker extensions | Wired into application | Requires verification | Not started | `MainWindow::on_open_file()` filter：`*.raw *.hdf5 *.h5 *.dat` | 每种格式分别打开 | M5 | Picker 中出现不代表 decoder/plugin 可用 |
| RAW-002 | RAW | Open file source | Wired into application | Requires verification | Not started | `PlaybackController::open_file()` → `CameraController::connect_file()` → `Metavision::Camera::from_file` | 成功、损坏文件、无权限和缺 plugin | M5 | 使用 `FileConfigHints::real_time_playback(false)` |
| RAW-003 | RAW | Geometry and camera metadata | Wired into application | Requires verification | Not started | `CameraController::fetch_sensor_info()`；`FileConverter::info()` | RAW/HDF5/DAT metadata 对比 | M5 | 缺失 facility 时字段可能保持默认值 |
| RAW-004 | RAW | Duration | Wired into application | Requires verification | Not started | `PlaybackController::query_duration()` 与 `FileFrameGenerator` last timestamp | 有/无 OSC、空文件、长文件 | M5 | 两个来源取较大值 |
| RAW-005 | RAW | Event buffering | Implemented in source | Requires verification | Not started | `FileFrameGenerator::add_events()` 将全部 events 加入 `events_` | 小/大文件内存与加载时间 | M5 | 全量内存缓冲可能产生明显占用；潜在风险，需要运行时验证 |
| RAW-006 | RAW | Playback | Wired into application | Requires verification | Not started | `PlaybackController::play()` → `FramePipeline::play_file()` | 真实速度、慢放、快进 | M5 | 速率由 fps 与 accumulation window 计算 |
| RAW-007 | RAW | Pause and resume | Wired into application | Requires verification | Not started | `PlaybackController::pause()`、`toggle_play_pause()` | 暂停位置、恢复、重复操作 | M5 | 状态同步是潜在风险，需要运行时验证 |
| RAW-008 | RAW | Step | Wired into application | Requires verification | Not started | `PlaybackControls` 以一个 accumulation window 调用 `seek()` | EOF 前后和不同 window | M5 | step 会先 pause 再同步 render |
| RAW-009 | RAW | Seek and timeline | Wired into application | Requires verification | Not started | slider → `PlaybackController::seek()` → `FileFrameGenerator::seek()` | 前向、后向、边界和 EOF 后 seek | M5 | seek 使用内存 buffer，不使用 OSC seek |
| RAW-010 | RAW | Loop | Wired into application | Requires verification | Not started | `set_loop()`、`FileFrameGenerator::looped`、MainWindow reset algorithms | 多轮 loop、算法状态和 XYT 清理 | M5/M7 | 潜在风险，需要运行时验证 |
| RAW-011 | RAW | Playback speed | Wired into application | Requires verification | Not started | `PlaybackControls` time-window/fps/multiplier；`PlaybackController::set_multiplier()` | 极慢、1x、快进和上限 | M5 | 算法输入 timestamp 会按 playback rate 缩放 |
| RAW-012 | RAW | EOF handling | Implemented in source | Requires verification | Not started | `FileFrameGenerator::eof_reached` → `PlaybackController::on_file_eof()` | 正常 EOF、decode error、loop EOF | M5 | file runtime error 与 EOF 的区分是潜在风险，需要运行时验证 |
| RAW-013 | RAW | Filter-chain processing | Wired into application | Requires verification | Not started | `FileFrameGenerator::render_frame()` 对 window events 应用 `FilterChain` | 每个 filter、运行中切换和坐标变化 | M5/M7 | 与 live 路径结果一致性需对比 |
| RAW-014 | RAW | Algorithm processing | Wired into application | Requires verification | Not started | `events_window_ready` → `MainWindow::on_events_window_ready()` → enabled `AlgoInstance::push_events()` | 每个实际算法的 RAW smoke test | M5/M7 | 运行能力不由静态接线保证 |
| RAW-015 | RAW | Algorithm reset on seek/loop/source switch | Implemented in source | Requires verification | Not started | `file_seeked`、`file_looped` 和 `camera connected` handlers 调用 `AlgoInstance::reset()` | stateful 算法回放连续性 | M5/M7 | 潜在风险，需要运行时验证 |
| RAW-016 | RAW | Switch between file and live source | Implemented in source | Requires verification | Not started | connect paths 先 `teardown()`；MainWindow 切换前停止 recorder | live→RAW→live、不同 geometry 和失败回滚 | M5/M6 | 生命周期和 UI 状态是潜在风险，需要运行时验证 |
| RAW-017 | RAW | File-to-file switch | Implemented in source | Requires verification | Not started | 再次调用 <code>PlaybackController::open_file()</code> 会经 <code>CameraController::connect_file()</code> teardown 旧 source | 播放中打开另一文件、不同 geometry、失败回滚 | M5 | playback state 同步是潜在风险，需要运行时验证 |
| RAW-018 | RAW | Disconnect and reopen file | Implemented in source | Requires verification | Not started | <code>CameraController::disconnect()</code> 与后续 <code>PlaybackController::open_file()</code> 路径均存在 | disconnect 后重开、play state、export source 和 UI 清理 | M5 | 潜在风险，需要运行时验证 |

## 6. Live camera

| ID | Feature area | Feature | Linux source status | Linux runtime status | macOS status | Evidence | Required verification | Target milestone | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| CAM-001 | Camera | Device enumeration | Wired into application | Requires verification | Not started | `CameraController::list_online_sources()`、`DevicesPanel` refresh | USB、embedded、remote、无设备 | M6 | enumeration 异常返回空列表 |
| CAM-002 | Camera | Open first local device | Wired into application | Requires verification | Not started | `connect_first_available()` 使用 `Metavision::Camera::from_serial("")` | 多设备、无设备和不同 vendor | M6 | 并非调用 `from_first_available()` |
| CAM-003 | Camera | Serial selection | Wired into application | Requires verification | Not started | `DevicesPanel::connect_serial_requested` → `CameraController::connect_serial()` | 正确 serial、无效 serial、重选 | M6 | 真实设备验证必需 |
| CAM-004 | Camera | Live event stream | Wired into application | Requires verification | Not started | `CameraController::start()`；CD callback → `FramePipeline` 与 statistics | 持续事件、空流、高事件率 | M6 | callback 运行于 SDK thread |
| CAM-005 | Camera | Device information | Wired into application | Requires verification | Not started | `fetch_sensor_info()` 读取 geometry、serial、integrator、plugin、encoding、firmware、generation | 多设备字段完整性 | M6 | facility/SDK exception 被静默降级为空字段 |
| CAM-006 | Camera facility | Bias access and changes | Wired into application | Requires verification | Not started | `biases_facility()`、`BiasesPanel` 读写 `I_LL_Biases` | 枚举、范围、修改、保存/加载 | M6 | facility 可用性依设备；潜在风险，需要运行时验证 |
| CAM-007 | Camera facility | ROI/RONI access and changes | Wired into application | Requires verification | Not started | `roi_facility()`、`RoiPanel`、display ROI drag | enable、window、RONI、边界 | M6 | facility 可用性依设备；潜在风险，需要运行时验证 |
| CAM-008 | Camera facility | Anti-flicker | Wired into application | Requires verification | Not started | `anti_flicker_facility()`、`EspPanel` | enable、frequency band、unsupported device | M6 | 潜在风险，需要运行时验证 |
| CAM-009 | Camera facility | Trail filter | Wired into application | Requires verification | Not started | `trail_filter_facility()`、`EspPanel` | enable、type、threshold | M6 | 潜在风险，需要运行时验证 |
| CAM-010 | Camera facility | ERC | Wired into application | Requires verification | Not started | `erc_facility()`、`EspPanel` | enable、target rate、device limits | M6 | 潜在风险，需要运行时验证 |
| CAM-011 | Camera facility | Trigger input | Wired into application | Requires verification | Not started | `trigger_in_facility()`、`TriggerPanel` | channels、enable、trigger events | M6 | facility 接线效果是潜在风险，需要运行时验证 |
| CAM-012 | Camera facility | Trigger output | Wired into application | Requires verification | Not started | `trigger_out_facility()`、`TriggerPanel` | enable、period、duty cycle | M6 | facility 接线效果是潜在风险，需要运行时验证 |
| CAM-013 | Camera | Sensor self-test | Wired into application | Requires verification | Not started | Devices panel action → `on_open_algo_window("sensor_self_test")` → `SensorSelfTestBackend` | live sensor coverage、heatmap、report、shutdown | M6/M7 | 源码未把入口限制为某一特定传感器型号 |
| CAM-014 | Camera | Manual disconnect | Wired into application | Requires verification | Not started | `MainWindow::on_disconnect()` → `CameraController::disconnect()` | streaming、recording、algorithm active 时断开 | M6 | 需确认 UI 和 facilities 全部清理 |
| CAM-015 | Camera | Runtime disconnect/error handling | Implemented in source | Requires verification | Not started | `add_runtime_error_callback()` 对 live error 停流并发出 error/stopped | 拔线、firmware error、callback exception | M6 | 潜在风险，需要运行时验证 |
| CAM-016 | Camera | Manual reconnect | Implemented in source | Requires verification | Not started | 用户可再次调用 connect first/serial；连接前 `teardown()` | 断开后同设备及其他设备重连 | M6 | 未发现自动重连策略；手动重连需验证 |
| CAM-017 | Camera | Automatic reconnect | Unknown | Unknown | Not started | 当前非 OpenEB 源码中未找到 retry/backoff 自动重连路径 | 先确认产品需求，再做断线场景测试 | M6 | 不据此断言自动重连一定不可用 |
| CAM-018 | Camera | Clean shutdown | Implemented in source | Requires verification | Not started | `CameraController::teardown()` 先移除 callbacks、停止 camera，再停止 pipeline | 正常退出、异常断开和反复重连 | M6 | 潜在风险，需要运行时验证 |
| CAM-019 | Camera | Live RAW recording | Wired into application | Requires verification | Not started | `RecorderController` 使用 `I_EventsStream::log_raw_data/get_latest_raw_data/stop_log_raw_data` | start、flush、stop、错误、文件可回放 | M6/M7 | 仅 live source；可能生成大文件 |

## 7. Algorithms

所有算法行均来自当前 HEAD 的真实 `AlgoBridge::registry_` 写入点。`AlgorithmsPanel` 只显示 `source == "self"` 的 30 个自研项，并通过 `AlgoBridge::find_or_create()` → `AlgoInstance` 接入 live/RAW 数据流。OpenEB 项中只有 `roi_filter`、`polarity_filter`、`polarity_invert`、`flip_x`、`flip_y`、`rotate`、`transpose`、`rescale` 由 `PreprocessingPanel` 接入 `FilterChain`；其余 22 个 OpenEB registry 项虽有注册信息，部分也有 backend class，但当前未发现普通 GUI 入口。逐行的运行验证必须覆盖入口、启停、参数、live、RAW、显示模式、reset 和资源释放。

### 7.1 OpenEB filters

| ID | Feature area | Feature | Linux source status | Linux runtime status | macOS status | Evidence | Required verification | Target milestone | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ALG-OEB-F01 | OpenEB filter | `roi_filter` | Wired into application | Requires verification | Not started | registry + `FilterChain::RoiFilterStage` | 参数、relative coordinates、live/RAW | M7 | FilterChain stage |
| ALG-OEB-F02 | OpenEB filter | `roi_mask` | Implemented in source | Requires verification | Not started | registry + `RoiMaskBackend` | 建立 GUI/调用入口后验证 mask loading、尺寸、live/RAW | M7 | 有 backend，但当前未发现普通 GUI 入口 |
| ALG-OEB-F03 | OpenEB filter | `polarity_filter` | Wired into application | Requires verification | Not started | registry + `FilterChain::PolarityFilterStage` | ON/OFF polarity、live/RAW | M7 | FilterChain stage |
| ALG-OEB-F04 | OpenEB filter | `polarity_invert` | Wired into application | Requires verification | Not started | registry + `FilterChain::PolarityInvertStage` | polarity inversion | M7 | FilterChain stage |
| ALG-OEB-F05 | OpenEB filter | `flip_x` | Wired into application | Requires verification | Not started | registry + `FilterChain::FlipXStage` | geometry、display、algorithm parity | M7 | FilterChain stage |
| ALG-OEB-F06 | OpenEB filter | `flip_y` | Wired into application | Requires verification | Not started | registry + `FilterChain::FlipYStage` | geometry、display、algorithm parity | M7 | FilterChain stage |
| ALG-OEB-F07 | OpenEB filter | `rotate` | Wired into application | Requires verification | Not started | registry + `FilterChain::RotateStage` | 0/90/180/270、非方形 sensor | M7 | 坐标裁剪是潜在风险，需要运行时验证 |
| ALG-OEB-F08 | OpenEB filter | `transpose` | Wired into application | Requires verification | Not started | registry + `FilterChain::TransposeStage` | geometry、bounds、live/RAW | M7 | FilterChain stage |
| ALG-OEB-F09 | OpenEB filter | `rescale` | Wired into application | Requires verification | Not started | registry + `FilterChain::RescaleStage` | scale range、geometry 和 downstream | M7 | FilterChain stage |
| ALG-OEB-F10 | OpenEB filter | `adaptive_rate_split` | Implemented in source | Requires verification | Not started | registry + `AdaptiveRateSplitBackend` | 建立 GUI/调用入口后验证 threshold、downsampling、event output | M7 | 有 backend，但当前未发现普通 GUI 入口 |

### 7.2 OpenEB frame modes

| ID | Feature area | Feature | Linux source status | Linux runtime status | macOS status | Evidence | Required verification | Target milestone | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ALG-OEB-R01 | OpenEB frame | `frame_integration` | Implemented in source | Requires verification | Not started | registry + `FrameIntegrationBackend` | 建立入口后验证 decay、Replace display、live/RAW | M7 | 有 backend，但当前未发现普通 GUI 入口 |
| ALG-OEB-R02 | OpenEB frame | `frame_diff` | Implemented in source | Requires verification | Not started | registry + `FrameDiffBackend` | 建立入口后验证 bit size、rollover、live/RAW | M7 | 有 backend，但当前未发现普通 GUI 入口 |
| ALG-OEB-R03 | OpenEB frame | `frame_histogram` | Implemented in source | Requires verification | Not started | registry + `FrameHistoBackend` | 建立入口后验证 channel bits、packed、color mapping | M7 | 有 backend，但当前未发现普通 GUI 入口 |
| ALG-OEB-R04 | OpenEB frame | `frame_time_decay` | Implemented in source | Requires verification | Not started | registry + `FrameTimeDecayBackend` | 建立入口后验证 decay、palette、reset | M7 | 有 backend，但当前未发现普通 GUI 入口 |
| ALG-OEB-R05 | OpenEB frame | `frame_contrast_map` | Implemented in source | Requires verification | Not started | registry + `FrameContrastMapBackend` | 建立入口后验证 ON/OFF contrast、reset | M7 | 有 backend，但当前未发现普通 GUI 入口 |
| ALG-OEB-R06 | OpenEB frame | `frame_periodic` | Implemented in source | Requires verification | Not started | registry + `FramePeriodicBackend` | 建立入口后验证 accumulation、fps、live/RAW | M7 | callback/timing 是潜在风险，需要运行时验证 |
| ALG-OEB-R07 | OpenEB frame | `frame_on_demand` | Implemented in source | Requires verification | Not started | registry + `FrameOnDemandBackend` | 建立入口后验证 on-demand generation、window | M7 | pull/result timing 需验证 |

### 7.3 OpenEB preprocessors

| ID | Feature area | Feature | Linux source status | Linux runtime status | macOS status | Evidence | Required verification | Target milestone | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ALG-OEB-P01 | OpenEB preprocessor | `preproc_diff` | Implemented in source | Requires verification | Not started | registry + `PreprocDiffBackend` | 建立入口后验证 tensor shape、normalization、frame | M7 | 有 backend，但当前未发现普通 GUI 入口 |
| ALG-OEB-P02 | OpenEB preprocessor | `preproc_histo` | Implemented in source | Requires verification | Not started | registry + `PreprocHistoBackend` | 建立入口后验证 CHW/HWC、polarity channels | M7 | 有 backend，但当前未发现普通 GUI 入口 |
| ALG-OEB-P03 | OpenEB preprocessor | `preproc_hw_diff` | Implemented in source | Requires verification | Not started | registry entry；factory 和 FilterChain 中未找到同名处理路径 | 确认 intended backend、hardware input 和 UI output | M7 | 当前只有注册与通用 UI；不把测试/注释视为实现 |
| ALG-OEB-P04 | OpenEB preprocessor | `preproc_hw_histo` | Implemented in source | Requires verification | Not started | registry entry；factory 和 FilterChain 中未找到同名处理路径 | 确认 intended backend、hardware input 和 UI output | M7 | 当前只有注册与通用 UI |
| ALG-OEB-P05 | OpenEB preprocessor | `preproc_time_surface` | Implemented in source | Requires verification | Not started | registry + `PreprocTimeSurfaceBackend` | 建立入口后验证 1/2 channel、decay image、reset | M7 | 有 backend，但当前未发现普通 GUI 入口 |
| ALG-OEB-P06 | OpenEB preprocessor | `preproc_event_cube` | Implemented in source | Requires verification | Not started | registry + `PreprocEventCubeBackend` | 建立入口后验证 bins、polarity、projection、memory | M7 | tensor 内存占用是潜在风险，需要运行时验证 |
| ALG-OEB-P07 | OpenEB preprocessor | `preproc_factory` | Implemented in source | Requires verification | Not started | registry + `PreprocFactoryBackend` | 建立入口后验证 JSON config parsing 和实际 processor creation | M7 | backend 明确标记为 stub/pass-through；无普通 GUI 入口 |

### 7.4 OpenEB utilities

| ID | Feature area | Feature | Linux source status | Linux runtime status | macOS status | Evidence | Required verification | Target milestone | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ALG-OEB-U01 | OpenEB utility | `util_rate_estimator` | Implemented in source | Requires verification | Not started | registry + `UtilRateEstimatorBackend` | 建立入口后与 Statistics panel 数值对比 | M7 | status-only stub；无普通 GUI 入口 |
| ALG-OEB-U02 | OpenEB utility | `util_frame_composer` | Implemented in source | Requires verification | Not started | registry + `UtilFrameComposerBackend` | 建立入口后验证 subimage composition API 和输出 | M7 | passive container，未实现 composition workflow；无普通 GUI 入口 |
| ALG-OEB-U03 | OpenEB utility | `util_rolling_buffer` | Implemented in source | Requires verification | Not started | registry + `UtilRollingBufferBackend` | 建立入口后验证 N_EVENTS、N_US、capacity、reset | M7 | 实际使用 `RollingEventBuffer`，但无普通 GUI 入口 |
| ALG-OEB-U04 | OpenEB utility | `util_video_writer` | Implemented in source | Requires verification | Not started | registry + `UtilVideoWriterBackend` | 明确预期与 ExporterController 的关系 | M7 | stub，提示使用 Export menu；无普通 GUI 入口 |
| ALG-OEB-U05 | OpenEB utility | `util_data_synchronizer` | Implemented in source | Requires verification | Not started | registry + `UtilDataSynchronizerBackend` | 建立入口后验证 trigger data、period 和多流同步 | M7 | stub/pass-through；无普通 GUI 入口 |
| ALG-OEB-U06 | OpenEB utility | `util_timing_profiler` | Implemented in source | Requires verification | Not started | registry + `UtilTimingProfilerBackend` | 建立入口后验证 timing measurements 和展示 | M7 | status-only stub；无普通 GUI 入口 |

### 7.5 Self-developed CV

| ID | Feature area | Feature | Linux source status | Linux runtime status | macOS status | Evidence | Required verification | Target milestone | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ALG-SELF-CV01 | Self CV | `hot_pixel_filter` | Wired into application | Requires verification | Not started | registry + `HotPixelFilterBackend` + `algo/cv/hot_pixel_filter.h` | learning、FPN、live/RAW、filtered output | M7 | Passive filtered events |
| ALG-SELF-CV02 | Self CV | `orientation_filter` | Wired into application | Requires verification | Not started | registry + `OrientationFilterBackend` | modes/thresholds、overlay、live/RAW | M7 | 参数和方向准确性需验证 |
| ALG-SELF-CV03 | Self CV | `direction_selective` | Wired into application | Requires verification | Not started | registry + `DirectionSelectiveBackend` | direction vectors、global mode、reset | M7 | Overlay |
| ALG-SELF-CV04 | Self CV | `sparse_optical_flow` | Wired into application | Requires verification | Not started | registry + `SparseOpticalFlowBackend` | 4 modes、vector scale、live/RAW | M7 | Overlay |
| ALG-SELF-CV05 | Self CV | `blob_detector` | Wired into application | Requires verification | Not started | registry + `BlobDetectorBackend` | threshold、learning、bbox | M7 | Overlay |
| ALG-SELF-CV06 | Self CV | `object_tracker` | Wired into application | Requires verification | Not started | registry + `ObjectTrackerBackend` | 4 modes、IDs、lost age、trajectory | M7 | Stateful overlay |
| ALG-SELF-CV07 | Self CV | `corner_detector` | Wired into application | Requires verification | Not started | registry + `CornerDetectorBackend` | 每个 mode、score、GUI label 对应关系 | M7 | registry 标签与 backend enum 的一致性需专项核对 |
| ALG-SELF-CV08 | Self CV | `line_segment` | Wired into application | Requires verification | Not started | registry + `LineSegmentBackend` | length、gap、line coordinates | M7 | Overlay |
| ALG-SELF-CV09 | Self CV | `hough_line` | Wired into application | Requires verification | Not started | registry + `HoughLineBackend` | bins、decay、threshold、CPU | M7 | 潜在性能风险，需要运行时验证 |
| ALG-SELF-CV10 | Self CV | `hough_circle` | Wired into application | Requires verification | Not started | registry + `HoughCircleBackend` | radius、threshold、decay、CPU | M7 | 潜在性能风险，需要运行时验证 |
| ALG-SELF-CV11 | Self CV | `orientation_cluster` | Wired into application | Requires verification | Not started | registry + `OrientationClusterBackend` | orientation、RF、history、clusters | M7 | Overlay |
| ALG-SELF-CV12 | Self CV | `cluster_lif` | Wired into application | Requires verification | Not started | registry + `ClusterLifBackend` | tau、threshold、receptive field、spikes | M7 | Overlay |
| ALG-SELF-CV13 | Self CV | `background_mask` | Wired into application | Requires verification | Not started | registry + `BackgroundMaskBackend` | learning、threshold、erosion、Replace | M7 | Stateful mask |
| ALG-SELF-CV14 | Self CV | `perspective_undistort` | Wired into application | Requires verification | Not started | registry + `PerspectiveUndistortBackend` | calibration/LUT input、zoom、geometry | M7 | 未发现 GUI calibration data 注入路径；潜在风险，需要运行时验证 |
| ALG-SELF-CV15 | Self CV | `trigger_synced` | Wired into application | Requires verification | Not started | registry + `TriggerSyncedBackend` | external trigger feed、channel、window | M7 | 未发现 trigger events 注入 backend 的明确路径；潜在风险，需要运行时验证 |
| ALG-SELF-CV16 | Self CV | `bandpass_filter` | Wired into application | Requires verification | Not started | registry + `BandpassFilterBackend` | cutoff、frequency response、overlay | M7 | 需合成信号和真实流对比 |
| ALG-SELF-CV17 | Self CV | `optical_gyro` | Wired into application | Requires verification | Not started | registry + `OpticalGyroBackend` | translation、rotation、stabilize | M7 | Overlay；准确性需基准数据 |
| ALG-SELF-CV18 | Self CV | `ultra_slow_motion` | Wired into application | Requires verification | Not started | registry + `UltraSlowMotionBackend` | dilation factor、timestamp output | M7 | Passive；与 playback rate 交互需验证 |
| ALG-SELF-CV19 | Self CV | `xyt_visualizer` | Wired into application | Requires verification | Not started | registry + `XYTVisualizerBackend` + `SpaceTimeDisplay` | point limit、time window、live/RAW | M7 | 独立 3D window |
| ALG-SELF-CV20 | Self CV | `overlay` | Wired into application | Requires verification | Not started | registry + `OverlayBackend` | event overlay 和 display strategy | M7 | 需确认输出对用户可见 |
| ALG-SELF-CV21 | Self CV | `time_surface` | Wired into application | Requires verification | Not started | registry + `TimeSurfaceBackend` | decay、palette、channels、reset | M7 | Standalone frame |

### 7.6 Self-developed analytics and calibration

| ID | Feature area | Feature | Linux source status | Linux runtime status | macOS status | Evidence | Required verification | Target milestone | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ALG-SELF-AN01 | Self analytics | `active_marker` | Wired into application | Requires verification | Not started | registry + `ActiveMarkerBackend` | marker detection、tracking、live/RAW | M7 | Overlay |
| ALG-SELF-AN02 | Self analytics | `event_to_video` | Wired into application | Requires verification | Not started | registry + `EventToVideoBackend` + `algo/analytics/event_to_video.h` | 三模式、参数、frame output、reset | M7 | BardowVariational、InteractingMaps、E2VID |
| ALG-SELF-AN03 | Self analytics | `flow_statistics` | Wired into application | Requires verification | Not started | registry + `FlowStatisticsBackend` | ground truth 输入、统计值、显示 | M7 | 当前未发现 GUI 的 external ground-truth 输入路径 |
| ALG-SELF-AN04 | Self analytics | `isi_analyzer` | Wired into application | Requires verification | Not started | registry + `ISIAnalyzerBackend` | per-pixel、histogram、window | M7 | Standalone |
| ALG-SELF-AN05 | Self analytics | `particle_counter` | Wired into application | Requires verification | Not started | registry + `ParticleCounterBackend` | line crossing、min area、count reset | M7 | Overlay |
| ALG-SELF-AN06 | Self analytics | `auto_bias` | Wired into application | Requires verification | Not started | registry + `AutoBiasBackend` | target rate、建议值、是否写入 device | M7 | 未发现 backend 调用 `CameraController` bias facility 的路径 |
| ALG-SELF-AN07 | Self analytics | `freq_detector` | Wired into application | Requires verification | Not started | registry + `FreqDetectorBackend` | frequency estimate、min events、update interval | M7 | Standalone |
| ALG-SELF-AN08 | Self analytics | `sensor_self_test` | Wired into application | Requires verification | Not started | manual registry entry + `SensorSelfTestBackend` + Devices panel action | 全传感器、heatmap、bad pixels、报告 | M6/M7 | 不附加 registry ROI/preprocessing 参数 |
| ALG-SELF-CAL01 | Self calibration | `intrinsic_calibration` | Wired into application | Requires verification | Not started | registry entry；Tools → `CalibrationWizard` → `algo/calibration/intrinsic.cpp` | board types、capture、solve、save YAML | M7 | generic backend factory 未注册同名 backend；实际流程由 wizard 驱动 |

## 8. Models and inference

| ID | Feature area | Feature | Linux source status | Linux runtime status | macOS status | Evidence | Required verification | Target milestone | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| MOD-001 | Model | E2VID heuristic fallback | Wired into application | Requires verification | Not started | `E2VIDInference` 在无 ONNX Runtime 或 load/infer failure 时使用 heuristic；`event_to_video` mode 2 | 无模型、坏模型、frame quality 和 reset | M7 | fallback 不代表神经网络推理成功 |
| MOD-002 | Model | Plain ONNX E2VID inference | Implemented in source | Requires verification | Not started | `E2VIDInference::load_model/infer_onnx()` 创建 `Ort::Session`、CPU memory | arm64 ONNX Runtime、input shape、output frame | M7 | 只发现 CPU path，未配置 CoreML/CUDA provider |
| MOD-003 | Model | Recurrent ONNX E2VID state | Implemented in source | Requires verification | Not started | `E2VIDInference` 管理 `prev_states_`、state buffers 和 recurrent I/O | recurrent model、state reset、seek/loop | M7 | 模型 shape 和 state name 需真实模型验证 |
| MOD-004 | Model tooling | RPG E2VID model conversion | Implemented in source | Requires verification | Not started | tracked `models/convert_to_onnx.py`；输入 `.pth.tar`，输出 `.onnx` | 仓库内 venv、CPU conversion、`onnx.checker` | M7 | 仓库未跟踪 `.onnx` 或 `.pth`；默认 `models/e2vid_lightweight.onnx` 当前不存在 |

## 9. Export and generated outputs

| ID | Feature area | Feature | Linux source status | Linux runtime status | macOS status | Evidence | Required verification | Target milestone | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| EXP-001 | Recording | Live RAW `.raw` | Wired into application | Requires verification | Not started | `RecorderController` + `MainWindow::on_record_start()` | start/stop、回放、file growth、disk full | M6/M7 | 用户选择路径；可快速增长为大文件 |
| EXP-002 | Recording | Companion bias `.bias` | Wired into application | Requires verification | Not started | `on_record_start()` 以 RAW basename 调用 biases `save_to_file()` | 有/无 bias facility、命名、可加载性 | M6/M7 | best-effort，与 RAW 同目录 |
| EXP-003 | Conversion/export | HDF5 `.h5` | Wired into application | Requires verification | Not started | `FileConverter::run_convert(HDF5)` 与 `ExporterController::run_hdf5()` | plugin、metadata、cancel、disk full、重开 | M7 | 依赖 HDF5 event writer/plugin |
| EXP-004 | Conversion | CSV `.csv` | Wired into application | Requires verification | Not started | `FileConverter::run_convert(CSV)` 写 `t,x,y,p` | 数据行、时间戳、cancel、大文件 | M7 | 文本输出可能显著放大磁盘占用 |
| EXP-005 | File tools | RAW clip `.raw` | Wired into application | Requires verification | Not started | `FileConverter::run_cut()` + `RAWEvt2EventFileWriter` | seek/no-seek、边界、metadata、回放 | M7 | 输出固定 EVT2 writer |
| EXP-006 | Video export | AVI `.avi` | Wired into application | Requires verification | Not started | `ExporterController::run_avi()` + `CvVideoRecorder` | H264/MJPG、color/gray、fps、codec availability | M7 | codec 可用性是潜在风险，需要运行时验证 |
| EXP-007 | Calibration | YAML `.yml/.yaml` | Wired into application | Requires verification | Not started | `CalibrationWizard::on_intrinsic_save()` | schema、数值、重载/消费方 | M7 | intrinsic result export |
| EXP-008 | Configuration | Camera/config JSON | Wired into application | Requires verification | Not started | `ConfigManager::save_to_file/load_from_file()` | round-trip、sensor mismatch、损坏 JSON | M7 | 用户选择路径 |
| EXP-009 | Configuration | Algorithm params JSON | Wired into application | Requires verification | Not started | File menu → `save_algo_params_to_file/load_algo_params_from_file` | 全部 60 项、enabled state、未知 key | M7 | lazy instance cache 需运行验证 |
| EXP-010 | Configuration | Layout JSON | Wired into application | Requires verification | Not started | `LayoutManager::save/load()`；View menu | round-trip、跨屏幕、损坏 JSON | M4 | 默认 layout 另写平台 config 目录 |
| EXP-011 | Annotation | Target labels JSON | Implemented in source | Requires verification | Not started | `TargetLabeler::save_to_json/load_from_json()` | GUI 入口、bbox、class、timestamp round-trip | M7 | 需确认主应用中完整入口与工作流 |
| EXP-012 | Algorithm output | General algorithm-result export | Unknown | Unknown | Not started | 未发现将通用 `AlgoResult` 写入文件的统一 controller | 明确需求，并逐算法验证导出 | M7 | 不与 AVI/HDF5 原始事件导出混同 |
| EXP-013 | Media export | MP4 or screenshot | Unknown | Unknown | Not started | 当前非 OpenEB 源码中未找到 MP4 或 screenshot export 路径 | 确认 Linux baseline 是否要求 | M7 | 不根据文案或图标推断支持 |

## 10. Configuration, state and logging

| ID | Feature area | Feature | Linux source status | Linux runtime status | macOS status | Evidence | Required verification | Target milestone | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| CFG-001 | Configuration | SDK camera save/load | Wired into application | Requires verification | Not started | File menu → `CameraController::save_config/load_config()` → `Camera::save/load` | live device round-trip、错误和兼容性 | M6 | 仅 live camera action enabled |
| CFG-002 | Configuration | Bias save/load | Wired into application | Requires verification | Not started | File menu → `BiasesPanel::save_to_file/load_from_file()` | 各设备、范围、错误和 companion bias | M6 | 依赖 `I_LL_Biases` |
| CFG-003 | Configuration | Presets | Wired into application | Requires verification | Not started | ROI panel → `MainWindow::on_apply_preset()` → `ConfigManager::apply_preset()` | 所有 preset 和 unsupported facilities | M6 | refreshes Bias/ROI/ESP/Trigger panels |
| CFG-004 | Persistent state | Theme | Wired into application | Requires verification | Not started | `ThemeController::load_settings/save_settings()` 使用 `QSettings` | restart、system theme、invalid values | M4 | 写入平台默认 settings 存储 |
| CFG-005 | Persistent state | Recent files | Wired into application | Requires verification | Not started | `MainWindow::build_recent_files_menu/add_recent_file()` 使用 `QSettings` | restart、missing path、10-entry cap | M4/M5 | 会保存用户文件绝对路径 |
| CFG-006 | Persistent state | Sidebar active group | Wired into application | Requires verification | Not started | `SettingsPanel` 使用 `QSettings("sidebar/active_group")` | restart 和无效 index | M4 | 写入平台默认 settings 存储 |
| CFG-007 | Persistent state | Default layout path | Wired into application | Requires verification | Not started | `LayoutManager::default_path()` 使用 `QStandardPaths::AppConfigLocation` 并 `mkpath` | Linux/macOS 实际路径、权限和 workspace policy | M4 | 项目主动写入仓库外配置目录，与新 policy 存在待协调项 |
| CFG-008 | Logging | Runtime logs | Documented only | Requires verification | Not started | `run.sh` 设置 `QT_LOGGING_RULES`；UI 主要通过 status bar、signals 和 dialogs 报告 | 确认 stdout/stderr、日志文件、debug 模式 | M4 | 未发现项目自有持久日志文件 controller |
| CFG-009 | Diagnostics | Performance/statistics | Wired into application | Requires verification | Not started | `StatisticsController`、`PerformanceProfiler`、Information/Statistics panels | event rate、ON/OFF、fps、drop/overload telemetry | M4/M6/M7 | 数值准确性需与已知输入对比 |

## 11. Packaging and CI

| ID | Feature area | Feature | Linux source status | Linux runtime status | macOS status | Evidence | Required verification | Target milestone | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| PKG-001 | Packaging | macOS `.app` bundle | Not applicable | Not applicable | Not started | 当前 CMake 只创建普通 `add_executable(gui_for_openeb)` | 配置 MACOSX_BUNDLE 并启动 bundle | M8 | 尚未实现 |
| PKG-002 | Packaging | macOS RPATH | Not applicable | Not applicable | In progress | 三个要求的 CLI、SDK/HAL dylib、Prophesee plugin/PSEE layer 和 HDF5 ECF plugin 已验证相对 `@executable_path`/`@loader_path` RPATH | 覆盖其余 10 个 sample executable、EBplus GUI executable、ONNX linkage 和 `.app` bundle launch | M3/M8 | OpenEB 最小三 CLI 范围已通过；其他 sample、GUI 和 bundle 尚未覆盖 |
| PKG-003 | Packaging | Dependency bundling | Unknown | Unknown | Not started | 当前未找到 Qt/OpenEB/OpenCV/ONNX bundle staging 逻辑 | 检查 `.app/Contents/Frameworks` 与 plugins | M8 | 尚未实现 |
| PKG-004 | Packaging | Code signing | Not applicable | Not applicable | Not started | 当前未找到签名配置或脚本 | ad-hoc/Developer ID 签名验证 | M8 | 后续可选工作 |
| PKG-005 | Packaging | Notarization | Not applicable | Not applicable | Not started | 当前未找到 notarization 配置 | Apple notarization workflow | M8 | 后续可选工作 |
| PKG-006 | Packaging | DMG | Not applicable | Not applicable | Not started | 当前未找到 DMG 生成逻辑 | 安装、卸载和空间预算 | M8 | 仅 milestone 需要时生成 |
| PKG-007 | CI | Linux and Apple Silicon CI | Unknown | Unknown | Not started | 当前审计未发现覆盖本目标的 CI workflow | Linux regression、macOS arm64 configure/build/smoke | M8 | CI 不得破坏 Linux baseline |

## 12. 更新规则

- 每个 milestone 开始时更新目标行的 `macOS status`；只有实际开始实施才改为 `In progress`。
- 只有执行并记录了对应检查，才能把 runtime status 改为 `Verified`。编译成功不能替代 GUI、RAW、相机、算法或导出验证。
- 若实现存在但没有应用入口，使用 `Implemented in source`；确认已连接到 GUI/controller/data flow 后使用 `Wired into application`。
- 新增、删除或重命名 registry 项时，必须从 `AlgoBridge::registry_` 写入点重新统计并同步算法各行，不以本次 60 项为永久常量。
- 发现静态风险时记录证据和验证方法；除非源码能证明确定性事实，否则使用“潜在风险，需要运行时验证”。
