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

**状态：** `Complete`
**Milestone 2A：** `Complete`
**Milestone 2A 分支：** `build/macos-openeb-5.2-audit`
**Milestone 2B prerequisite：** `Complete`
**Milestone 2B prerequisite 分支：** `build/macos-openeb-5.2-hdf5-dependency`
**Milestone 2B：** `Complete`
**Milestone 2B base build：** `Complete`
**Milestone 2B RPATH portability：** `Complete`
**Milestone 2B 分支：** `build/macos-openeb-5.2-isolation`
**Milestone 2C source audit/design：** `Complete`
**Milestone 2C 审计分支：** `audit/centuryarks-openeb-5x-integration`
**Milestone 2C implementation/build：** `Complete`
**Milestone 2C Phase 1 side-by-side plugin：** `Complete`
**Milestone 2C bounded live-event validation：** `Complete`
**Milestone 2C PID 0003 enumeration/open/reopen：** `Verified`
**Milestone 2C PID 0003 bounded live CD event delivery：** `Verified`
**Milestone 2C PID 0002 hardware：** `Not tested`
**Milestone 2C PID 0004 hardware：** `Not tested`

**Milestone 2A 输出**

- [`openeb_5_2_macos_build_audit.md`](openeb_5_2_macos_build_audit.md)：OpenEB 5.2 源码身份、HDF5 gitlink、构建选项、5.1.1 patch、系统依赖和磁盘预算审计。
- [`openeb_5_2_macos_build_command_draft.md`](openeb_5_2_macos_build_command_draft.md)：已验证的仓库内隔离构建、安装、无 DYLD 运行和 RPATH 检查命令；未来执行仍需重新通过 preflight。

**Milestone 2B validation 输出**

- [`openeb_5_2_macos_build_validation.md`](openeb_5_2_macos_build_validation.md)：base build、Apple-only RPATH 修复、RAW/HDF5、discovery smoke、动态链接和剩余范围记录。

**Milestone 2C 输出**

- [`centuryarks_openeb_5x_source_audit.md`](centuryarks_openeb_5x_source_audit.md)：CenturyArks 包身份、许可证、5.1.1/5.2.0 逐文件及逐 hunk 映射，以及 Phase 1 适用范围。
- [`centuryarks_openeb_5_2_integration_plan.md`](centuryarks_openeb_5_2_integration_plan.md)：Architecture B ignored source copy、始终构建的并列 CenturyArks/Prophesee plugins、单一 profile 和分阶段验收边界。
- [`centuryarks_openeb_5_2_overlay_build.md`](centuryarks_openeb_5_2_overlay_build.md)：prepared source、configure/build/install、动态链接、文件回归和单台 `31f7:0003` 相机 enumeration/open/reopen 的 Phase 1 本地验证记录。
- [`centuryarks_openeb_5_2_live_event_validation.md`](centuryarks_openeb_5_2_live_event_validation.md)：项目自有 bounded headless probe、显式 runtime selector、两次 CD event delivery/start/stop 和运行前后只读系统字段对比。

**Milestone 2A 完成标准**

- 已通过根仓库 Git 证据确认 `openeb/` 的版本、源码形态、dirty 状态、导入历史和 EBplus 集成方式。
- 已确认 HDF5 ECF gitlink 的锁定提交和上游声明来源，并记录根 mapping/工作树内容缺失对 HDF5-enabled configure 的影响。
- 已建立 OpenEB 5.2 CMake 选项、built-in CLI 约束、5.1.1 macOS patch 映射、现有依赖及三档空间预算。
- 本阶段未执行 configure、build、install、下载、submodule init/update 或程序运行，且未修改 OpenEB、Linux 行为或稳定 5.1.1 环境。

**Milestone 2B prerequisite 输出**

- [`hdf5_ecf_dependency_recovery.md`](hdf5_ecf_dependency_recovery.md)：根级 submodule mapping、锁定提交恢复、Git metadata 边界和静态 fresh-clone 配置验证记录。

