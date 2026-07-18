# Linux 功能基线清单

## 1. Purpose and scope

本文档记录 EBplus 当前 tracked source 所呈现的 Linux 功能范围、构建和运行入口、OpenEB 集成边界、平台专用假设及后续运行验证需求。它是后续 macOS 功能对齐工作的参考基线，不是运行验收报告。

- 本轮只进行了源码、CMake、脚本和文档的静态审计。
- 本轮未执行 CMake configure、编译、CTest、GUI 启动、RAW 回放、真实相机、算法、模型或导出。
- “代码存在”不等于“运行验证通过”；“已经连接到应用”也不保证具体硬件、数据、动态库或错误路径可用。
- 除非有可信既有记录并明确注明来源，本轮不新增 <code>Runtime-verified</code> 结论。
- 对静态分析不能证明必然错误的事项，统一表述为“潜在风险，需要运行时验证”，不视为已经复现的 Bug。

### 证据等级

| 等级 | 含义 |
| --- | --- |
| <code>Documented</code> | 只在 README 或其他文档中出现。 |
| <code>Implemented in source</code> | 当前 tracked source 中存在明确实现路径。 |
| <code>Wired into application</code> | 实现已连接到 GUI、controller、入口或数据流。 |
| <code>Runtime-verified</code> | 有实际运行证据；本轮不新增此等级。 |
| <code>Requires runtime verification</code> | 静态路径存在，但本轮没有实际运行。 |
| <code>Unknown</code> | 当前证据不足。 |
| <code>Conflicting evidence</code> | 文档、registry、backend、UI 或其他实现路径之间存在明确矛盾。 |

## 2. Audit metadata

| Item | Value |
| --- | --- |
| Branch | <code>docs/linux-baseline-inventory</code> |
| Commit | <code>d80642817d2581687aa0b2a55f171c0c1e84901a</code> |
| Audit date | 2026-07-18 |
| Repository state | 开始审计前 <code>main</code> clean 且与 <code>origin/main</code> 同步；本文件及相关文档修改保持本地未提交 |
| Upstream comparison | 本地 tracking ref 显示 <code>upstream/main...main = 0 7</code>；本轮未 fetch |
| Build performed | No |
| CMake configure performed | No |
| Runtime tests performed | No |
| Camera or RAW used | No |
| Preliminary algorithm expectation | 60，仅作为前期审计预期值 |
| Actual registry result at audited HEAD | 60 个唯一名称：OpenEB 30、self 30；无重复 |

算法数量来自 <code>AlgoBridge::AlgoBridge()</code> 调用的七个注册函数及最终写入 <code>AlgoBridge::registry_</code> 的唯一名称。统计结果为 59 个 <code>add({...})</code> 调用加手动注册的 <code>sensor_self_test</code>。测试名称、backend 类、文件名和注释均未计入。

## 3. Repository and build baseline

### 3.1 主要目录

| Path | Current role |
| --- | --- |
| <code>CMakeLists.txt</code> | 顶层项目、依赖发现、子目录和测试入口 |
| <code>run.sh</code> | 当前 Linux 推荐启动脚本 |
| <code>gui/</code> | Qt GUI、controller、panel、record/playback/export 和算法桥接 |
| <code>algo/</code> | 自研算法、公共数据结构、E2VID 和算法测试 |
| <code>models/</code> | 当前只跟踪 ONNX 转换脚本，不跟踪模型权重 |
| <code>openeb/</code> | 仓库自带 OpenEB 5.2.0 源码；本轮只审计 EBplus 集成边界 |
| <code>doc/</code> | 既有设计、编译和优化文档 |
| <code>docs/</code> | macOS 移植、隔离、工作区和本基线文档 |

### 3.2 CMake 和 targets

| Item | Static evidence |
| --- | --- |
| Minimum CMake | <code>CMakeLists.txt</code>：3.16 |
| Project | <code>GUI_for_openEB</code> 1.9.0 |
| C++ | C++17，required，extensions off |
| Default build type | 未指定时设为 Release |
| Compiler condition | GNU 15+ 全局加入 <code>-include;cstdint</code> |
| Qt | Qt6 REQUIRED：Widgets、OpenGL、OpenGLWidgets |
| MetavisionSDK | 5.2.0 REQUIRED：base、core、stream；未使用 EXACT |
| OpenCV | REQUIRED，未限制版本或组件 |
| Tests | 顶层无条件 <code>enable_testing()</code> 并加入 <code>algo/tests</code> |
| <code>gui_algo</code> | STATIC library；链接 Metavision base/core/stream 和 OpenCV |
| <code>gui_core</code> | STATIC library；链接 Qt、<code>gui_algo</code>，存在 target 时链接 <code>MetavisionSDK::hal</code> |
| <code>gui_for_openeb</code> | 普通 executable，不是 <code>MACOSX_BUNDLE</code> |
| Install rule | executable 安装到 <code>bin</code>；未显式指定 prefix 时 Unix CMake 通常默认为 <code>/usr/local</code> |
| Expected build-tree executable | 使用 <code>cmake -B build</code> 时为 <code>build/gui/gui_for_openeb</code> |
| In-source protection | 当前顶层 CMake 未禁止 in-source build |

顶层 CMake 通过 <code>find_package(MetavisionSDK ...)</code> 使用已安装或用户显式指定 prefix 中的 SDK，没有 <code>add_subdirectory(openeb)</code>。因此仓库内 OpenEB 源码不会随 EBplus 顶层配置自动构建或链接；实际来源取决于 CMake package 搜索环境。

### 3.3 Tests

- <code>gui/tests/CMakeLists.txt</code> 定义 5 个 test target：<code>test_theme_tokens</code>、<code>test_algo_bridge</code>、<code>test_config_manager</code>、<code>test_layout_manager</code>、<code>test_display_strategy</code>。
- <code>algo/tests/CMakeLists.txt</code> 定义 11 个 test/diagnostic target：<code>test_phase6_common</code>、<code>test_phase7_cv</code>、<code>test_phase8_10</code>、<code>test_raw_algos</code>、<code>test_raw_e2v</code>、<code>test_raw_playback</code>、<code>test_playback_e2v</code>、<code>test_file_frame_generator</code>、<code>test_filter_in_render</code>、<code>test_filter_integration</code>、<code>test_loop_flip</code>。
- 两套测试 CMake 均使用 <code>find_package(GTest REQUIRED)</code>。<code>GUI_BUILD_TESTS</code> 只控制 GUI tests，而顶层无条件加入 <code>algo/tests</code>，因此“GTest 可选”的既有文档与当前配置存在冲突。
- <code>algo/tests/sparklers.raw</code> 是 tracked RAW 测试资产；本轮未读取或运行它。

