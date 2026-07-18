# EBplus macOS 移植路线图

本文档定义 EBplus 从当前 Linux 基线扩展到 macOS Apple Silicon 的长期路线、阶段边界和验收方法。macOS 支持目前正在开发中，尚未达到正式发布状态。

## 目标与边界

最终目标是在不破坏现有 Linux 行为的前提下，为 macOS Apple Silicon（arm64）提供正式支持，并逐步达到当前 Linux 版本的功能范围。初始目标技术栈为：

```text
macOS
Apple Silicon / arm64
OpenEB / Metavision SDK 5.2.0
Qt 6
CMake
C++17
```

本路线遵循以下边界：

- macOS 支持是跨平台增强，不是将项目改写为 macOS-only。
- 不允许为了 macOS 支持删除、绕过或破坏 Linux 代码路径。
- 平台差异应集中在 CMake 条件、预处理条件、专用脚本或平台适配层中。
- 每个 milestone 使用独立分支，范围、检查方法和完成标准必须明确。
- 一个分支不得夹带其他 milestone 的修改；发现范围外问题时只记录，不顺手修复。
- 完成 milestone 后更新本文档状态，并附上实际检查证据。
- “配置成功”或“编译成功”不能单独作为功能完成依据。
- 所有项目主动控制的 build、install、dependency、cache、temporary、log、download 和 artifact 必须位于 `$REPO_ROOT` 的标准工作区目录中；`$REPO_ROOT/.git/` 仅供 Git 自身管理。
- 采用 disk-conscious development：每个 milestone 开始前报告空间预算，结束后记录仓库与生成目录的实际增长，避免并存不必要的构建配置和重复依赖。
- 详细工作区边界、磁盘授权阈值和清理规则以 [`local_workspace_policy.md`](local_workspace_policy.md) 为准。

## 当前 Linux 基线

当前仓库的 Linux 构建与运行入口如下，后续 macOS 工作必须保留这些路径的原有行为：

- 顶层 `CMakeLists.txt` 最低要求 CMake 3.16，项目使用 C++17，未显式指定时默认 `Release`。
- 顶层依赖为 Qt 6 的 `Widgets`、`OpenGL`、`OpenGLWidgets`，MetavisionSDK 5.2.0 的 `base`、`core`、`stream`，以及 OpenCV。
- 顶层 CMake 启用 CTest，并加入 `algo/`、`gui/` 和 `algo/tests/`。
- README 当前推荐的 Linux 配置和构建入口为：

  ```bash
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -- -j"$(nproc)"
  ```

- 推荐运行入口为 `./run.sh`。脚本解析仓库根目录并启动 `build/gui/gui_for_openeb`。
- `run.sh` 当前设置 Linux 动态库、HDF5、HAL 插件、Wayland/XCB 和 OpenGL RHI 相关环境；这些逻辑属于 Linux 基线，未来只能通过平台条件隔离，不能被 macOS 默认值无条件替换。
- `gui/main.cpp` 当前也包含 Linux HAL/HDF5 默认路径和 Wayland/XCB 设置；Milestone 1 应记录其行为，Milestone 4 再进行平台隔离。
- `algo/CMakeLists.txt` 当前包含 Linux 风格的 ONNX Runtime 搜索路径和 `.so` RPATH 假设；Milestone 1 应盘点，Milestone 3 或 7 按职责边界处理。
- Linux 测试入口为构建目录中的 `ctest --output-on-failure`；实际套件、前置依赖和硬件要求须在 Milestone 1 完整核对。

以上内容是现状记录，不表示这些 Linux 专用设置可直接用于 macOS，也不授权在基础建设阶段修改它们。

## Milestone 状态

状态值使用以下含义：

- `Complete (merged via PR #1)`：Milestone 0 的基础建设已经通过 fork 内 PR #1 合并到 `main`。
- `Complete`：当前 milestone 的范围和规定检查已经完成；具体 commit、push 和 PR 状态在任务报告或 Git 历史中单独记录。
- `Planned`：已定义范围，尚未开始实施。
- 后续实施时可使用 `In progress` 或 `Blocked`，但必须附上当前证据和阻塞原因。

