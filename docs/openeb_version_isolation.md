# OpenEB 版本隔离规范

本文档定义 EBplus macOS 开发期间 OpenEB / Metavision SDK 5.1.1 与 5.2.0 的隔离边界。目标是在保留已验证稳定环境的同时，使仓库自带的 5.2.0 可以独立配置、构建、安装和运行，且不会覆盖或污染系统环境。

> 本文档使用 `$HOME` 表示当前用户的主目录，使用 `$REPO_ROOT` 表示 EBplus 仓库根目录。未来代码、CMake、脚本及运行配置不得写死展开后的个人绝对路径；可复用配置应使用项目相对路径、`$PWD`、`$REPO_ROOT`、CMake cache 变量或用户显式传入的 prefix。

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
- 任何可能改变 5.1.1 源码、安装内容或运行环境的操作，均须先取得用户明确授权。

## EBplus isolated environment：OpenEB 5.2.0

EBplus 应使用仓库自带的 OpenEB 5.2.0 源码，并把源码、构建树、安装树和 EBplus GUI 构建树严格分开。

| 用途 | 隔离路径 |
|---|---|
| OpenEB 5.2.0 source | `$REPO_ROOT/openeb` |
| OpenEB 5.2.0 build | `$REPO_ROOT/.build/openeb-5.2.0-macos` |
| OpenEB 5.2.0 install | `$REPO_ROOT/.deps/openeb-5.2.0-macos` |
| EBplus GUI build | `$REPO_ROOT/.build/ebplus-macos` |

`$REPO_ROOT/.build/` 和 `$REPO_ROOT/.deps/` 必须保持为未跟踪的项目本地目录。不得进行 in-source build，也不得复用曾经指向 5.1.1 或 `/usr/local` 的 CMake build directory；每套配置使用全新的 build directory，防止 CMake cache 将依赖解析回稳定环境。

### 5.2.0 硬性规则

- 禁止把 OpenEB 5.2.0 安装到 `/usr/local`。
- 禁止执行：

  ```bash
  sudo cmake --install ...
  sudo make install
  ```

- 所有 build 和 install 目录必须位于用户可写路径，不得依赖 `sudo`。
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
  -DCMAKE_INSTALL_PREFIX="$REPO_ROOT/.deps/openeb-5.2.0-macos"
```

配置前应确认使用的是全新的 `$REPO_ROOT/.build/openeb-5.2.0-macos`，并审核配置输出和 `CMakeCache.txt`，确保未从 `/usr/local` 意外拾取 OpenEB / Metavision SDK 5.1.1。

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

如果 cache 同时出现两个版本的路径，不得继续构建；应使用新的项目内 build directory 重新配置，并查明依赖发现顺序。

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
- 普通终端默认仍解析到 `/usr/local` 中的 5.1.1。
- EBplus 的 CMake cache、动态链接和 HAL plugin 加载均明确指向项目内 5.2.0。
- 不需要 `sudo`、全局符号链接或永久环境变量。
- C++ CLI、RAW 文件和真实相机验证均有实际执行记录，未执行项不得宣称通过。
