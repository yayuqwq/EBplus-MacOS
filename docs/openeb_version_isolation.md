# OpenEB 版本隔离规范

本文档定义 EBplus macOS 开发期间 OpenEB / Metavision SDK 5.1.1 与 5.2.0 的隔离边界。目标是在保留已验证稳定环境的同时，使仓库自带的 5.2.0 可以独立配置、构建、安装和运行，且不会覆盖或污染系统环境。

> 本文档使用 `$HOME` 表示当前用户的主目录，使用 `$REPO_ROOT` 表示 EBplus 仓库根目录。未来代码、CMake、脚本及运行配置不得写死展开后的个人绝对路径；可复用配置应使用项目相对路径、`$PWD`、`$REPO_ROOT`、CMake cache 变量或用户显式传入的 prefix。所有项目主动控制的工作区和生成内容还必须遵守 [`local_workspace_policy.md`](local_workspace_policy.md)。

## Stable system environment：OpenEB 5.1.1

这套环境已经过真实硬件和 RAW 工作流验证，是当前 macOS 稳定基线。

| 项目 | 当前值 |
|---|---|
| Version | OpenEB / Metavision SDK 5.1.1 |
| Source | `$HOME/Metavision/openeb` |
| Install prefix | `/usr/local` |
| Architecture | macOS Apple Silicon / arm64 |
| Purpose | 已验证的稳定 macOS 环境 |
| Status | 可连接真实相机、打开 RAW 文件并运行 C++ CLI |
| Saved branch | `macos/openeb-5.1.1-working` |
| Saved tag | `macos-openeb-5.1.1-working` |
| Saved commit | `01692d4` |
| CMake cache backup | `$HOME/Metavision/openeb-5.1.1-macos-CMakeCache.txt` |

### 5.1.1 保护规则

- 默认只允许对源码目录和安装结果进行只读检查；除非用户明确要求，不得修改该环境。
- 不修改 `$HOME/Metavision/openeb` 中的源码、构建配置或生成内容。
- 不在该源码目录切换 branch、tag 或 commit，不改变已保存的 branch、tag 和 commit。
- 不重新安装到 `/usr/local`，不覆盖现有安装。
- 不覆盖现有头文件、动态库、CMake package files、命令或 HAL plugins。
- 不删除其现有 build directory 或其他已验证产物。
- 不把该源码目录或其 build directory 用作 EBplus 所需 OpenEB 5.2.0 的源码或构建目录。
- 不把 5.1.1 的安装前缀改作 5.2.0 的临时安装位置。
- 不改写或删除 CMake cache 备份、保存分支、保存标签及保存提交。
- `$HOME/Metavision/openeb` 与 `/usr/local` 只允许作为外部稳定环境进行只读审计，不得用作 EBplus 的 build、cache、download、log、temporary 或 artifact 目录。
- 任何可能改变 5.1.1 源码、安装内容或运行环境的操作，均须先取得用户明确授权。

## EBplus isolated environment：OpenEB 5.2.0

EBplus 应使用仓库自带的 OpenEB 5.2.0 源码，并把源码、构建树、安装树和 EBplus GUI 构建树严格分开。

| 用途 | 隔离路径 |
|---|---|
| OpenEB 5.2.0 source | `$REPO_ROOT/openeb` |
| OpenEB 5.2.0 build | `$REPO_ROOT/.build/openeb-5.2.0-macos` |
| OpenEB 5.2.0 install | `$REPO_ROOT/.deps/openeb-5.2.0-macos` |
| EBplus GUI build | `$REPO_ROOT/.build/ebplus-macos` |
| OpenEB cache | `$REPO_ROOT/.cache/openeb-5.2.0-macos` |
| Approved downloads | `$REPO_ROOT/.downloads/openeb-5.2.0-macos` |
| Build and test logs | `$REPO_ROOT/.logs/openeb-5.2.0-macos` |
| Controlled temporary files | `$REPO_ROOT/.tmp/openeb-5.2.0-macos` |
| Generated artifacts | `$REPO_ROOT/.artifacts/openeb-5.2.0-macos` |

这些目录必须保持为未跟踪的项目本地目录。OpenEB 5.2.0 默认只允许使用一个标准的 macOS arm64 `Release` build tree：`$REPO_ROOT/.build/openeb-5.2.0-macos`。不得进行 in-source build，不得使用仓库外 build tree，也不得复用曾经指向 5.1.1 或 `/usr/local` 的 CMake cache。需要重建时，应先检查并报告现有标准目录，再在用户授权清理后复用同一路径；不得通过另建 Debug、RelWithDebInfo 或重复 Release build tree 绕过缓存或磁盘规则。

### 5.2.0 硬性规则