### 3.4 ONNX Runtime

<code>algo/CMakeLists.txt</code> 在 <code>third_party/onnxruntime</code>、<code>/usr</code>、<code>/usr/local</code> 和 <code>/opt/onnxruntime</code> 搜索 headers/library。找到时定义 <code>GUI_ALGO_HAS_ONNXRUNTIME</code>，否则 E2VID 使用 heuristic fallback。

当前配置包含 Linux 风格的 <code>libonnxruntime.so</code>、<code>$ORIGIN/../lib</code> 和若干系统路径假设。RPATH 属性设置在 STATIC <code>gui_algo</code> 上，最终 executable 是否获得可用 RPATH 是潜在风险，需要运行时动态链接检查。

## 4. Application entrypoints

| Entrypoint | Evidence | Level |
| --- | --- | --- |
| GUI executable | <code>gui/CMakeLists.txt</code>：<code>add_executable(gui_for_openeb ...)</code> | Implemented in source |
| <code>main()</code> | <code>gui/main.cpp::main()</code> 创建 <code>QApplication</code> 和 <code>gui::MainWindow</code> | Wired into application |
| Main window | <code>gui/main_window.cpp::MainWindow::MainWindow()</code> | Wired into application |
| Linux launcher | <code>run.sh</code> 解析仓库根目录并执行 <code>build/gui/gui_for_openeb</code> | Wired into application |
| Test/diagnostic programs | <code>gui/tests</code> 和 <code>algo/tests</code> 中的 targets | Implemented in source |

<code>run.sh</code> 使用 Bash、<code>set -euo pipefail</code>、<code>BASH_SOURCE</code> 和 <code>[[ ... ]]</code>。它设置或补充：

- <code>LD_LIBRARY_PATH=/usr/local/lib</code>
- <code>HDF5_PLUGIN_PATH=/usr/local/lib/hdf5/plugin</code>
- <code>MV_HAL_PLUGIN_PATH=/usr/local/lib/metavision/hal/plugins</code>
- Wayland 环境下默认 <code>QT_QPA_PLATFORM=xcb</code>
- 默认 <code>QSG_RHI_BACKEND=opengl</code>
- 默认 <code>QT_LOGGING_RULES=*.warning=false</code>

脚本本身未使用 <code>/proc</code>、<code>nproc</code>、<code>readlink -f</code>、<code>realpath</code> 或 <code>/tmp</code>；README 和编译文档中的构建命令使用 <code>nproc</code>。

## 5. Feature inventory

### 5.1 GUI

| Feature | Evidence and implementation path | Evidence level | Runtime verification |
| --- | --- | --- | --- |
| Main window | <code>main()</code> → <code>gui::MainWindow</code> | Wired into application | GUI launch、窗口和退出 |
| Menus | <code>MainWindow::build_menus()</code> 实际创建 File、View、Theme、Tools、Help | Wired into application | 每个 action、快捷键和 modal dialog |
| Central visualization | <code>EventDisplayWidget : QOpenGLWidget</code> 接收 <code>FramePipeline::frame_ready</code> | Wired into application | OpenGL 3.3、palette、resize、清理 |
| XYT visualization | <code>SpaceTimeDisplay : QOpenGLWidget</code> | Wired into application | live/RAW point cloud、时间轴和资源释放 |
| Settings sidebar | <code>SettingsPanel</code> 聚合 Devices、Information、Statistics、Display、Biases、ROI、ESP、Trigger、Preprocessing、Algorithms、File Tools | Wired into application | 每个 panel 状态和缺失 facility 降级 |
| Playback controls | <code>PlaybackControls</code> → <code>PlaybackController</code> | Wired into application | play/pause/step/seek/loop/rate |
| Algorithm windows | <code>MainWindow::on_open_algo_window()</code> → <code>AlgoWindow</code> | Wired into application | Standalone/Overlay/Replace 显示 |
| Status | status bar、<code>StatisticsController</code>、Information/Statistics panels | Wired into application | connection、rate、timestamp、recording |
| File dialogs | RAW/open、record、export、config、bias、layout、calibration | Wired into application | 路径、覆盖、取消和错误 |
| Theme | <code>ThemeController</code> + <code>QSettings</code> | Wired into application | restart 和系统主题 |
| Layout | <code>LayoutManager</code> JSON + <code>QStandardPaths::AppConfigLocation</code> | Wired into application | restart、跨屏幕、权限 |
| Error reporting | <code>QMessageBox</code>、status bar、controller signals | Wired into application | 每类失败路径 |
| Persistent log file | 当前 GUI 源码未找到项目自有文件日志 controller | Unknown | stdout/stderr、debug 和日志需求 |

### 5.2 Live camera

| Capability | Evidence | Level | Notes and required verification |
| --- | --- | --- | --- |
| Device enumeration | <code>CameraController::list_online_sources()</code> 调用 <code>Camera::list_online_sources()</code>，显示 EMBEDDED/USB/REMOTE/Other | Wired into application | 枚举异常返回空列表；需区分无设备和失败 |
| Open first local device | <code>connect_first_available()</code> 使用 <code>Camera::from_serial("")</code> | Wired into application | 多设备、无设备、不同 vendor |
| Open selected serial | <code>DevicesPanel</code> → <code>connect_serial()</code> → <code>Camera::from_serial(serial)</code> | Wired into application | 有效/无效 serial、重新选择 |
| Live stream | <code>setup_camera()</code> 注册 CD/status/error callbacks；CD events 进入 statistics 和 <code>FramePipeline</code> | Wired into application | 空流、高事件率、callback 错误 |
| Device information | <code>fetch_sensor_info()</code>：geometry、serial、integrator、plugin、encoding、firmware、generation | Wired into application | 多设备字段和异常降级 |
| Biases | <code>I_LL_Biases</code>、<code>BiasesPanel</code> | Wired into application | 读取、范围、修改、保存/加载 |
| ROI/RONI | <code>I_ROI</code>、<code>RoiPanel</code>、display drag | Wired into application | window、RONI、边界和坐标 |
| Anti-flicker | <code>I_AntiFlickerModule</code>、<code>EspPanel</code> | Wired into application | band、50/60 Hz、unsupported device |
| Trail filter | <code>I_EventTrailFilterModule</code>、<code>EspPanel</code> | Wired into application | type、threshold、enable |
| ERC | <code>I_ErcModule</code>、<code>EspPanel</code> | Wired into application | target rate 和设备限制 |
| Trigger input/output | <code>I_TriggerIn</code>、<code>I_TriggerOut</code>、<code>TriggerPanel</code> | Wired into application | channels、period、duty、真实信号 |
| Manual disconnect | <code>MainWindow::on_disconnect()</code> 先 stop recorder，再 <code>CameraController::disconnect()</code> | Wired into application | active recording/algorithm 时清理 |
| Runtime error | live error callback 发出 error/stopped 并请求 stop | Implemented in source | 异常拔线后的 UI、facility 和句柄状态是潜在风险，需要运行时验证 |
| Reconnect | 用户可再次调用 first/serial connect，连接前执行 <code>teardown()</code> | Implemented in source | 未找到自动 retry/backoff；手动重连需验证 |
| Clean shutdown | <code>CameraController::teardown()</code>、<code>MainWindow::closeEvent()</code> | Implemented in source | callbacks、stream、GL 和 worker 清理是潜在风险，需要运行时验证 |