**Milestone 2 当前结果**

HDF5 ECF source prerequisite 已恢复。Milestone 2B/OpenEB 5.2 primary profile 已完成 macOS arm64 configure、bootstrap/full build、repository-local install，以及无 `DYLD_LIBRARY_PATH` 的三项 CLI、RAW、HDF5 ECF 和无设备 discovery smoke。Apple-only target-specific `INSTALL_RPATH` 修复已通过独立 build/install tree 验证，未发现稳定 5.1.1 runtime 污染。

Milestone 2C Phase 1 已在 ignored prepared source 中完成始终构建的 `hal_plugin_prophesee` 与 `hal_plugin_centuryarks` 并列 topology；canonical tracked `openeb/` 保持不变。单一 OpenEB 5.2 macOS arm64 profile 的 configure、bootstrap/full build 和 repository-local install 已通过；两个 plugin、共享 `metavision_psee_hw_layer`、object ownership、registration、Apple RPATH 和 5.1.1 contamination 审核已通过。三个基础 CLI 以及 RAW/HDF5 回归均在无 `DYLD_*` 环境下通过。

一台 `31f7:0003` CenturyArks 相机已在新的 OpenEB 5.2 prefix 中通过 `--short`、`--system` 和正常退出后的单进程 reopen，设备由 `hal_plugin_centuryarks` 打开并报告 CenturyArks integrator。项目自有 headless probe 随后使用显式 runtime selector 完成一次 5 秒和一次 3 秒 bounded live CD event smoke；两次均收到事件、正常 start/stop、无 runtime error，并在运行前后保持可见系统配置字段不变。`31f7:0002` 与 `31f7:0004` 仅按供应源码注册，尚无硬件验证。

Milestone 2 的 `Complete` 结论仅覆盖：隔离的 OpenEB 5.2 build/install、CLI 与 RAW/HDF5 regression、CenturyArks side-by-side plugin、单台 PID `0003` enumeration/open/reopen，以及 bounded CD event delivery。

EBplus GUI live display、facility 和参数 parity、物理 disconnect/reconnect、多相机 operation、EEPROM/pixel masks、PID `0002`/`0004` 硬件以及 Linux runtime regression 不属于 Milestone 2 完成声明；这些能力继续进入 Milestone 3、Milestone 6 或明确 backlog。持续稳定性、事件正确性/图像质量和吞吐/性能同样未由本 milestone 验证，不得据此宣称完整 CenturyArks 相机支持。

**范围**

- 构建仓库自带的 OpenEB / Metavision SDK 5.2.0。
- 使用项目内唯一的标准 `Release`/arm64 build tree 和独立 install prefix，不影响 `/usr/local` 中稳定的 5.1.1。
- 当前经验证的 EBplus producer 是 CenturyArks prepared profile：source 为
  `$REPO_ROOT/.tmp/openeb-5.2.0-centuryarks-source`，build 为
  `$REPO_ROOT/.build/openeb-5.2.0-centuryarks-macos`，install 为
  `$REPO_ROOT/.deps/openeb-5.2.0-centuryarks-macos`。历史 generic M2B
  base/RPATH profile 仅保留为历史验证证据；不得并行重建两个完整 profile，亦不得
  创建仓库外或不必要的重复 build tree。
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

**状态：** `Complete — macOS arm64 validated; native Linux regression not run, risk accepted by maintainer.`
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
- 原计划在 Linux 执行对应 configure/build/CTest 回归，确认原有发现路径和目标不变。

**完成标准**

- macOS arm64 可从干净 build directory 重复 configure 和 build。
- 所有依赖来源和运行时链接可解释、可覆盖且不依赖永久全局环境变量。
- EBplus 明确链接项目隔离的 OpenEB 5.2.0；Linux 分支和默认值仅通过静态检查确认保留。
- 构建产物、cache 和安装树均未提交。

**完成证据**