- 禁止把 OpenEB 5.2.0 安装到 `/usr/local`。
- 禁止在仓库外创建 OpenEB 5.2.0 的 build、install、cache、download、log、temporary 或 artifact 目录，包括 `~/opt/openeb-5.2.0-ebplus`、`~/openeb-5.2-build`、`~/Downloads` 和系统临时目录。
- 禁止执行：

  ```bash
  sudo cmake --install ...
  sudo make install
  ```

- 所有 build、install、cache、download、log、temporary 和 artifact 目录必须位于 `$REPO_ROOT` 的标准工作区目录中，不得仅以“用户可写”为理由使用仓库外路径，也不得依赖 `sudo`。
- `$REPO_ROOT/.git/` 只能由 Git 自身管理，不得把 OpenEB 构建产物、缓存、日志、下载或临时文件放入其中。
- 禁止为 5.2.0 创建指向 `/usr/local/bin`、`/usr/local/lib` 或其他全局目录的符号链接。
- 禁止覆盖 5.1.1 的以下内容：
  - `metavision_*` 命令。
  - `libmetavision_*.dylib` 动态库。
  - CMake package files。
  - headers。
  - HAL plugins。
- 禁止把 5.2.0 永久写入全局 `~/.zshrc` 或其他 shell 启动配置。
- 不永久修改全局 `PATH`、`CMAKE_PREFIX_PATH`、`DYLD_LIBRARY_PATH`、`MV_HAL_PLUGIN_PATH` 或其他 HAL plugin path。
- 只允许通过项目专用命令、CMake preset 或启动脚本，在当前配置或进程范围内启用 5.2.0。
- CMake 必须优先使用用户显式传入的 prefix；不得在代码、CMake、脚本或运行配置中写死 `/Users/<username>/...`、固定的包管理器前缀、系统安装前缀或其他机器相关路径。
- 构建失败时应检查依赖发现、架构、动态链接和平台兼容问题，不得通过覆盖 `/usr/local` 或修改稳定的 5.1.1 环境绕过问题。
- 当前目标版本是 5.2.0；不得为了快速通过 CMake 而把 EBplus 的要求降为 5.1.1，也不得在本阶段随意升级到更高版本。
- 5.1.1 的 macOS 补丁只能作为参考。每项补丁都必须结合 5.2.0 的源码差异、上游变化和必要性单独评估，不得假设可以原样复制。

## 构建前磁盘检查与授权

在配置、构建或安装 OpenEB 5.2.0，以及下载其依赖或生成大型测试输出前，必须先运行：

```bash
REPO_ROOT="$(git rev-parse --show-toplevel)"

df -h "$REPO_ROOT"
du -sh "$REPO_ROOT"

du -sh \
  "$REPO_ROOT/.build" \
  "$REPO_ROOT/.deps" \
  "$REPO_ROOT/.venv" \
  "$REPO_ROOT/.cache" \
  "$REPO_ROOT/.tmp" \
  "$REPO_ROOT/.logs" \
  "$REPO_ROOT/.downloads" \
  "$REPO_ROOT/.artifacts" \
  2>/dev/null || true
```

开始操作前必须报告当前实际可用空间、仓库总大小、现有 build 与 dependencies 大小、预计新增占用，以及是否会产生重复构建树。磁盘预算还必须遵守以下硬性规则：

- 单次操作预计新增占用达到或超过 `1 GiB` 时，必须说明预计大小和主要来源，并取得用户对该具体操作的明确授权。无法合理估算时按可能超过 `1 GiB` 处理。
- 不得把一个大型操作拆成多个小步骤来规避 `1 GiB` 授权阈值。
- 如果预计完成后的实际可用空间低于 `20 GiB`，或低于磁盘总容量的 `15%`，不得直接执行；两条保护线取更严格者。
- 触发最低剩余空间保护线时，必须先报告完整空间预算、可清理生成产物的准确路径和大小、是否可重新生成、无需清理的替代方案，以及能否减少模块、配置或输出，并取得用户明确授权。
- 不得把 APFS、macOS 或其他系统可能自动释放的 purgeable/cache 空间计入预算，只能使用当前实际报告的可用空间。
- 不得为了满足预算擅自删除已有 build tree 或其他文件；任何清理仍须单独报告并取得用户明确授权。
- 默认只保留上述单一 `Release`/arm64 build tree；不构建 universal binary，也不默认构建不需要的 tests、samples、benchmarks、documentation 或可选模块。

## 未来建议命令

以下命令记录下一阶段建议的隔离流程，**本轮基础建设不得执行这些命令，不得构建或安装 OpenEB 5.2.0**。执行时先在 EBplus 仓库内设置当前 shell 使用的仓库根目录变量：

```bash
REPO_ROOT="$(git rev-parse --show-toplevel)"
```

### 配置 OpenEB 5.2.0

```bash
cmake -S "$REPO_ROOT/openeb" \
  -B "$REPO_ROOT/.build/openeb-5.2.0-macos" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_INSTALL_PREFIX="$REPO_ROOT/.deps/openeb-5.2.0-macos"
```

