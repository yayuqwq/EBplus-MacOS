# 仓库内工作区与磁盘使用规范

本文档定义 EBplus 开发期间项目工作区、生成目录、依赖、缓存、临时文件和磁盘空间的统一规则。目标是让所有项目主动控制的内容保持在仓库内，避免污染稳定系统环境，并在磁盘空间有限的设备上控制重复构建和大型产物。

除非用户在当前任务中明确授权例外，开发者、Codex 和其他 AI agent 都必须遵守本文档。本文档只建立目录和操作规范；创建本文档的任务不应实际创建下述生成目录，也不应构建、下载或安装任何内容。

## 仓库边界

执行任何项目操作前，应从 Git 获取仓库根目录，不得依赖个人绝对路径：

```bash
REPO_ROOT="$(git rev-parse --show-toplevel)"
```

所有由项目、开发者或 AI agent 主动创建的以下内容都必须位于 `$REPO_ROOT` 内：

- CMake、Ninja、Xcode 或其他构建系统的 build tree。
- 项目专用安装前缀和第三方依赖。
- Python 虚拟环境和 Python package cache。
- 编译缓存及其他可重定向缓存。
- 下载文件、临时文件和日志。
- 测试输出、导出文件和 benchmark 输出。
- GUI 运行产物和崩溃调试文件。
- `.app`、DMG、符号包及其他打包产物。
- 生成代码、模型转换缓存及其他生成内容。

禁止为了方便将项目产物放到以下位置：

```text
/tmp
/private/tmp
$HOME/Downloads
$HOME/Desktop
$HOME/Documents
$HOME/Library
$HOME/.cache
$HOME/.local
/usr/local
/opt/homebrew
```

禁止在用户 home 目录创建新的项目级工作区，例如：

```text
$HOME/ebplus-build
$HOME/openeb-5.2-build
$HOME/venvs/ebplus
$HOME/Downloads/ebplus-dependencies
```

### `.git/` 专用边界

`$REPO_ROOT/.git/` 位于仓库边界以内，但仅供 Git 自身管理。不得手动将构建产物、安装内容、依赖、缓存、日志、下载文件、临时文件、测试输出、导出结果或其他项目生成内容放入 `.git/`。不得利用 `.git/` 隐藏不符合本规范的项目文件。

对 `.git/` 的任何维护应通过正常 Git 命令完成，并继续遵守仓库的 Git 安全规则；本文档不授权直接修改 Git 内部对象、引用或配置文件。

## 标准仓库内目录

后续项目工作统一使用以下根级目录：

| 目录 | 用途 |
|---|---|
| `$REPO_ROOT/.build/` | CMake、Ninja、Xcode 等构建目录。 |
| `$REPO_ROOT/.deps/` | 项目专用第三方依赖安装前缀；OpenEB 5.2.0 安装在这里。 |
| `$REPO_ROOT/.venv/` | 项目唯一默认 Python 虚拟环境。 |
| `$REPO_ROOT/.cache/` | pip、Python、编译器和其他可重定向缓存。 |
| `$REPO_ROOT/.tmp/` | 项目控制的临时文件。 |
| `$REPO_ROOT/.logs/` | 构建、运行和测试日志。 |
| `$REPO_ROOT/.downloads/` | 经用户批准下载的依赖源码和压缩包。 |
| `$REPO_ROOT/.artifacts/` | 测试输出、导出结果、应用包和其他生成产物。 |

这些目录是按需使用的约定，不应仅为了“初始化工作区”而提前创建。它们必须保持未跟踪，不得提交其中的生成内容。

不得在源码目录内创建传统构建树，例如：

```text
$REPO_ROOT/openeb/build
$REPO_ROOT/gui/build
$REPO_ROOT/algo/build
```

OpenEB 5.2.0 和 EBplus 的标准路径固定为：

```text
Source:
$REPO_ROOT/openeb

Build:
$REPO_ROOT/.build/openeb-5.2.0-macos

Install:
$REPO_ROOT/.deps/openeb-5.2.0-macos

EBplus build:
$REPO_ROOT/.build/ebplus-macos
```

不得另建内容相同的仓库外 build tree，也不得为了不同尝试随意复制已有构建树。

## 外部环境只读边界

以下环境可以在任务需要时进行只读审计：

```text
$HOME/Metavision/openeb
/usr/local
/opt/homebrew
```

除非用户在当前任务中对具体操作明确授权，否则不得在这些位置创建、修改、安装、删除、覆盖或链接任何内容。尤其不得：