- fresh Release/arm64 configure 和 clean complete build 通过。
- 16/16 test/diagnostic binaries 均为 non-fat arm64；9/9 GTest discovery targets 通过。
- 完整 CTest 以并行度 2 运行，295/295 tests 通过且无 test artifact 残留。
- repository-local OpenEB 5.2 header/library provenance 通过，未发现 5.1.1 污染。
- repository-local install 通过；manifest 仅包含 `bin/gui_for_openeb`。
- installed `LC_RPATH` 精确为 `@executable_path/../lib`。
- 精确清理 canonical build/install trees 后，fresh configure/build/CTest/install 可重复通过。
- 完整证据见 [Milestone 3 macOS CMake Configuration Validation](macos_milestone_3_validation.md)。

**已接受的验证缺口**

The originally planned native Linux configure/build/CTest regression was not
executed. The maintainer accepted this remaining validation risk when closing
M3. Linux-specific `$ORIGIN` install RPATH、ONNX search roots、default test
behavior 和 launcher behavior 仅完成 source-level static inspection，不能据此
声明 Linux runtime 已验证。

### Milestone 4: GUI launch

**状态：** Complete — macOS Apple Silicon idle Cocoa launch, interaction,
resize, theme persistence, and clean shutdown validated; native Linux
compilation and Linux GUI launch regression were not run, with the residual
risk explicitly accepted by the maintainer on 2026-07-22.
**独立分支：** `feat/macos-gui-launch`

- **M4A-1：** Passed — macOS startup environment isolation.
- **M4A-2：** Passed on rerun 1 — macOS Cocoa idle GUI runtime validation.
- **M4B：** Not run — residual Linux regression risk accepted by maintainer.

**验证记录：** [macOS Milestone 4A Validation](macos_milestone_4a_validation.md)

**范围**

- 使 GUI 能在 macOS 启动，并正常显示基本窗口、菜单、面板和渲染区域。
- macOS 使用 Qt Cocoa 平台插件和适合的图形后端。
- Linux 的 X11、XCB、Wayland、`LD_LIBRARY_PATH` 和 `.so` 逻辑仅在 Linux 路径启用。
- 通过 `if(APPLE)`、`#ifdef __APPLE__`、macOS 专用脚本或平台适配层集中处理差异。
- 保持共享 GUI 业务行为，不在各页面散布平台判断。
- M4A 不包括 RAW、camera/facility、algorithm/model/export、portable dependency
  closure、.app bundle 或 packaging。

**检查方法**

- 在 macOS 从隔离环境启动 GUI，检查进程退出码、日志、主窗口、菜单、布局和基本交互。
- 检查 Qt 实际使用 Cocoa，且未强制使用 Linux 的 XCB/Wayland 设置。
- 检查动态库、HAL 和资源加载路径，不依赖个人绝对路径或全局 shell 配置。
- 在 Linux 运行 GUI launch smoke test，确认原有 XCB/Wayland 兼容行为仍然有效。

**完成标准**

- macOS GUI 可重复启动和正常关闭，无缺失动态库、平台插件或资源错误。
- 基本窗口、菜单和渲染可用，启动日志不包含错误平台路径。
- Linux 与 macOS 启动逻辑边界清晰，Linux 行为无回归。
- **Linux criterion status:** Not run; explicitly waived for milestone closure by maintainer risk
  acceptance on 2026-07-22. This is a maintainer closure decision, not Linux
  validation evidence; Linux parity remains unverified.

**维护者风险接受与关闭决定（2026-07-22）**

macOS Apple Silicon idle Cocoa launch, interaction, resize, theme persistence
and clean shutdown 已完成验证。Native Linux compilation 和 Linux
XCB/Wayland GUI launch regression 均未运行。源码静态审核和 macOS
纯策略测试表明既有 Linux environment defaults 被保留，但这不是 Linux 原生
build 或 runtime 证据。维护者明确接受剩余 Linux regression risk，并以该
决定关闭 Milestone 4；Linux parity remains unverified。

### Milestone 5: RAW playback parity