### Milestone 0: Repository foundation

**状态：** `Complete (merged via PR #1)`
**独立分支：** `chore/macos-porting-foundation`

**范围**

- 建立 Git、branch、commit、PR 和任务最终报告规则。
- 建立 OpenEB 5.1.1 与仓库内 5.2.0 的隔离规范。
- 建立项目内 macOS 构建/安装目录约定和安全的 ignore 规则。
- 记录当前 Linux CMake、运行脚本、依赖发现和测试入口。
- 在 README 中增加开发文档入口，同时明确 macOS 尚未正式支持。
- 只建立规则和文档，不修改 GUI、算法、模型或 OpenEB 功能。

**检查方法**

- 检查当前分支、工作区、`origin` 和 `upstream` 配置。
- 审核 `git diff --stat`、逐文件 diff 和 `git diff --check`。
- 确认 `openeb/`、`algo/`、`gui/`、`models/`、顶层 CMake 和运行脚本无功能差异。
- 检查文档链接、忽略规则、敏感信息和绝对路径使用边界。
- 明确记录本阶段未构建、未运行程序、未安装 OpenEB，以及实际 commit、push 和 PR 状态。

**完成标准**

- 仓库规则、移植路线和 OpenEB 版本隔离文档可供后续 agent 与开发者直接执行。
- README 能找到这些文档，且未宣称 macOS 已正式支持。
- 基础建设修改已拆分审核，并通过 fork 内 PR #1 合并到 `main`。
- 没有功能代码、Linux 行为或系统 OpenEB 环境变化。

### Milestone 1: Linux baseline inventory

**状态：** `Complete`
**独立分支：** `docs/linux-baseline-inventory`

**输出文档**

- [`linux_baseline_inventory.md`](linux_baseline_inventory.md)：当前 Linux 源码、构建入口、功能接线、平台热点、证据等级和运行验证 backlog。
- [`platform_parity_matrix.md`](platform_parity_matrix.md)：Linux/macOS 功能对齐状态、证据、所需验证和目标 milestone。

**范围**

- 确认当前 Linux 版本支持的完整功能范围和已知限制。
- 记录所有 GUI 页面、菜单、状态显示、配置入口和用户工作流。
- 盘点设备发现、相机打开、实时事件流、facility、参数控制、关闭和重连行为。
- 盘点 RAW 打开、元数据、播放、暂停、跳转、结束处理、算法和导出行为。
- 记录算法清单、模型及推理路径、回退路径、导出格式和输出约定。
- 记录 Linux 构建依赖、CMake 选项、运行脚本及代码中的 Linux 专用逻辑。
- 为后续 Linux/macOS 功能对齐建立逐项 checklist 和可复用测试样本说明。

**检查方法**

- 对 CMake、脚本、GUI、算法注册、模型加载、导出和配置代码进行只读审计。
- 依据当前 tracked source 重新统计算法注册项，并为功能清单逐项记录实现路径和证据等级。
- 建立无需硬件、需要 RAW 样本、需要模型和需要真实相机的运行验证 backlog。
- 本 milestone 不执行 configure、build、CTest 或功能 smoke test；不得以 README 描述或静态源码存在替代实测结论。

**完成标准**

- 已完成可追踪的 Linux 静态功能清单，并为每项记录实现路径、证据等级、已知限制和运行验证需求。
- 已建立 Linux/macOS 平台对齐矩阵，为后续 parity milestone 提供 Linux 源码基线和完成判据。
- 本 milestone 未执行运行验证；configure、build、CTest、GUI、RAW、模型、导出和真实相机检查均明确进入 runtime verification backlog。
- Linux 专用逻辑及其原因已定位，但本 milestone 不实施平台重构。

### Milestone 2: Isolated OpenEB 5.2.0 on macOS

**状态：** `Planned`
**独立分支：** `build/macos-openeb-5.2-isolation`

**范围**