### 5.3 Sensor self-test

静态入口为：

1. <code>DevicesPanel::self_test_requested</code>
2. <code>MainWindow::on_open_algo_window("sensor_self_test")</code>
3. <code>AlgoBridge::register_self_analytics()</code>
4. <code>SensorSelfTestBackend</code>
5. <code>gui_algo::SensorSelfTest</code>

输出包括 refractory heatmap、Triggered/Measured/Bad 摘要，以及关闭窗口时的统计报告和疑似坏点坐标。证据等级为 <code>Wired into application</code>，但运行状态为 <code>Requires runtime verification</code>。

当前入口只检查 camera controller 是否 connected，而 file source 也属于 connected；文件模式 timestamp 可能按播放倍率缩放，live/file 算法输入还可能经过全局 FilterChain。以上属于明确的接线差异；其实际影响是潜在风险，需要用真实相机和 RAW 分别验证，不能把“测试期间未触发”直接等同于硬件坏点。

### 5.4 RAW and file playback

| Capability | Evidence | Level | Required verification |
| --- | --- | --- | --- |
| File selection | <code>MainWindow::on_open_file()</code>：<code>*.raw *.hdf5 *.h5 *.dat</code> | Wired into application | 每种格式及缺失 plugin |
| Open API | <code>PlaybackController::open_file()</code> → <code>CameraController::connect_file()</code> → <code>Camera::from_file()</code> | Wired into application | 正常、损坏、无权限和空文件 |
| Playback mode | <code>FileConfigHints::real_time_playback(false)</code>；events 进入 <code>FileFrameGenerator</code> | Implemented in source | 加载时间和 SDK stop 状态 |
| Metadata | <code>fetch_sensor_info()</code>、<code>FileConverter::info()</code>、OSC duration | Wired into application | geometry、encoding、duration、缺失 fields |
| Buffering | <code>FileFrameGenerator</code> 将 events 保存到内存 vector | Implemented in source | 大 RAW 内存和启动延迟是潜在风险，需要运行时验证 |
| Play/pause/resume | <code>PlaybackController::play/pause/toggle_play_pause</code> | Wired into application | 状态同步、重复操作 |
| Step | <code>PlaybackControls</code> 通过 accumulation window seek | Wired into application | EOF 前后和不同 window |
| Seek | <code>PlaybackController::seek()</code> → <code>FileFrameGenerator::seek()</code> | Wired into application | 前向、后向、边界、EOF 后 |
| Loop | <code>set_loop()</code> 和 <code>file_looped</code> | Wired into application | 多轮播放和 stateful algorithm reset |
| Speed | fps、accumulation window、multiplier | Wired into application | 极慢、1x、快进和 timestamp scaling |
| EOF | <code>FileFrameGenerator::eof_reached</code> → <code>PlaybackController::on_file_eof()</code> | Implemented in source | 正常 EOF 与解码/插件错误区分是潜在风险，需要运行时验证 |
| RAW → algorithms | <code>events_window_ready</code> → <code>MainWindow::on_events_window_ready()</code> | Wired into application | 每个算法、seek/loop/source switch |
| RAW → preprocessing | render window 应用 <code>FilterChain</code> | Wired into application | 与 live 路径的顺序和坐标一致性 |
| File/live switching | connect paths 先 teardown；MainWindow 切换前停止 recorder | Implemented in source | file-to-file、disconnect/reopen 和 live↔file 状态是潜在风险，需要运行时验证 |
| Export | <code>ExporterController</code> 和 <code>FileConverter</code> 重新读取 source file | Wired into application | 见输出格式表 |

<code>PlaybackController::closed</code> 声明但未找到 emit，且 controller 未直接监听 camera disconnected；<code>PlaybackControls::crop_range_requested</code> 也只找到声明。它们是静态接线缺口。是否导致用户可见的 stale state 或重开失败是潜在风险，需要运行时验证。

### 5.5 Algorithm registry and shared wiring

当前 HEAD 实际注册 60 个唯一名称：

| Category | Count |
| --- | ---: |
| OpenEB filters | 10 |
| OpenEB frame modes | 7 |
| OpenEB preprocessors | 7 |
| OpenEB utilities | 6 |
| Self CV | 21 |
| Self analytics | 8 |
| Self calibration | 1 |
| Total | 60 |

共同规则：

- <code>AlgorithmsPanel</code> 只显示 <code>source == "self"</code> 的 30 项，通过 <code>find_or_create()</code> 创建 instance、编辑参数并启停；普通主算法采用互斥选择。
- 除 <code>sensor_self_test</code> 和 calibration 外，self 项自动附加共享 ROI 与 stackable preprocessing 参数。
- live 输入在 Metavision SDK CD callback thread 同步调用 backend；RAW 输入在 GUI thread 的 <code>on_events_window_ready()</code> 同步调用。<code>AlgoInstance</code> 使用 mutex 串行化 push、参数和 pull。
- 算法没有统一的结果导出 controller。<code>AlgoResult</code> 主要由 <code>process_algo_results()</code> 消费为 frame、overlay 或 status；filtered events 是否产生用户可见效果须逐项验证。
- 30 个 OpenEB registry 项不显示在 <code>AlgorithmsPanel</code>。只有 8 个 FilterChain stage 通过 preprocessing UI 和 live/RAW pipeline 接线；其他项即使存在 backend，也不能仅凭注册宣称为普通 GUI 功能。

#### 5.5.1 Self CV（21）