配置前应完成磁盘检查和所需授权，并确认使用的是唯一标准路径 `$REPO_ROOT/.build/openeb-5.2.0-macos`。审核配置输出和 `CMakeCache.txt`，确保未从 `/usr/local` 意外拾取 OpenEB / Metavision SDK 5.1.1；若现有 cache 已受污染，应停止并报告，不得改用第二个 build tree。

### 构建 OpenEB 5.2.0

```bash
cmake --build "$REPO_ROOT/.build/openeb-5.2.0-macos" \
  -j"$(sysctl -n hw.logicalcpu)"
```

### 安装到项目内 prefix

```bash
cmake --install "$REPO_ROOT/.build/openeb-5.2.0-macos"
```

该命令只能在配置阶段的 `CMAKE_INSTALL_PREFIX` 已确认指向 `$REPO_ROOT/.deps/openeb-5.2.0-macos` 后执行。不得为安装命令添加 `sudo`。

### 配置 EBplus GUI

```bash
cmake -S "$REPO_ROOT" \
  -B "$REPO_ROOT/.build/ebplus-macos" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$REPO_ROOT/.deps/openeb-5.2.0-macos"
```

未来若需要额外的 Qt、OpenCV 或模型运行时 prefix，应由用户显式传入或通过项目专用 preset 管理；不得将本机安装位置写死到项目文件中。

## 验证方法

验证必须同时覆盖普通终端中的稳定环境和 EBplus 隔离构建中的依赖解析。仅配置或编译成功不足以证明隔离有效。

### 1. 确认普通终端仍使用 5.1.1

在未启用任何 EBplus 项目专用环境的普通终端中运行：

```bash
which metavision_viewer
metavision_viewer --version
```

预期结果：命令仍来自 `/usr/local`，报告 OpenEB / Metavision SDK 5.1.1。若普通终端默认解析到项目内 5.2.0，说明存在全局环境污染，必须停止后续工作并检查 shell 配置、符号链接和搜索路径。

### 2. 确认 EBplus CMake 使用隔离的 5.2.0

EBplus 完成配置后运行：

```bash
grep -i -E "Metavision|OpenEB" \
  "$REPO_ROOT/.build/ebplus-macos/CMakeCache.txt"
```

检查所有相关 include、library、package 和 prefix 路径。它们应指向：

```text
$REPO_ROOT/.deps/openeb-5.2.0-macos
```

而不是：

```text
/usr/local
```

如果 cache 同时出现两个版本的路径，不得继续构建；应查明依赖发现顺序，并在获准清理现有标准 build tree 后使用同一路径重新配置，不得创建重复 build directory。

### 3. 确认 EBplus 运行时动态链接

EBplus 构建完成后运行：

```bash
otool -L <EBplus可执行文件> | grep -i metavision
```

预期所有 Metavision 动态库解析到 `$REPO_ROOT/.deps/openeb-5.2.0-macos`，或使用最终能够正确解析到该 prefix 的受控 RPATH；不得解析到 `/usr/local` 中的 5.1.1。还应检查 HAL plugin 的实际加载位置，确认没有混用两个版本的插件和动态库。

### 4. 后续功能验证

版本和链接路径确认后，仍须分别验证：

- C++ CLI 可以运行并报告 5.2.0。
- RAW 文件可以打开、解析和读取。
- 真实相机可以枚举、打开、产生事件流并干净关闭。
- HAL plugins 来自项目内 5.2.0 环境。
- 退出项目专用环境后，普通终端中的 5.1.1 行为保持不变。

以上验证属于后续 OpenEB 5.2.0 隔离 milestone，本轮不执行。

## 隔离验收标准

只有同时满足以下条件，才能认为 OpenEB 5.2.0 的 macOS 环境已正确隔离：

- 5.1.1 源码、build、保存引用和 `/usr/local` 安装内容均未改变。
- 5.2.0 的 build、install 和 EBplus build 均位于仓库内各自独立的未跟踪目录。
- 5.2.0 的 cache、download、log、temporary 和其他项目控制产物均位于仓库内标准目录，且 `$REPO_ROOT/.git/` 未被用作项目工作区。
- OpenEB 使用唯一的标准 `Release`/arm64 build tree，没有不必要的重复配置树。
- 构建前磁盘检查、预计增长、授权判断和完成后的实际增长均有记录，并满足 `1 GiB` 授权阈值及 `20 GiB`/总容量 `15%` 最低剩余空间保护线。
- 普通终端默认仍解析到 `/usr/local` 中的 5.1.1。
- EBplus 的 CMake cache、动态链接和 HAL plugin 加载均明确指向项目内 5.2.0。
- 不需要 `sudo`、全局符号链接或永久环境变量。
- C++ CLI、RAW 文件和真实相机验证均有实际执行记录，未执行项不得宣称通过。