- 构建仓库自带的 OpenEB / Metavision SDK 5.2.0。
- 使用项目内唯一的标准 `Release`/arm64 build tree 和独立 install prefix，不影响 `/usr/local` 中稳定的 5.1.1。
- OpenEB build 固定为 `$REPO_ROOT/.build/openeb-5.2.0-macos`，install 固定为 `$REPO_ROOT/.deps/openeb-5.2.0-macos`；不得创建仓库外或不必要的重复构建树。
- 验证 OpenEB C++ CLI、RAW 文件读取和真实相机连接。
- 参考已工作的 5.1.1 macOS 方案，逐项评估并记录 5.2.0 所需的最小补丁。
- 不盲目复制全部 5.1.1 修改，不顺带升级 OpenEB 或其他依赖。

**检查方法**

- 使用 `docs/openeb_version_isolation.md` 规定的项目内路径配置、构建和安装。
- 首次配置或构建前执行磁盘检查并报告空间预算；OpenEB 大型构建预计达到 `1 GiB` 或无法合理估算时，必须先取得用户明确授权。
- 如果预计操作完成后可用空间低于 `20 GiB` 或磁盘总容量的 `15%`，按更严格的保护线暂停，并报告可清理生成产物与替代方案；不得把系统 purgeable 空间计入预算。
- 检查 CMake cache、安装树、命令解析和动态链接，确认 5.2.0 未指向或覆盖 `/usr/local`。
- 运行版本信息与基础 C++ CLI；分别验证 RAW 打开/读取以及真实设备枚举、打开和事件流。
- 比较普通终端与项目专用环境，确认普通终端仍解析到稳定 5.1.1。
- 记录每项 5.2.0 补丁的原因、适用文件、与 5.1.1 的差异及 Linux 影响。

**完成标准**

- 5.2.0 可在 macOS arm64 的项目隔离环境中配置、构建和安装，且不需要 `sudo`。
- CLI、RAW 和真实相机验证均有实际结果；未能执行的硬件检查不得标记通过。
- `/usr/local` 中 5.1.1 的命令、头文件、动态库、CMake package 和 HAL plugin 保持不变。
- 最小补丁集可独立审核，并保留 Linux 路径。
- 记录 milestone 开始前的空间预算和结束后的实际增长，且没有不必要的重复 OpenEB build tree。

### Milestone 3: EBplus CMake configuration

**状态：** `Planned`
**独立分支：** `build/macos-cmake-configuration`

**范围**

- 让顶层 CMake 显式找到项目隔离安装的 OpenEB 5.2.0。
- 支持 macOS Apple Silicon / arm64 和严格的 out-of-source build。
- 处理 `.dylib`、build/install RPATH、Qt Cocoa、OpenCV、ONNX Runtime 及其他模型依赖发现。
- 移除对某台机器路径的隐式依赖，优先接受用户传入的 prefix 或 CMake cache 变量。
- 保持 Linux 的 Qt、OpenEB、OpenCV、ONNX Runtime 和 `.so` 路径正常工作。

**检查方法**

- 在全新的 `$REPO_ROOT/.build/ebplus-macos` 中执行 macOS configure，检查架构、依赖版本和所有解析路径。
- 检查 `CMakeCache.txt`，确认 Metavision/OpenEB 来自项目内 5.2.0 prefix，而不是 `/usr/local`。
- 构建 EBplus，并用 `otool -L` 检查 Metavision、Qt、OpenCV 和模型运行时链接。
- 验证 in-source build 会被明确阻止或文档化拒绝。
- 在 Linux 执行对应 configure/build 回归，确认原有发现路径和目标不变。

**完成标准**

- macOS arm64 可从干净 build directory 重复 configure 和 build。
- 所有依赖来源和运行时链接可解释、可覆盖且不依赖永久全局环境变量。
- EBplus 明确链接项目隔离的 OpenEB 5.2.0，Linux 构建路径保持可用。
- 构建产物、cache 和安装树均未提交。

### Milestone 4: GUI launch

**状态：** `Planned`
**独立分支：** `feat/macos-gui-launch`