表中 live/RAW、启停和参数 UI 沿用上述 self 通用接线；所有行均无专用算法结果导出。
表中省略目录的 backend 文件名均位于 <code>gui/algo_bridge/backends/</code>。

| Registry name | Implementation, input and output | Parameters | Evidence and runtime test |
| --- | --- | --- | --- |
| <code>hot_pixel_filter</code> | <code>algo/cv/hot_pixel_filter.h::HotPixelFilter</code>；<code>cv_backends.cpp::HotPixelFilterBackend</code>；EventCD → filtered events/status | learning_window_s、n_sigma、FPN flags/rate | Wired into application；验证 learning、FPN、live/RAW 和可见过滤效果 |
| <code>orientation_filter</code> | <code>algo/cv/orientation_filter.h::OrientationFilter</code>；<code>filter_backends.cpp::OrientationFilterBackend</code>；EventCD → colored events、orientation line/histogram | tau、neighbors、min-dt、multi/history/pass-all | Wired into application；验证方向、overlay 和 reset |
| <code>direction_selective</code> | <code>algo/cv/direction_selective_filter.h::DirectionSelectiveFilter</code>；<code>filter_backends.cpp::DirectionSelectiveBackend</code>；EventCD → direction colors、motion lines/text | tau、min-dt、distance、low-pass、global mode | Wired into application；验证 8 方向和 global motion |
| <code>sparse_optical_flow</code> | <code>algo/cv/sparse_optical_flow.h::SparseOpticalFlow</code>；<code>cv_backends.cpp::SparseOpticalFlowBackend</code>；EventCD → flow points/vectors | LocalPlanes/LucasKanade/BlockMatch/ClusterOF、radius、window、EMA | Wired into application；逐 mode 验证 live/RAW |
| <code>blob_detector</code> | <code>algo/cv/blob_detector.h::BlobDetector</code>；<code>cv_backends.cpp::BlobDetectorBackend</code>；EventCD → bounding boxes | threshold、learning_rate | Wired into application；验证 detection、学习和 overlay |
| <code>object_tracker</code> | <code>algo/cv/object_tracker.h::ObjectTracker</code>；<code>cv_backends.cpp::ObjectTrackerBackend</code>；EventCD → boxes、IDs、velocity、trajectory | RCT/Median/Kalman/MultiHypothesis 及 cluster/lost/prediction 参数 | Wired into application；验证多目标和 source reset |
| <code>corner_detector</code> | <code>algo/cv/corner_detector.h::CornerDetector</code>；<code>cv_backends.cpp::CornerDetectorBackend</code>；EventCD → corner points | mode、min_score | Conflicting evidence：registry 显示 Harris/FAST/AGAST，而真实 enum 为 EndStopped/TypeCoincidence/Harris；逐 mode 验证 |
| <code>line_segment</code> | <code>algo/cv/line_segment_detector.h::LineSegmentDetector</code>；<code>cv_vector_backends.cpp::LineSegmentBackend</code>；EventCD → ELiSeD lines | min_length、gap | Wired into application；验证线段坐标和 overlay |
| <code>hough_line</code> | <code>algo/cv/hough_line_tracker.h::HoughLineTracker</code>；<code>cv_vector_backends.cpp::HoughLineBackend</code>；EventCD → lines 和 accumulator frame | threshold、theta/rho bins、decay | Wired into application；CPU、内存和准确性是潜在风险，需要运行时验证 |
| <code>hough_circle</code> | <code>algo/cv/hough_circle_tracker.h::HoughCircleTracker</code>；<code>cv_vector_backends.cpp::HoughCircleBackend</code>；EventCD → circles 和 accumulator | radius、threshold、decay、buffer、max circles | Wired into application；CPU、throttle、准确性需验证 |
| <code>orientation_cluster</code> | <code>algo/cv/orientation_cluster.h::OrientationCluster</code>；<code>cv_vector_backends.cpp::OrientationClusterBackend</code>；EventCD → centroid/orientation lines | min_events、dt、RF、orientation/history 等 | Wired into application；验证 cluster 和 history |
| <code>cluster_lif</code> | <code>algo/cv/cluster_lif.h::ClusterLIF</code>；<code>cv_vector_backends.cpp::ClusterLifBackend</code>；EventCD → LIF cluster boxes | tau、threshold、RF、potential/jump | Wired into application；验证 spikes 和 reset |
| <code>background_mask</code> | <code>algo/cv/background_mask_filter.h::BackgroundMaskFilter</code>；<code>filter_backends.cpp::BackgroundMaskBackend</code>；EventCD → mask frame | learning_rate、threshold、erosion | Wired into application；registry learning_rate 到 backend 语义需验证 |
| <code>perspective_undistort</code> | <code>algo/cv/perspective_undistort.h::PerspectiveUndistort</code>；<code>cv_backends.cpp::PerspectiveUndistortBackend</code>；EventCD → transformed events | enable、zoom | Conflicting evidence：未找到 calibration/LUT 注入路径；验证 calibration、geometry 和可见输出 |
| <code>trigger_synced</code> | <code>algo/cv/trigger_synced_filter.h::TriggerSyncedFilter</code>；<code>analytics_extra_backends.cpp::TriggerSyncedBackend</code>；EventCD + trigger intent → filtered events | window_us、t0_us、trigger_channel | Conflicting evidence：未找到 external trigger events 注入 backend 的路径；用真实 trigger 验证 |
| <code>bandpass_filter</code> | <code>algo/cv/bandpass_filter.h::BandpassFilter</code>；<code>filter_backends.cpp::BandpassFilterBackend</code>；ROI event rate → text/status | low/high cutoff | Wired into application；用合成频率和真实流验证 |
| <code>optical_gyro</code> | <code>algo/cv/optical_gyro.h::OpticalGyro</code>；<code>cv_backends.cpp::OpticalGyroBackend</code>；EventCD → transformed events、motion lines/text | stabilize、rotation、smoothing | Wired into application；主显示采用方式和准确性需验证 |
| <code>ultra_slow_motion</code> | <code>algo/cv/ultra_slow_motion.h::UltraSlowMotion</code>；<code>display_backends.cpp::UltraSlowMotionBackend</code>；EventCD → timestamp-dilated events | factor | Wired into application；与 playback rate 和显示接线需验证 |
| <code>xyt_visualizer</code> | <code>algo/cv/xyt_visualizer.h::XYTVisualizer</code>；<code>display_backends.cpp::XYTVisualizerBackend</code>；独立 <code>SpaceTimeDisplay</code> point cloud | time_window_us、max_points | Wired into application；双路径参数、live/RAW 和 GL 资源需验证 |
| <code>overlay</code> | <code>display_backends.cpp::OverlayBackend</code>；ROI pass-through，未生成专用 primitive | 无专用参数 | Wired into application；实际用户可见输出需验证 |
| <code>time_surface</code> | <code>algo/cv/time_surface.h::TimeSurface</code>；<code>display_backends.cpp::TimeSurfaceBackend</code>；EventCD → time-surface frame | decay、palette、channels | Wired into application；Standalone frame、reset 和 channels |