**状态：** `Complete — Linux comparison gap explicitly accepted by maintainer`
**独立分支：** `feat/macos-raw-playback`

**M5 closure summary：** [macOS Milestone 5 Validation](macos_milestone_5_validation.md)

**窄范围 macOS evidence（2026-07-22）：** [macOS RAW core playback validation](macos_raw_core_playback_validation.md) 记录了 arm64 build-tree executable 在两个 Terminal.app/Aqua sessions 中打开 tracked <code>algo/tests/sparklers.raw</code> 的结果：RAW open、非空 event display、duration/position observation、pause/resume、forward/backward seek、natural EOF、EOF recovery、reopen、close-button exit 和 Cmd+Q exit 均在该单一样本范围内通过。该验证还观察到 progress indicator circular handle 局部裁切这一 deferred visual defect。

**2026-07-23 transport-controls evidence：** [macOS RAW transport controls validation](macos_raw_transport_controls_validation.md) 记录了同一 tracked <code>algo/tests/sparklers.raw</code> 在一个 Terminal.app/Aqua session 中的 single 和 repeated Step、可观察的 Window/Rate/multiplier linkage、x0.5/x1/x2 的相对推进顺序、一次 Loop wrap、关闭 Loop 后 natural EOF、near-start 和 near-EOF seek，以及 close-button exit <code>0</code>。exact control values 和 numerical position deltas 未记录；exact timing、conversion formulas、multiple-loop 和 stress behavior 仍未验证。

**2026-07-24 RAW file lifecycle robustness fix and post-fix validation：** [macOS RAW file lifecycle robustness validation](macos_raw_file_lifecycle_validation.md) 记录了 build-tree arm64 / repository-local OpenEB 5.2 CenturyArks 的两次 Terminal.app/Aqua sessions：empty RAW 的 pre-fix SIGABRT 已修复；unsupported 和 empty file-open failures 各产生一个受控详细错误对话框，随后均可恢复有效 RAW；same-file reopen 自动恢复播放；different-path switch、Recent reopen，以及 unique stale Recent 的缺失文件警告和移除均通过；Run 1 和 Run 2 均 exit <code>0</code>。generated truncated RAW 的 exact outcome 未精确分类；permission/plugin failure、HDF5/H5/DAT、different geometry 和 Linux 仍未验证。

**2026-07-26 HDF5/H5 generic-offline compatibility fix and validation：** [macOS HDF5/H5 generic offline validation](macos_hdf5_h5_generic_offline_validation.md) 记录了 build-tree arm64 / repository-local OpenEB 5.2 CenturyArks 的 bounded HDF5/H5 ECF validation：pre-fix HDF5/H5 generic-offline GUI path reproduced <code>DeviceUnavailable</code> 102113，因为 hardware-facility lookup 针对无 HAL Device 的 generic offline source；shared platform-neutral facility-degradation fix 和 partial <code>connect_file()</code> setup rollback 已实施。incremental build、focused lifecycle 1/1 和 full CTest 309/309 均通过；fresh <code>.hdf5</code>/<code>.h5</code> CLI ECF fixture validation 通过；HDF5 GUI open/display/autoplay/pause/resume/seek、HDF5→H5 switch、Recent reopen、natural EOF responsiveness 及 exit <code>0</code> 均通过。DAT 未运行；other HDF5/H5 cases 和 Linux 仍未验证。

**2026-07-27 DAT playback validation：** [macOS DAT playback validation](macos_dat_playback_validation.md) 记录了 canonical build-tree OpenEB DAT converter 对 isolated working RAW 的结果：converter outputs 与 expected RAW index sidecar 均保持在 isolated repository-local sample root；one generated CD DAT CLI validation、DAT GUI open/display/autoplay、pause/resume、forward/back seek smoke、Recent reopen、EOF/recovery 和 process exit <code>0</code> 均通过。generic-offline <code>DeviceUnavailable</code> 102113 在该 DAT session 中未复现。trigger DAT 为零字节并保留但未作为 playback media；其他 DAT、different geometry、large/corrupt DAT 和 Linux 仍未验证。