**范围**

- 使 GUI 能在 macOS 启动，并正常显示基本窗口、菜单、面板和渲染区域。
- macOS 使用 Qt Cocoa 平台插件和适合的图形后端。
- Linux 的 X11、XCB、Wayland、`LD_LIBRARY_PATH` 和 `.so` 逻辑仅在 Linux 路径启用。
- 通过 `if(APPLE)`、`#ifdef __APPLE__`、macOS 专用脚本或平台适配层集中处理差异。
- 保持共享 GUI 业务行为，不在各页面散布平台判断。

**检查方法**

- 在 macOS 从隔离环境启动 GUI，检查进程退出码、日志、主窗口、菜单、布局和基本交互。
- 检查 Qt 实际使用 Cocoa，且未强制使用 Linux 的 XCB/Wayland 设置。
- 检查动态库、HAL 和资源加载路径，不依赖个人绝对路径或全局 shell 配置。
- 在 Linux 运行 GUI launch smoke test，确认原有 XCB/Wayland 兼容行为仍然有效。

**完成标准**

- macOS GUI 可重复启动和正常关闭，无缺失动态库、平台插件或资源错误。
- 基本窗口、菜单和渲染可用，启动日志不包含错误平台路径。
- Linux 与 macOS 启动逻辑边界清晰，Linux 行为无回归。

### Milestone 5: RAW playback parity

**状态：** `Planned`
**独立分支：** `feat/macos-raw-playback`

**范围**

- 在 macOS 支持打开 RAW、播放、暂停、继续、跳转和结束处理。
- 验证元数据解析、事件可视化、时间轴、状态显示和文件错误处理。
- 验证 RAW 数据进入算法处理和导出路径。
- 使用同一输入与 Linux 当前行为进行对照，记录允许的展示差异和不可接受的数据差异。

**检查方法**

- 使用已知有效、空文件、截断文件和不支持格式等样本分别测试文件打开与错误提示。
- 分别验证 metadata、playback、pause/resume、seek、end-of-file、algorithm processing 和 export。
- 比较 Linux/macOS 的时长、事件数量、时间戳范围、关键状态变化和算法/导出结果。
- 重复打开、关闭和切换文件，检查资源释放、崩溃、死锁和状态残留。

**完成标准**

- RAW 核心工作流在 macOS 可用，并与记录的 Linux 基线对齐。
- 正常文件和错误文件均有确定、可复现的处理结果。
- 算法与导出不是仅能触发 UI，而是有输出证据和对照结果。
- 所有不一致均已修复或作为明确限制记录。

### Milestone 6: Live camera parity

**状态：** `Planned`
**独立分支：** `feat/macos-live-camera`

**范围**

- 在 macOS 发现并打开真实相机，显示实时事件流。
- 验证 HAL plugin 加载、facility 访问和 Linux 基线支持的相机参数控制。
- 支持正确关闭、再次打开、重连和异常断开处理。
- 保持 Linux 相机路径、插件加载和厂商配置行为。

**检查方法**

- 分别验证 device enumeration、device open、live event stream、facility access、parameter changes、clean shutdown 和 reconnect。
- 检查无设备、权限不足、插件缺失、设备占用和传输中断等失败路径。
- 多次执行打开/关闭/重连，检查线程、回调、句柄、设备状态和 GUI 状态恢复。
- 在 Linux 使用受支持相机执行对应回归，比较功能、参数范围和错误行为。

**完成标准**

- 至少一台目标真实相机在 macOS 上完成完整枚举、打开、流式显示、控制、关闭和重连周期。
- HAL 插件和依赖来自隔离的 OpenEB 5.2.0 环境，没有回退到稳定 5.1.1。
- 断开和失败路径不会造成崩溃、挂起或不可恢复状态。
- Linux 相机功能无回归；未覆盖的设备型号明确列为限制。

### Milestone 7: Algorithms, models and export

**状态：** `Planned`
**独立分支：** `feat/macos-algorithms-models-export`

**范围**