- 修改或复用稳定 OpenEB 5.1.1 的源码、build tree、安装内容或 Python 环境。
- 向 `/usr/local` 安装 OpenEB 5.2.0 或 EBplus 依赖。
- 修改 Homebrew formula、Cellar、prefix 或已安装包。
- 创建指向项目依赖的全局 symlink。
- 修改 `~/.zshrc`、`~/.bashrc` 或其他全局 shell 配置来永久启用项目环境。

`docs/openeb_version_isolation.md` 对 OpenEB 5.1.1 和 5.2.0 的隔离规则具有同等约束力。

## Python 与虚拟环境

项目唯一默认 Python 虚拟环境是：

```text
$REPO_ROOT/.venv
```

除非用户明确要求采用另一种隔离方案，否则禁止：

```bash
python -m venv "$HOME/..."
pip install --user ...
sudo pip install ...
conda create ...
```

具体规则如下：

- 不把 Python 包安装到系统 Python 或用户级全局 site-packages。
- 不创建仓库外 EBplus 专用 venv。
- 不修复或复用仓库外已经失效的 Python venv。
- 不修改 OpenEB 5.1.1 对应的外部 Python 环境。
- 创建 venv 或安装 Python 包仍须先执行本文档规定的磁盘检查。
- Python 缓存应尽可能重定向到仓库内，而不是写入 `$HOME/.cache` 或其他全局目录。

未来项目专用命令或脚本可以只在当前进程中使用：

```bash
export PIP_CACHE_DIR="$REPO_ROOT/.cache/pip"
export PYTHONPYCACHEPREFIX="$REPO_ROOT/.cache/python"
```

这些变量不得永久写入全局 shell 配置。

## CMake、依赖和临时文件

- 只允许 out-of-source build；CMake build directory 必须位于 `$REPO_ROOT/.build/`。
- 项目专用安装前缀必须位于 `$REPO_ROOT/.deps/`。
- 默认只使用用户显式传入的 prefix，不把 `/usr/local` 或 `/opt/homebrew` 当作项目写入目标。
- CMake 找不到依赖时，应修正配置、依赖发现或平台适配，不能将文件复制到 `/usr/local/lib`、`/usr/local/include` 或其他全局位置。
- 项目依赖的源码或压缩包只能下载到 `$REPO_ROOT/.downloads/`，且下载前必须获得任务所需授权并完成空间预算。
- 构建、运行和测试日志应写入 `$REPO_ROOT/.logs/`；可复现输出和打包产物应写入 `$REPO_ROOT/.artifacts/`。
- 项目控制的临时文件应优先写入 `$REPO_ROOT/.tmp/`。

除非用户对该具体操作明确授权，否则禁止：

```bash
sudo cmake --install ...
sudo make install
pip install --user ...
brew install ...
brew upgrade ...
```

后续运行项目专用脚本时，可以只在当前进程中设置：

```bash
export TMPDIR="$REPO_ROOT/.tmp"
```

不得永久修改全局 `TMPDIR`。如果某个工具不能安全使用重定向后的 `TMPDIR`，应在执行前说明其行为和潜在外部写入，不得静默改用系统临时目录。

## 运行前磁盘检查

以下操作开始前必须执行磁盘检查：

- 编译 OpenEB 或 EBplus。
- 创建 Python venv、安装依赖或填充大型缓存。
- 下载模型、数据、依赖源码或压缩包。
- 运行大规模测试或 benchmark。
- 生成视频、RAW、HDF5 或其他大型导出文件。
- 创建 `.app`、DMG、符号包或其他打包产物。

检查命令：

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

开始前必须报告：

```text
当前磁盘总容量和可用空间
当前仓库总大小
现有 build 大小
现有 dependencies 大小
其他现有生成目录大小
预计新增磁盘占用及主要来源
预计操作完成后的可用空间
本轮是否可能产生重复构建树或重复下载
```

空间预算应采用保守估算，只使用 `df` 显示的当前实际可用空间。不得把 APFS、macOS 或其他系统可能自动释放的 purgeable space、cache 或压缩收益计入预算。

## 用户批准门槛

以下两类门槛相互独立；满足任意一项都必须暂停并取得用户对该具体操作的明确授权。

### 单次增长门槛

如果一个操作预计新增磁盘占用达到或超过 `1 GiB`，执行前必须说明预计大小及主要来源并等待授权。不能把一个大型操作拆成多个小步骤规避该规则。如果无法合理估算大小，按可能超过 `1 GiB` 处理。

### 最低剩余空间保护线

预计操作完成后，实际可用空间必须同时满足：

```text
不低于 20 GiB
不低于磁盘总容量的 15%
```