**2026-07-28 HDF5 export metadata round-trip fix and validation：** [macOS HDF5 Export Metadata Round-trip Validation](macos_hdf5_export_round_trip_validation.md) 记录了 pre-fix HDF5 export reopen 的 null-geometry <code>SIGSEGV</code>，以及 shared platform-neutral metadata preservation 修复。Release/arm64 repository-local OpenEB 5.2 build-tree 的 focused regression 1/1 和 full CTest 310/310 通过；fresh tracked RAW → HDF5 GUI export/reopen/autoplay/pause/resume/seek 通过，GUI exit <code>0</code>。source/output CLI 的 CD events、first/last timestamp、duration 和 generation 一致。外部缺 geometry HDF5 的 OpenEB reader robustness、AVI、ExtTrigger、cancel、large-file、algorithm/model 和 Linux 仍未验证。

**2026-07-28 Time Surface algorithm lifecycle validation：** [macOS Time Surface algorithm validation](macos_time_surface_algorithm_validation.md) 记录了一个 representative non-model algorithm 在 macOS arm64 上对一个 tracked RAW fixture 的受限 smoke：RAW input、visibly distinct dynamic output、pause/resume、bidirectional seek recovery、same-source reopen/reset 和 clean exit 均通过。该会话使用默认参数；seek 后曾观察到数帧 transient white output，随后动态输出恢复。未测量 reset latency 或数值正确性。

**2026-07-28 paused seek immediate-render fix and validation：** [macOS paused seek immediate-render validation](macos_paused_seek_immediate_render_validation.md) 记录了 pre-fix paused timeline seek 只更新 position、而显示画面直到 Resume 才变化的 defect。shared `QSlider::valueChanged` fix 与 open-file initialization signal blocker 后，focused regression 1/1 和 full CTest 311/311 通过；one tracked RAW/ROI session 的 paused forward/back seek 均在未 Resume 时立即更新显示，ROI enable/disable、resume、same-file reopen 与 exit <code>0</code> 也通过。该结果不测量 pixel/event 数值正确性，且不覆盖 other controls、other filters、long stability 或 Linux。

**2026-08-10 disconnect/reopen and different-geometry source-switch evidence：** [macOS Milestone 5 Validation](macos_milestone_5_validation.md) 记录了 build-tree GUI 的 `Camera → Devices → Disconnect` 后 reopen tracked <code>algo/tests/sparklers.raw</code>，以及同一 session 中 640x480 tracked RAW → 320x240 synthetic OpenEB 5.2 EVT2 → 640x480 tracked RAW 的双向切换。display geometry、duration/position、画面、pause/resume 和 paused seek 均按当前 source 恢复；GUI exit <code>0</code>，fatal-marker scan clean。synthetic B 只作为 geometry/lifecycle evidence，不代表第二种真实 sensor recording。

**M5 closure boundary：** Milestone 5 完成为 bounded macOS arm64 file-source lifecycle、representative FilterChain/algorithm behavior、HDF5 export round-trip、disconnect/reopen 和不同 geometry source switching 的验证结论。broader algorithms/models/AVI/general algorithm-result export 属于 Milestone 7；physical camera、facilities 和 live workflows 属于 Milestone 6；broader file corpus、failure/stress/performance/memory-safety work 保持 backlog。native Linux compile/runtime/export comparison 未运行；维护者明确接受该 M5 closure risk。此接受不构成 Linux 验证证据，Linux 仍为 <code>Not run / unverified</code>。

**范围**

- 在 macOS 支持打开 RAW、播放、暂停、继续、跳转和结束处理。
- 验证元数据解析、事件可视化、时间轴、状态显示和文件错误处理。
- 验证 RAW 数据进入算法处理和导出路径。
- 原计划使用同一输入与 Linux 当前行为进行对照。native Linux compile/runtime/export comparison 未运行；维护者仅为 M5 closure 明确接受该剩余风险。该接受不是 Linux validation evidence，Linux 仍为 <code>Not run / unverified</code>。