- 验证 Milestone 1 盘点出的全部 Linux 算法在 macOS 的可用性和输出。
- 验证模型发现、加载、推理、错误处理和已定义的回退模式。
- 验证 CSV、视频、HDF5 及 Linux 基线中发现的其他导出格式。
- 比较 Linux 与 macOS 的算法和导出结果，定义数值容差或格式一致性要求。
- 记录暂不支持的功能及原因，不通过静默降级掩盖缺失能力。

**检查方法**

- 对每个算法使用固定 RAW/事件输入，记录参数、输出摘要、性能和失败信息。
- 对需要模型的路径分别测试模型存在、缺失、损坏和版本不匹配。
- 检查是否实际启用 ONNX Runtime；将真实模型推理与启发式回退分别报告。
- 对每种导出检查文件创建、元数据、记录数量、时间戳、可重新读取性和错误清理。
- 在 Linux/macOS 比较确定性输出；浮点结果使用预先记录的合理容差。

**完成标准**

- Linux 基线中的每个算法、模型和导出项都有 macOS 结果及对照结论。
- 支持项通过 smoke test 和结果校验，不能只以按钮可点击或进程未崩溃为依据。
- 不支持项有清晰提示、文档和后续处理建议。
- Linux 的算法选择、结果、模型回退和导出格式无回归。

### Milestone 8: Packaging and CI

**状态：** `Planned`
**独立分支：** `build/macos-packaging-ci`

**范围**

- 生成可运行的 macOS `.app` bundle。
- 收集并验证 Qt、OpenEB、OpenCV、模型运行时、HAL plugin 和其他运行时依赖及 RPATH。
- 根据需要提供 DMG；代码签名和 notarization 作为后续可选工作，不作为初始移植阻塞项。
- 增加 Apple Silicon CI，同时保持 Linux CI 正常。
- 记录打包输入、产物边界、运行环境、已知限制和回滚方法。

**检查方法**

- 在干净环境检查 `.app` 结构、资源、插件和 `otool -L`/RPATH 结果。
- 从 Finder 和终端分别启动，执行 GUI、RAW、算法和无需硬件的导出 smoke test。
- 在可用硬件环境验证打包应用的设备发现和实时相机路径。
- CI 执行 macOS arm64 configure/build/tests/packaging smoke test，并持续执行 Linux configure/build/tests。
- 若制作 DMG，验证安装、首次启动、升级/覆盖和卸载说明；签名状态必须如实标注。

**完成标准**

- `.app` 在规定的 macOS arm64 环境可独立启动，运行时依赖不指向开发者个人目录。
- CI 能阻止 macOS 构建回归，同时 Linux job 保持通过。
- 打包文档列出支持范围、硬件限制、签名/notarization 状态和可复现命令。
- 仅在对应功能 milestone 已完成后，才可对外声明相应 macOS 能力。

## 跨平台实施原则