#### 5.5.2 Self analytics and calibration（9）

表中省略目录的 backend 文件名均位于 <code>gui/algo_bridge/backends/</code>。

| Registry name | Implementation, input and output | Parameters | Evidence and runtime test |
| --- | --- | --- | --- |
| <code>active_marker</code> | <code>algo/analytics/active_marker.h::ActiveMarker</code>；<code>analytics_extra_backends.cpp::ActiveMarkerBackend</code>；EventCD → marker circles/Hz text | window_us、min_events | Wired into application；检测、tracking、live/RAW |
| <code>event_to_video</code> | <code>algo/analytics/event_to_video.h::EventToVideo</code>；<code>analytics_backends.cpp::EventToVideoBackend</code>；EventCD → intensity frame | Bardow/InteractingMaps/E2VID、fps、window/decay、TV lambdas、IM params、model_path/bins/HDR/sharpen | Wired into application；三模式、seek/reset、性能和画质 |
| <code>flow_statistics</code> | <code>algo/analytics/flow_statistics.h::FlowStatistics</code>；<code>analytics_backends.cpp::FlowStatisticsBackend</code> 当前累计 events，设计需要 ground truth | 无专用参数 | Conflicting evidence：未找到 GUI ground-truth 输入；验证产品预期和 metrics |
| <code>isi_analyzer</code> | <code>algo/analytics/isi_analyzer.h::ISIAnalyzer</code>；<code>analytics_backends.cpp::ISIAnalyzerBackend</code>；EventCD → histogram frame | per_pixel、max_isi_ms | Wired into application；histogram 和 per-pixel |
| <code>particle_counter</code> | <code>algo/analytics/particle_counter.h::ParticleCounter</code>；<code>analytics_extra_backends.cpp::ParticleCounterBackend</code>；EventCD → cumulative count/status | line_y、min_area | Wired into application；tracking、line crossing 和 reset |
| <code>auto_bias</code> | <code>algo/analytics/auto_bias_controller.h::AutoBiasController</code>；<code>analytics_extra_backends.cpp::AutoBiasBackend</code>；event rate → <code>BiasCommand</code>/status | target_event_rate_mev | Conflicting evidence：未找到 backend 调用 camera bias facility；验证是建议还是实际控制 |
| <code>freq_detector</code> | <code>algo/analytics/freq_detector.h::FreqDetector</code>；<code>analytics_extra_backends.cpp::FreqDetectorBackend</code>；EventCD → source circles/Hz text | update_interval_s、min_events | Wired into application；频率准确性和 Standalone 显示 |
| <code>sensor_self_test</code> | <code>algo/analytics/sensor_self_test.h::SensorSelfTest</code>；<code>analytics_backends.cpp::SensorSelfTestBackend</code>；EventCD → refractory heatmap/report | 无用户参数；内部 final report flag | Wired into application；真实相机、全传感器、测试时长、RAW gating |
| <code>intrinsic_calibration</code> | registry item 无通用 backend；实际 <code>gui/calibration/calibration_wizard.cpp</code> → <code>algo/calibration/intrinsic.cpp::IntrinsicCalibration</code> → YAML | registry board/squares/size；wizard 自有控件 | Conflicting evidence；验证 registry entry 与 wizard 的职责和完整 calibration 流程 |

#### 5.5.3 OpenEB filters（10）

这些 registry 项位于 <code>gui/algo_bridge/algo_bridge.cpp</code>。8 个已接线 stage 位于 <code>gui/algo_bridge/filter_chain.cpp</code>，另外两个 backend 位于 <code>gui/algo_bridge/backends/openeb_filter_backends.cpp</code>。

| Registry name | Implementation and parameters | Application wiring | Evidence and runtime test |
| --- | --- | --- | --- |
| <code>roi_filter</code> | <code>FilterChain::RoiFilterStage</code>；x0/y0/x1/y1/relative coordinates | Preprocessing/ROI pipeline，live + RAW | Wired into application；geometry 和 relative coordinates |
| <code>polarity_filter</code> | <code>PolarityFilterStage</code>；polarity | FilterChain，live + RAW | Wired into application；ON/OFF |
| <code>polarity_invert</code> | <code>PolarityInvertStage</code> | FilterChain，live + RAW | Wired into application |
| <code>flip_x</code> | <code>FlipXStage</code> | FilterChain，live + RAW | Wired into application；geometry |
| <code>flip_y</code> | <code>FlipYStage</code> | FilterChain，live + RAW | Wired into application；geometry |
| <code>rotate</code> | <code>RotateStage</code>；0/90/180/270 | FilterChain，live + RAW | Wired into application；非方形 sensor 边界 |
| <code>transpose</code> | <code>TransposeStage</code> | FilterChain，live + RAW | Wired into application；bounds |
| <code>rescale</code> | <code>RescaleStage</code>；scale width/height | FilterChain，live + RAW | Wired into application；downstream geometry |
| <code>roi_mask</code> | <code>RoiMaskBackend</code>；mask_path | 不显示在 AlgorithmsPanel，未找到普通 UI 创建路径 | Implemented in source；mask loading、尺寸和产品入口 |
| <code>adaptive_rate_split</code> | <code>AdaptiveRateSplitBackend</code>；variance threshold/downsampling | 不显示在 AlgorithmsPanel，未找到普通 UI 创建路径 | Implemented in source；event output 和产品入口 |

#### 5.5.4 OpenEB frame modes（7）

这些项的 registry 位于 <code>gui/algo_bridge/algo_bridge.cpp</code>，backend 位于 <code>gui/algo_bridge/backends/openeb_frame_backends.cpp</code>。它们不显示在 <code>AlgorithmsPanel</code>，未找到普通 GUI 创建和启用路径；证据等级为 <code>Implemented in source</code>，而不是 <code>Wired into application</code>。