**检查方法**

- 使用已知有效、空文件、截断文件和不支持格式等样本分别测试文件打开与错误提示。
- 分别验证 metadata、playback、pause/resume、seek、end-of-file、algorithm processing 和 export。
- Linux/macOS 对照未运行，且不得将维护者风险接受写成 Linux 结果；未来跨平台结果对照保留在 Milestone 6/7 和 backlog。
- 重复打开、关闭和切换文件，检查资源释放、崩溃、死锁和状态残留。

**完成标准**

- bounded macOS arm64 RAW/HDF5/H5/DAT file-source lifecycle、代表性 unified
  ROI、FilterChain、Time Surface 和 RAW-to-HDF5 round-trip 均有实际 evidence；
  disconnect/reopen 与两种 geometry 的 A→B→A source switch 也已验证。
- 已复现的 empty RAW、same-file reopen、generic-offline DeviceUnavailable、HDF5 export null-geometry 和 paused seek presentation defects 均已有相应修复与有界验证；未验证事项保留为明确限制。
- broader algorithms/models/AVI/general algorithm-result export、physical camera/live workflows、broader corpus 和 stress/performance/memory-safety 不因本 closure 被写成 passed，分别移交 Milestone 6、Milestone 7 或 backlog。
- **Linux criterion status:** native Linux compilation、file-source runtime 和 export comparison 未运行；维护者明确接受该 M5 closure risk。此为管理性关闭决定，不是 Linux validation evidence；Linux remains <code>Not run / unverified</code>。

### Frozen upstream baseline integration checkpoint

**状态：** `Qualified on the frozen active integration tree; Git closure remains separate.`

The candidate integrates fork baseline
`d01f1c1a632dece8be10618d2212d6c3f76aeb23` with frozen upstream integration
baseline `f72fdf750ab82c09eb1d11ba828a4ac0601a2ea9`, using merge base
`e0439b79f4b272f249cb096f8daf7f73824ca788`. The latter is a pinned/frozen
integration baseline, **not** a statement about current live `upstream/main`.

The active merge reconciliation, Release/arm64 configure and full build,
focused CTest `42/42`, full CTest `341/341`, Mach-O/OpenEB 5.2 CenturyArks
provenance, and representative post-integration M5 GUI/file/export
requalification are recorded in [macOS frozen upstream baseline integration
validation](macos_upstream_baseline_integration_validation.md). The evidence
belongs to this frozen candidate source state and does not rewrite historical
M0–M5 validation records.

**Linux integration configure/build/runtime regression: Not run / unverified.**
The maintainer accepts this residual risk for the frozen integration closure
only. **Risk acceptance is not Linux validation evidence.** It does not extend
to M6, M7 or M8. Any later upstream movement is a separate synchronization
scope and does not automatically reopen or change this pinned integration.

### Milestone 6: Live camera parity

**状态：** `Planned / Paused — physical CenturyArks camera currently unavailable`
**独立分支：** `feat/macos-live-camera`

Milestone 6 covers the current integrated source's live-camera and facility
lifecycle. Source wiring exists, but no current macOS EBplus GUI physical-camera
evidence is implied by that fact or by OpenEB-only camera evidence.

M6 is paused because the required physical CenturyArks camera is currently
unavailable. This is a temporary hardware dependency, not a failed, cancelled
or complete milestone. M7 Slice 1 configuration evidence does not substitute
for any M6 device, facility, live-stream, recording or reconnect evidence.

#### M6-A: basic live lifecycle

**范围与最小验收**

```text
Devices Refresh / enumerate
-> explicit selected-device open (do not use "Connect first" as acceptance)
-> visibly non-empty dynamic live display and responsive GUI
-> manual Disconnect
-> select and reopen the same device
-> bounded second live observation
-> clean quit
```