因此，实际采用的保护线是两者中更严格（数值更高）的一项：

```text
保护线 = max(20 GiB, 磁盘总容量 × 15%)
```

如果预计完成后的可用空间低于保护线，不得直接执行，即使该操作预计增长不足 `1 GiB`。必须先向用户报告：

- 完整空间预算和保护线计算结果。
- 已确认属于生成产物且可重新生成的清理候选。
- 每项候选的准确路径、文件类型和当前大小。
- 删除后是否能够重新生成，以及重新生成的成本。
- 不清理现有内容时的替代方案。
- 是否可以通过减少模块、构建配置、测试范围或输出降低空间需求。

只有用户明确授权具体操作后才能继续。不得预先删除内容来满足保护线，也不得假设系统会在操作期间自动回收空间。

## 节省空间的构建原则

在满足功能目标的前提下，默认采用以下配置：

- 优先使用单一 `Release` build tree。
- 不同时保留 Debug、Release 和 RelWithDebInfo 多套构建。
- 初期只构建 Apple Silicon `arm64`，不默认构建 universal binary。
- 默认不构建当前 milestone 不需要的 tests、samples、benchmarks、documentation 和可选模块。
- 不创建多个内容相同的 OpenEB build tree。
- 不复制大型模型、RAW、视频或数据集。
- 不在不同目录重复下载同一依赖。
- 不启用 Docker 构建，除非用户明确要求。
- 不生成 DMG、符号包或完整发布资产，除非当前 milestone 明确需要。
- 不保留没有调查价值的失败构建副本。

节省空间不能成为擅自删除用户文件、减少用户明确要求的验证范围或隐藏测试失败的理由。若空间优化会改变功能范围或测试覆盖，必须先报告并获得用户确认。

## 清理和删除规则

不得为了节省空间擅自删除已有文件。提出清理建议前必须先确认候选确实属于生成产物，并报告：

```text
准确路径
文件类型或用途
目录大小
是否确认属于生成产物
删除后是否能够重新生成
重新生成所需条件或成本
```

只有用户明确允许删除具体路径后才能执行。对生成目录的删除也必须使用完整、明确且经过检查的路径，不得使用模糊 glob、仓库根目录相对猜测或范围扩大的命令。

禁止执行：

```bash
git clean -fd
git clean -fdx
rm -rf .
rm -rf "$REPO_ROOT"
```

本文档不授权任何删除操作；仓库中的其他 Git 安全规则继续适用。

## 系统缓存例外

macOS、Xcode、Qt、编译器、链接器或系统框架可能产生项目无法完全控制的系统级缓存。对此遵守以下规则：

- 不主动把项目文件写入系统缓存目录。
- 不把系统缓存作为项目构建、运行或测试流程的必需依赖。
- 能够重定向的缓存必须重定向到 `$REPO_ROOT/.cache/` 或对应的仓库内标准目录。
- 无法重定向且可能产生明显磁盘占用时，执行前必须报告预计行为和空间风险。
- 最终报告应区分项目主动控制的写入和工具或系统可能产生的外部缓存。
- 除非实际检查了相关系统位置，否则不得声称 macOS、Xcode、Qt 或其他工具绝对没有产生任何外部缓存。

当本轮没有项目控制的仓库外写入时，应准确报告：

```text
No project-controlled files were written outside the repository.
```

该表述不等于断言操作系统没有更新自身缓存。

## 既有 Linux 工作流说明

现有 README、`run.sh` 或 `doc/compile.md` 中可能仍记录 Linux 基线使用的 `build/`、`/tmp` 或 `/usr/local` 路径。这些内容是需要在后续独立 milestone 中审计和协调的 legacy baseline：

- 本规范不授权新的开发任务继续向这些外部位置写入项目产物。
- 本轮为避免改变 Linux 安装和运行说明，不修改这些既有流程或功能代码。
- 后续调整必须保留 Linux 行为，并将新的项目主动控制产物迁移到仓库内标准目录或提供明确的平台兼容方案。
- 如果既有脚本与本规范冲突，应先报告冲突和迁移方案，不得静默执行外部写入，也不得在当前无关任务中顺手修改脚本。

## 任务结束时的磁盘与工作区报告

任何可能产生项目文件的任务都应在最终报告中列出：

```text
Repository size before/after
Generated directories created
Estimated and actual disk growth
Minimum free-space protection line and projected/actual remaining space
Any project-controlled writes outside repository
```

如果没有创建生成目录、没有外部项目写入，也应明确写出，而不是省略。磁盘增长应尽可能使用执行前后的实际测量值，并解释明显差异。