| Registry name | Backend/output | Parameters | Required verification |
| --- | --- | --- | --- |
| <code>frame_integration</code> | <code>FrameIntegrationBackend</code> → frame | decay_time_us | 产品入口、frame 和 reset |
| <code>frame_diff</code> | <code>FrameDiffBackend</code> → diff frame | bit_size、rollover | product wiring、rollover |
| <code>frame_histogram</code> | <code>FrameHistoBackend</code> → histogram frame | channel bits、packed | color/channel mapping |
| <code>frame_time_decay</code> | <code>FrameTimeDecayBackend</code> → decay frame | decay、palette | reset 和 palette |
| <code>frame_contrast_map</code> | <code>FrameContrastMapBackend</code> → contrast frame | contrast_on/off | output 和 reset |
| <code>frame_periodic</code> | <code>FramePeriodicBackend</code> → callback-updated frame | accumulation、fps | callback thread、timing、产品入口 |
| <code>frame_on_demand</code> | <code>FrameOnDemandBackend</code> → on-demand frame | accumulation | pull timing 和产品入口 |

#### 5.5.5 OpenEB preprocessors（7）

这些 registry 项位于 <code>gui/algo_bridge/algo_bridge.cpp</code>；已有 backend 均位于 <code>gui/algo_bridge/backends/openeb_preproc_backends.cpp</code>。

| Registry name | Backend/output and parameters | Evidence | Required verification |
| --- | --- | --- | --- |
| <code>preproc_diff</code> | <code>PreprocDiffBackend</code> → tensor/frame；increment/clip | Implemented in source；无普通 UI | shape、normalization、产品入口 |
| <code>preproc_histo</code> | <code>PreprocHistoBackend</code> → tensor；increment/clip/CHW | Implemented in source；无普通 UI | layout、polarity channels |
| <code>preproc_hw_diff</code> | registry only；未找到同名 factory backend | Conflicting evidence | intended hardware input 和实现范围 |
| <code>preproc_hw_histo</code> | registry only；未找到同名 factory backend | Conflicting evidence | intended hardware input 和实现范围 |
| <code>preproc_time_surface</code> | <code>PreprocTimeSurfaceBackend</code>；channels | Implemented in source；无普通 UI | channel layout、reset |
| <code>preproc_event_cube</code> | <code>PreprocEventCubeBackend</code>；bins/delta/polarity/increment | Implemented in source；无普通 UI | tensor shape、memory、产品入口 |
| <code>preproc_factory</code> | <code>PreprocFactoryBackend</code>；config_path；源码标记 stub/pass-through | Implemented in source | JSON parsing 和实际 processor creation |

#### 5.5.6 OpenEB utilities（6）

所有 registry 项位于 <code>gui/algo_bridge/algo_bridge.cpp</code>，backend 位于 <code>gui/algo_bridge/backends/openeb_util_backends.cpp</code>。它们均无普通 GUI 创建路径，证据等级不高于 <code>Implemented in source</code>。

| Registry name | Current backend behavior | Parameters | Required verification |
| --- | --- | --- | --- |
| <code>util_rate_estimator</code> | status-only stub | 无 | 与 Statistics 的职责和产品入口 |
| <code>util_frame_composer</code> | passive container | 无 | composition workflow |
| <code>util_rolling_buffer</code> | 实际 <code>RollingEventBuffer</code> wrapper | mode、N events、delta time | N_EVENTS/N_US、capacity、reset |
| <code>util_video_writer</code> | stub，提示使用 Export menu | 无 | 与实际 AVI export 的职责 |
| <code>util_data_synchronizer</code> | stub/pass-through | period_us | trigger/multi-stream synchronization |
| <code>util_timing_profiler</code> | status-only stub | 无 | timing 数据和展示 |

### 5.6 Models and inference

| Flow | Evidence | Status | Required verification |
| --- | --- | --- | --- |
| E2VID heuristic | <code>EventToVideo</code> mode 2 在无 ORT 或 load/infer failure 时回退 | Wired into application | 无模型、坏模型、frame quality、用户提示 |
| Plain ONNX | <code>E2VIDInference::load_model/infer_onnx()</code> 创建 CPU <code>Ort::Session</code> | Implemented in source | arm64 ORT、input/output shape、dynamic dimensions |
| Recurrent ONNX | 管理 recurrent state tensors 和 reset | Implemented in source | state names、shape、seek/loop reset |
| Model conversion | tracked <code>models/convert_to_onnx.py</code>，输入 PyTorch checkpoint、输出 opset 17 ONNX | Implemented in source | 仓库内 venv、依赖、plain/recurrent export、onnx checker |

当前 <code>models/</code> 只跟踪转换脚本，没有 tracked <code>.onnx</code>、<code>.pth</code> 或 <code>.pt</code> 文件。registry 默认 <code>models/e2vid_lightweight.onnx</code> 当前不存在。C++ 路径只发现 CPU execution，未配置 CUDA 或 CoreML provider；相对模型路径依赖进程 working directory。

### 5.7 Visualization

- 2D events/frame：<code>EventDisplayWidget</code> 使用 QOpenGLWidget、GLSL 330 core、VAO/VBO。
- Overlay：<code>FrameAnnotator</code> 和各 display strategy 绘制 points、boxes、lines、circles、text。
- 3D XYT：<code>SpaceTimeDisplay</code> 使用独立 OpenGL widget。
- Standalone/Replace/Overlay/Passive 由 <code>AlgoDisplayMode</code> 和 <code>AlgoWindow</code> 路由。

以上均为 <code>Wired into application</code>，但 OpenGL context、跨线程更新、实际 overlay 可见性和资源释放均为 <code>Requires runtime verification</code>。

### 5.8 Export and generated outputs