The runtime must use repository-local OpenEB 5.2 CenturyArks provenance with no
`/usr/local` 5.1.1 fallback. It requires one authorized, visible supported
device, an explicit selector without full serial disclosure, a bounded GUI
session and fresh repository-local logs. A previously qualified build may be
reused only while source and dependency provenance remain unchanged; otherwise
configure/build qualification comes first. M6-A does not need a file fixture,
recording, model or ONNX dependency.

**明确排除**

- bias、ROI/RONI、ESP/ERC 或 trigger 写入；
- sensor self-test、calibration、RAW/processed recording、algorithms/models；
- physical unplug/replug、automatic reconnect、多相机和 live↔file switching。

manual Disconnect → reopen is not evidence for physical loss/manual recovery
or automatic reconnect. Later independently authorized M6 slices cover
facility inventory and controlled mutation, live RAW/processed recording
lifecycle, physical-loss/manual recovery, product-defined automatic reconnect,
and live↔file switching. Linux M6 criteria remain a future maintainer decision;
the frozen integration Linux-risk acceptance does not extend to M6.

**M6-A 完成标准**

- selected device is enumerated/opened through the repository-local OpenEB
  5.2 profile and live display is visibly dynamic;
- manual Disconnect clears the active state without crash/hang, and the same
  selected device streams again after reopen;
- normal quit has bounded clean exit/no scoped fatal marker; and
- facility/UI presence may be observed read-only, but no mutation or
  device-capability claim is made.

### Milestone 7: Algorithms, models and export

**状态：** `In progress` — Slices 1, 2, and 3A are `Complete / Qualified`;
M7 Slice 3 and Milestone 7 overall are not complete.
**Slice 1 branch：** `feat/macos-config-persistence-contract`
**Slice 2 branch：** `feat/macos-file-algorithm-qualification`
**Slice 3A branch：** `feat/macos-e2vid-fallback-qualification`

**当前 source baseline**

The current registry is `33 = 26 self-developed (19 CV + 7 analytics) + 7
OpenEB FilterChain transforms`:
`polarity_filter`, `polarity_invert`, `flip_x`, `flip_y`, `rotate`,
`transpose`, and `rescale`. The unified ROI is separate from FilterChain;
file playback uses a software crop while live ROI/RONI uses the hardware
`I_ROI` facility. `sensor_self_test` is a Devices hardware diagnostic and
intrinsic calibration is a Tools/CalibrationWizard workflow, not registry
entries. Source existence or registry count is not a macOS runtime-completion
metric.

**独立可验收 slices**

1. **Complete / Qualified:** current catalog/config persistence and migration;
   see [macOS Milestone 7 Slice 1 Config Persistence Validation](macos_milestone_7_config_persistence_validation.md).
2. **Complete / Qualified:** deterministic file-source preprocessing and
   category-based representative algorithm lifecycle/numerical evidence,
   including nine shared noise modes, KNoise, Arc and current Time Surface
   modes; see [macOS Milestone 7 Slice 2 File-Source Algorithm Validation](macos_milestone_7_file_algorithm_validation.md).
3. **In progress:** E2VID/model qualification. **Slice 3A is Complete /
   Qualified** for the no-successfully-loaded-model heuristic fallback,
   deterministic cross-mode temporal-state invalidation, bounded real-RAW and
   playback evidence, and one Cocoa wiring/lifecycle session; see [macOS
   Milestone 7 Slice 3A E2VID Heuristic Fallback Validation](macos_milestone_7_e2vid_fallback_validation.md).
   Real compatible arm64 ONNX Runtime plus a model, plain/recurrent inference,
   and model conversion remain separate pending sub-phases;
4. offline export: extend existing bounded HDF5 evidence independently for
   CSV, RAW clip and AVI creation/readback/error behavior;
5. calibration solve/YAML/undistort after the M6 live-capture prerequisite;
6. processed-recording semantics and output integrity after M6 live lifecycle.