1. macOS 支持不能通过删除 Linux 分支或使 Linux 分支失效实现。
2. Linux 专用逻辑保留在 Linux 条件中，macOS 专用逻辑放在 `if(APPLE)`、`#ifdef __APPLE__`、专用脚本或平台适配层。
3. 公共业务行为保持共享，避免在业务代码中到处散布平台判断。
4. 禁止在代码、CMake、脚本和运行配置中写死个人或机器相关绝对路径，例如 `/Users/<username>/...`、固定的包管理器前缀或系统安装前缀。
5. 当前真实路径可以出现在版本隔离审计文档中；可复用命令优先使用 `$PWD`、`$REPO_ROOT`、CMake cache 变量或用户显式 prefix。
6. CMake 优先接受用户显式传入的 prefix，不暗中选择某台机器的安装目录。
7. 不依赖永久全局 `PATH`、`CMAKE_PREFIX_PATH`、`DYLD_LIBRARY_PATH` 或 HAL plugin path。
8. 不提交二进制、build tree、`.app`、`.dSYM`、模型缓存或安装目录。
9. 不随意升级 Qt、OpenCV、OpenEB、模型运行时或其他依赖。
10. 当前目标是 OpenEB / Metavision SDK 5.2.0，不为了快速通过 CMake 降为 5.1.1。
11. 5.1.1 的 macOS 补丁只能逐项评估后移植，不假设可原样应用到 5.2.0。
12. 每项平台差异必须通过代码注释、变更说明或开发文档解释原因和适用范围。
13. 项目主动创建的 build、install、dependency、venv、cache、temporary、log、download、test output、export 和 packaging artifact 必须位于 `$REPO_ROOT` 的标准目录中；不得使用仓库外项目目录。
14. `$REPO_ROOT/.git/` 虽位于仓库边界内，但仅供 Git 自身管理，不得存放项目构建产物、缓存、日志、下载或临时文件。
15. 每个 milestone 开始前必须检查磁盘并报告预计增长，结束后记录实际增长；预计新增达到 `1 GiB` 或无法估算时必须先获得授权。
16. 如果预计完成后可用空间低于 `20 GiB` 或磁盘总容量的 `15%`，按更严格者暂停并请求授权；不得把系统可能自动释放的空间计入预算。
17. 默认只保留当前 milestone 所需的单一构建配置，不并存无必要的 Debug、Release、RelWithDebInfo、universal binary 或重复依赖树；清理任何现有内容前仍须用户明确授权。

## 测试与报告原则

每个 milestone 应按变更范围考虑并记录以下检查：

```text
Configure test
Build test
GUI launch smoke test
RAW playback smoke test
Live camera smoke test
Algorithm smoke test
Export smoke test
Linux regression check
Dynamic library linkage check
```

相机检查必须拆分为：

```text
Device enumeration
Device open
Live event stream
Facility access
Parameter changes
Clean shutdown
Reconnect
```

RAW 检查必须拆分为：

```text
File open
Metadata parsing
Playback
Pause/resume
Seek
End-of-file handling
Algorithm processing
Export
```

所有测试报告必须区分 `Passed`、`Failed`、`Not run` 和 `Blocked`，并记录平台、架构、命令、依赖、输入和实际结果。没有目标平台、真实相机、RAW 样本或模型时，应明确写出限制，不得推断通过。每个 milestone 开始前还必须记录空间预算和重复构建风险，结束后记录仓库大小、各生成目录大小及实际增长。完成一个 milestone 后，应在本文档更新状态，并在任务报告中列出实际检查和未执行检查。

## 已知风险与后续审计项

- 仓库已有 `doc/`，本路线及版本隔离文档按要求位于新增的 `docs/`；两个目录暂时并存，本轮不重命名旧目录。
- `README.md` 与 `README_CN.md` 写明 Ubuntu 22.04+，而 `doc/compile.md` 记录 Ubuntu 26.04；Milestone 1 应核实支持基线，本轮不修改既有 Linux 文档。
- `doc/compile.md` 的直接运行示例使用 `./build/gui_for_openeb`，而 `run.sh` 和当前 CMake 目标布局指向 `build/gui/gui_for_openeb`；Milestone 1 应核实并记录正确入口，本轮不顺带修复。
- 根仓库跟踪 `$REPO_ROOT/openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf` gitlink，但根级缺少对应 `.gitmodules` mapping；当前从根运行 `git submodule status` 会失败。Milestone 2 开始前必须审计并明确修复策略。
- `run.sh`、`gui/main.cpp` 和 `algo/CMakeLists.txt` 存在 Linux 系统路径、动态库和显示后端假设；它们是后续平台隔离热点，本轮仅记录，不修改。
- 现有 Linux README、`run.sh` 和 `doc/compile.md` 仍包含传统 `build/`、系统 `/tmp` 或 `/usr/local` 流程。这些是需要后续协调的 legacy baseline，本轮保持原文以避免改变 Linux 指引，但不构成未来任务绕过仓库内工作区规范的授权。
- OpenEB 5.1.1 是已验证的稳定 macOS 环境，但 EBplus 的目标版本是 5.2.0；任何比较结果都不能成为覆盖 5.1.1 或将 EBplus 降级到 5.1.1 的理由。