| Format/output | Entry and implementation | Mode | Evidence and limitations |
| --- | --- | --- | --- |
| Live RAW <code>.raw</code> | <code>MainWindow::on_record_start()</code> → <code>RecorderController</code> → <code>I_EventsStream::log_raw_data</code> | Live camera only | Wired；需验证 flush/footer、错误、重开和磁盘增长 |
| Companion <code>.bias</code> | RAW 开始时 best-effort 保存同 basename bias | Live | Wired；依赖 bias facility |
| HDF5 <code>.h5</code> | <code>FileConverter</code> 和 <code>ExporterController</code> → <code>HDF5EventFileWriter</code> | File source | Wired；plugin、cancel、partial file、disk full |
| CSV <code>.csv</code> | <code>FileConverter</code> 写 header <code>t,x,y,p</code> 和逐 event 文本 | File source | Wired；可能显著放大磁盘占用 |
| RAW clip <code>.raw</code> | <code>FileConverter::run_cut()</code> → <code>RAWEvt2EventFileWriter</code> | File source | Wired；seek/no-seek、时间边界、metadata |
| AVI <code>.avi</code> | <code>ExporterController::run_avi()</code> → <code>CDFrameGenerator</code> + <code>CvVideoRecorder</code> | File source | Wired；H264/MJPG codec 可用性是潜在风险，需要运行时验证 |
| Calibration YAML | <code>CalibrationWizard::on_intrinsic_save()</code> → OpenCV FileStorage | Calibration | Wired；保存 geometry、K、distortion、RMS |
| Camera/config JSON | <code>ConfigManager::save_to_file/load_from_file()</code> | User-selected | Wired；round-trip 和 sensor mismatch |
| Algorithm params JSON | <code>save_algo_params_to_file/load_algo_params_from_file</code> | User-selected | Wired；未知算法、lazy instance cache、全部实际 registry 项 |
| Layout JSON | <code>LayoutManager::save/load</code> | User-selected/default | Wired；默认路径另见配置章节 |
| Target-label JSON | <code>TargetLabeler::save_to_json/load_from_json</code> | Annotation | Implemented in source；未确认主应用完整入口 |

未找到 MP4、PNG/JPEG screenshot 或将通用 <code>AlgoResult</code> 写入文件的统一导出链。HDF5/AVI/CSV/RAW cutter 重新读取原始 source file，不等同于导出算法结果。保存对话框通常由用户选择路径；writer 层未普遍实现独立的 no-overwrite guard，取消或失败是否保留 partial output 需逐格式验证。

### 5.9 Configuration, state and logging

| State/config | Path | Evidence and policy impact |
| --- | --- | --- |
| Theme | <code>ThemeController</code> + <code>QSettings</code> | 启动间持久化 |
| Recent files | <code>MainWindow</code> + <code>QSettings("recentFiles")</code> | 保存用户文件绝对路径 |
| Sidebar group | <code>SettingsPanel</code> + <code>QSettings</code> | 保存 active group |
| Default layout | <code>LayoutManager::default_path()</code> + <code>QStandardPaths::AppConfigLocation</code> | 启动 load、退出 save，并调用 <code>mkpath</code>；与 repository-local policy 存在待协调项 |
| Camera config | SDK <code>Camera::save/load</code> 和 ConfigManager JSON | 用户选择文件 |
| Bias | <code>I_LL_Biases::save_to_file/load_from_file</code> | 用户选择文件 |
| Runtime reporting | status bar、dialogs、signals、<code>QT_LOGGING_RULES</code> | 未找到项目自有持久日志文件 controller |

Linux 上 <code>QSettings</code>/<code>QStandardPaths</code> 通常使用用户配置目录，macOS 通常使用 <code>$HOME/Library</code>。实际路径和写入行为需运行确认；静态上可以确认项目当前没有把这些默认状态重定向到 repository-local workspace。

### 5.10 Error handling, threads and shutdown

- Camera connect/start/stop/config 路径捕获 <code>CameraException</code> 并发出 signals。
- HAL panels 多数把异常转为 <code>error_message</code>；主窗口显示 dialog/status。
- CD callback 包含 exception boundary，避免异常越过 SDK streaming thread。
- <code>FileConverter</code> 和 <code>ExporterController</code> 使用各自 <code>std::thread</code>，结果以 queued Qt invocation 返回 GUI thread。
- Recorder 使用 GUI-thread timers 周期性读取和 flush RAW data。
- self algorithm backend 没有独立 worker：live 同步运行在 SDK CD callback thread，RAW 同步运行在 GUI thread；重算法可能影响 callback 或 GUI 响应，这属于潜在性能风险，需要运行时验证。
- <code>MainWindow::closeEvent()</code>、<code>CameraController::teardown()</code>、<code>RecorderController::stop()</code> 和 OpenGL widgets 均包含显式清理路径。

设备枚举和 sensor info 的部分异常会被转换为空列表/空字段；file source runtime error 的分类、打开失败提示数量以及 catch-all 静默路径需要错误注入验证。不能据静态代码宣称这些路径已经失败或已经正确恢复。

## 6. Linux-specific assumptions and macOS hotspots

| File/symbol | Current Linux behavior | macOS concern | Target milestone | Linux regression concern |
| --- | --- | --- | --- | --- |
| <code>run.sh</code> | Bash、<code>LD_LIBRARY_PATH</code>、<code>/usr/local</code>、Wayland→XCB、OpenGL | macOS 使用 DYLD/RPATH、Cocoa，无 XCB/Wayland | M3/M4 | 必须保留 Linux launcher 行为 |
| <code>gui/main.cpp::ensure_openeb_env_defaults()</code> | 重复设置 HAL/HDF5 <code>/usr/local</code> 默认值和 XCB/OpenGL | 无平台 guard，路径/显示后端不同 | M3/M4 | Linux defaults 只能条件隔离 |
| <code>gui/main.cpp::main()</code> | 请求 OpenGL 3.3 core/vsync | Cocoa QOpenGLWidget/GLSL 330 需验证 | M4 | Linux GL context 不应退化 |
| <code>MainWindow</code> | 无条件 frameless window + custom grips | macOS system move/resize 行为不同 | M4 | Linux custom chrome 保持 |
| <code>algo/CMakeLists.txt</code> | Linux ONNX search、<code>.so</code>、<code>$ORIGIN</code> | 需要 <code>.dylib</code>、<code>@rpath</code>/<code>@loader_path</code> | M3/M7 | Linux ORT 搜索继续可用 |
| HAL plugin path | 默认 <code>/usr/local/lib/metavision/hal/plugins</code> | 隔离的 5.2.0 prefix 和 bundle plugin 路径 | M2/M6/M8 | 不混用 5.1.1/5.2.0 |
| HDF5 plugin path | 默认 <code>/usr/local/lib/hdf5/plugin</code> | 隔离 plugin 和 bundle path | M2/M5/M7 | Linux HDF5 继续可读 |
| QSettings/QStandardPaths | 用户配置目录持久化 | macOS 通常写 <code>$HOME/Library</code> | M4 | 需协调 workspace policy 和既有行为 |
| File dialogs | 用户可选择任意输出路径 | repository-local policy 需要项目控制边界 | M4/M7 | 不无条件限制既有 Linux 用户 |
| <code>models/e2vid_lightweight.onnx</code> | 相对 working directory | app bundle/不同启动 cwd 易失效 | M7/M8 | Linux relative-path workflow 需兼容 |