Slice 3 remains **In progress**: only its 3A no-model heuristic-fallback
sub-phase is qualified. Slices 4 through 6 remain pending. Slice 1 qualifies
the current catalog and algorithm-parameter persistence contract; Slice 2
qualifies bounded deterministic file-source preprocessing and representative
algorithm evidence. These slices, including Slice 3A, do not establish
broader algorithm runtime, neural model behavior, export, calibration, or
processed-recording behavior.

General `AlgoResult` export remains a deferred product decision, not an
assumed current feature. M7 must separate current source inventory, automated
tests, representative macOS runtime and numerical/Linux comparison evidence.
The frozen-integration Linux risk acceptance does not extend to M7.

**完成标准**

Each accepted slice records fixed inputs, actual parameters, output/error and
reset/cleanup behavior. Real model inference, source-event export and
algorithm-result export are reported separately. Linux/macOS comparison and
future Linux acceptance remain explicit decisions rather than inherited risk
acceptance.

### Milestone 8: Packaging and CI

**状态：** `Planned`
**独立分支：** `build/macos-packaging-ci`

**范围**

- 生成可运行的 macOS `.app` bundle。
- 收集并验证 Qt frameworks/plugins、OpenCV、OpenEB SDK/HAL plugins、HDF5/ECF
  plugin、optional ONNX Runtime/models 和其他运行时依赖及 RPATH。
- 根据需要提供 DMG；代码签名和 notarization 作为后续可选工作，不作为初始移植阻塞项。
- 增加 Apple Silicon CI，同时保持 Linux CI 正常。
- 记录打包输入、产物边界、运行环境、已知限制和回滚方法。

**检查方法**

- 在干净环境检查 `.app` 结构、资源、插件和 `otool -L`/RPATH 结果；从 Finder 与
  terminal launch 均不得依赖 developer absolute path 或 fallback prefix。
- 从 Finder 和终端分别启动，执行 GUI、RAW、算法和无需硬件的导出 smoke test。
- 在可用硬件环境验证打包应用的设备发现和实时相机路径。
- CI 执行 macOS arm64 configure/build/tests/packaging smoke test，并持续执行 Linux configure/build/tests。
- 若制作 DMG，验证安装、首次启动、升级/覆盖和卸载说明；签名状态必须如实标注。

**完成标准**

- `.app` 在规定的 macOS arm64 环境可独立启动，运行时依赖不指向开发者个人目录；
  build-tree/install RPATH evidence does not by itself satisfy standalone
  loader or bundle closure.
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

- `devlog/` 保存 legacy Linux design/build 文档，`docs/` 保存 macOS、workspace 和
  validation 记录；两者的文档角色不同，不应把历史路径当作当前 build producer。
- `README.md` 与 `README_CN.md` 写明 Ubuntu 22.04+，而 `devlog/compile.md` 记录 Ubuntu 26.04；Milestone 1 historical record 仍提示需要核实支持基线。
- `devlog/compile.md` 的直接运行示例使用 `./build/gui_for_openeb`，而 `run.sh` 和当前 CMake 目标布局指向 `build/gui/gui_for_openeb`；需要在独立 Linux-documentation scope 中协调。
- 根仓库的 HDF5 ECF gitlink 已建立完整根级 `.gitmodules` mapping，并在 prerequisite 分支检出锁定提交；该静态依赖完整性恢复尚未经过独立 fresh clone、configure、build 或运行验证。
- `run.sh`、`gui/main.cpp` 和 `algo/CMakeLists.txt` 存在 Linux 系统路径、动态库和显示后端假设；它们是后续平台隔离热点，本轮仅记录，不修改。
- 现有 Linux README、`run.sh` 和 `devlog/compile.md` 仍包含传统 `build/`、系统 `/tmp` 或 `/usr/local` 流程。这些是需要后续协调的 legacy baseline，本轮保持原文以避免改变 Linux 指引，但不构成未来任务绕过仓库内工作区规范的授权。
- OpenEB 5.1.1 是已验证的稳定 macOS 环境，但 EBplus 的目标版本是 5.2.0；任何比较结果都不能成为覆盖 5.1.1 或将 EBplus 降级到 5.1.1 的理由。