本轮未在 EBplus 边界内发现 <code>APPLE</code>、<code>__APPLE__</code>、<code>CMAKE_OSX_ARCHITECTURES</code>、<code>MACOSX_BUNDLE</code> 或 macOS RPATH 实现。

## 7. Documentation and implementation conflicts

1. README 要求 Ubuntu 22.04+ / GCC 13+，<code>doc/compile.md</code> 记录 Ubuntu 26.04 / GCC 15。
2. README 和 <code>run.sh</code> 使用 <code>build/gui/gui_for_openeb</code>，<code>doc/compile.md</code> 的直接运行示例使用 <code>build/gui_for_openeb</code>。
3. 既有编译文档把 GTest 描述为可选，但顶层无条件加入 <code>algo/tests</code>，其中 <code>find_package(GTest REQUIRED)</code>。
4. README 和 <code>AlgoBridge</code> 文件头仍写 59 项（29 self + 30 OpenEB）；当前 registry 实际为 60 项（30 self + 30 OpenEB），测试源码也静态断言 60，但本轮未运行测试。
5. 新工作区规范要求 <code>.build/</code>，现有 Linux README 和 <code>run.sh</code> 仍使用 legacy <code>build/</code>。
6. <code>doc/compile.md</code> 包含 <code>/tmp</code>、仓库外 venv、sudo install、<code>/usr/local</code> 和 shell profile 修改；本轮只记录，不修改。
7. EBplus 顶层只使用 <code>find_package(MetavisionSDK)</code>，不会自动使用仓库内 OpenEB 源码；<code>run.sh</code> 默认又指向 <code>/usr/local</code>。
8. 根仓库跟踪 mode 160000 的 <code>openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf</code>，但根级缺少对应 <code>.gitmodules</code> mapping；<code>git submodule status</code> 会失败。
9. <code>main_window.h</code> 注释仍提 6 menus/Camera menu，实际只有 File/View/Theme/Tools/Help；<code>EventDisplayWidget</code> 空状态仍提示 Camera menu。
10. <code>corner_detector</code> registry mode 标签与真实 enum 顺序明确不一致。
11. <code>intrinsic_calibration</code> registry item 没有通用 backend，实际功能由 Tools/CalibrationWizard 驱动。
12. OpenEB 大多数 registry 项存在 backend 或 stub，但没有普通 GUI 创建入口；注册数量不能作为用户功能数量。
13. 默认 E2VID ONNX 路径指向未 tracked 的模型；heuristic fallback 不能写成 ONNX inference 已成功。
14. Target-label JSON 有源码实现，但未确认主应用中的完整实例化和用户入口。

## 8. Unknowns

- 当前 Linux 支持的精确发行版、编译器、Qt/OpenCV/Metavision/GTest 版本组合。
- 默认 configure 是否成功，以及条件 <code>MetavisionSDK::hal</code> target 是否由已安装 SDK 提供。
- 最终 executable 的 ONNX Runtime RPATH 和实际动态库来源。
- HAL/HDF5 plugins 的实际加载路径和厂商设备兼容范围。
- OpenGL 3.3、frameless window、XCB/Wayland 和多显示器行为。
- 每种 HAL facility 在具体设备上的存在、参数范围和错误恢复。
- RAW/HDF5/DAT 的实际 decoder、metadata、seek、EOF 和损坏文件行为。
- file-to-file、disconnect/reopen、live↔file 切换后的 controller/UI 状态。
- 每个算法的数值正确性、性能、实际显示、reset 和资源占用。
- OpenEB registry 中没有普通 UI 的 22 项是否计划成为产品功能。
- ONNX plain/recurrent 模型的真实 shape、性能、内存和输出质量。
- 导出 codec、HDF5 plugin、覆盖、取消、partial cleanup 和磁盘满行为。
- QSettings、QStandardPaths 和 file dialogs 的实际 Linux/macOS 写入位置。
- 正常退出和异常退出时 callbacks、RAW footer、threads 和 GL resources 是否完整释放。

## 9. Runtime verification backlog

### Build and launch

- Linux configure、build、CTest；记录发行版、架构、编译器和依赖版本。
- 验证 5 个 GUI tests 和 11 个 algo test/diagnostic targets。
- Linux <code>ldd/readelf</code> 和未来 macOS <code>otool -L</code>。
- GUI launch、五个菜单、OpenGL 3.3、frameless resize、layout/theme 和 clean exit。

### RAW

- 分别验证 RAW、HDF5、H5、DAT 的 open、metadata 和 duration。
- playback、pause/resume、step、seek、loop、multiplier、EOF。
- 空文件、损坏文件、缺 plugin、无权限和大文件内存增长。
- file-to-file、disconnect/reopen、live↔file。
- FilterChain、每个 self algorithm、seek/loop reset 和 timestamp scaling。

### Live camera

- Device enumeration、first local、serial selection、live stream。
- 每种 facility 的读取、修改、缺失降级和错误回滚。
- 手动断开、物理拔线、runtime error、手动重连、clean shutdown。
- Sensor self-test 的 live-only 约束、刺激充分性、全传感器覆盖、heatmap/report 和关闭。

### Algorithms and models

- 对当前实际 60 个 registry 项逐项确认产品入口；没有入口的 OpenEB 项先确认产品预期。
- 对 30 个 self 项分别验证 enable/disable、参数、ROI/preprocessing、live、RAW、输出类型、reset 和切源。
- 专项验证 corner mode、perspective calibration、trigger input、flow ground truth、auto-bias device write 和 filtered-event 可见性。
- E2VID heuristic、plain ONNX、recurrent ONNX、模型缺失/错误 fallback、dynamic shape、state reset 和 CPU 性能。

### Export

- Live RAW 和 companion bias。
- HDF5、CSV、RAW clip、AVI、YAML、camera/algo/layout JSON、bias 和 target labels。
- 每种格式验证有效输入、错误输入、取消、同名覆盖、磁盘满、partial output、资源释放和输出可重新打开。
- 明确哪些算法结果需要专用导出；不得把原始事件导出写成算法结果导出。

## 10. Baseline conclusion

当前文档确认了 Linux 版本在源码层面的构建入口、应用结构、功能范围、算法 registry、OpenEB 集成边界和平台热点。当前 HEAD 实际注册 60 个唯一算法名称，但 registry 或 backend 存在不等于普通 GUI 用户能够使用，也不等于运行结果正确。

由于本轮未执行配置、构建和运行测试，所有运行时能力仍需通过后续 Linux smoke test、RAW 样本、模型、动态链接检查或真实设备测试确认。本基线不得被引用为“Linux 全功能正常”的证明。
