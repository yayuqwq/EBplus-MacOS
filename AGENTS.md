# EBplus 仓库协作规则

本文件是进入本仓库工作的 Codex、AI agent 和开发者必须首先阅读的规则。除非当前任务得到用户明确授权，否则不得偏离这里规定的范围、Git 工作流和兼容性要求。

## Project goal

- 保持 EBplus 现有 Linux 版本的行为、构建路径和功能。
- 为 macOS Apple Silicon（arm64）增加正式支持，并逐步达到与 Linux 当前版本相同的功能范围。
- macOS 支持是跨平台增强，不是把 Linux 项目改写为 macOS-only 项目。
- 平台特定修改必须尽可能隔离在 CMake 条件、预处理条件、专用脚本或平台适配层中。
- 不允许为了 macOS 支持删除、绕过或破坏 Linux 代码路径。
- 当前第一目标是对齐 EBplus 所要求的 OpenEB / Metavision SDK 5.2.0，不随意升级到更高版本，也不为了快速通过配置而把项目要求降为 5.1.1。

## Required workflow

每次修改前必须运行：

```bash
git status --short --branch
git branch --show-current
```

工作规则：

- 不直接修改 `main`；在目的和范围明确的分支上工作。
- 修改前先检查工作区，不覆盖、不暂存、不丢弃用户已有改动。
- 修改范围必须与当前任务直接相关，不进行无关格式化、重命名、大范围清理或顺手修复。
- 保持 `main` 可构建、可运行，并保持 Linux 原有行为。
- 每次只推进一个清晰的 milestone；不同 milestone 不混在同一分支或变更集中。
- 优先形成小而可审核的修改。
- 如果发现超出当前任务范围的问题，只记录并报告，不自行扩大范围。
- 生成文件、构建目录、安装目录、应用包、模型缓存和本地运行配置不得提交。

未经用户明确要求，不得执行：

- `commit`
- `push`
- `merge`
- `rebase`
- `cherry-pick`
- `reset`
- 创建或修改 `tag`
- 创建 `release`
- 创建 PR
- Git 历史重写

不得执行 `git clean -fd`、`git reset --hard`、`git checkout -- <path>`、`git restore <path>` 或其他会删除、覆盖、回退现有修改的破坏性命令，除非用户明确指定了对象并授权该操作。

## Branch conventions

使用能够表达变更性质和范围的分支名：

```text
chore/...     项目结构、构建工具、依赖、CI、脚本和维护
feat/...      新功能
fix/...       Bug 修复
docs/...      仅文档修改
test/...      测试和 smoke test
refactor/...  不应改变行为的结构调整
build/...     CMake、依赖发现、链接、打包和构建系统
```

macOS 相关分支示例：

```text
chore/macos-porting-foundation
build/macos-openeb-5.2-isolation
build/macos-cmake-configuration
feat/macos-gui-launch
feat/macos-raw-playback
feat/macos-live-camera
feat/macos-export-support
test/macos-smoke-tests
docs/macos-build-guide
```

不要使用范围不清的分支名，例如：

```text
macos
update
changes
fix-stuff
test
```

## Commit conventions

项目使用小而可审核的 commit；不要把不相关修改放入同一个 commit。

允许的 commit 类型：

```text
docs:
chore:
build:
feat:
fix:
test:
refactor:
ci:
```

提交前必须检查：

```bash
git status --short
git diff --stat
git diff
git diff --cached --stat
git diff --cached
```

- 只有用户明确要求时才能 commit。
- 提交前确认暂存区只包含当前任务的文件和修改。
- 不直接向 `main` push。
- 不通过 amend、force push 或 rebase 擅自重写历史。

## Pull request requirements

每个 PR 应清楚说明：

- 修改了什么。
- 为什么需要修改。
- 如何测试。
- 测试平台和架构。
- 是否影响 Linux。
- 是否影响 macOS。
- 是否影响 OpenEB 版本或加载路径。
- 是否影响相机连接。
- 是否影响 RAW 回放。
- 是否影响 GUI。
- 是否影响算法结果。
- 是否影响导出格式。
- 是否影响模型推理。
- 是否影响构建、安装、打包或运行脚本。
- 已知限制。
- 回滚方法。

如果 PR 涉及平台特定逻辑，必须分别说明 Linux 和 macOS 路径如何配置、构建和运行，不得只描述其中一个平台。

## Cross-platform development principles

1. macOS 支持不能通过删除 Linux 分支或使 Linux 分支失效来实现。
2. Linux 专用逻辑应保留在 Linux 条件中；不要把 Linux 行为无条件替换成 macOS 行为。
3. macOS 专用逻辑应优先放在以下位置：
   - CMake 的 `if(APPLE)` 条件。
   - C/C++ 的 `#ifdef __APPLE__` 条件。
   - macOS 专用脚本。
   - 明确的平台适配层。
4. 不在业务代码中到处散布平台判断；公共行为保持共享，平台差异集中处理。
5. 禁止在代码、CMake、脚本和运行配置中写死个人或机器相关绝对路径，例如 `/Users/<username>/...`、固定的包管理器前缀或系统安装前缀。
6. 本机真实路径只允许出现在审计说明、文档示例或未跟踪的本地配置中。`docs/openeb_version_isolation.md` 可以记录当前环境的真实路径，以便明确隔离边界；这些路径不得复制到代码、CMake、脚本或运行配置。
7. 文档中的可复用命令应优先使用 `$PWD`、`$REPO_ROOT`、CMake cache 变量或用户显式传入的 prefix。
8. CMake 应优先接受用户显式传入的依赖 prefix，不应暗中选择某台机器的安装目录。
9. 不依赖永久全局环境变量；项目环境应通过项目专用命令、CMake preset 或启动脚本临时启用。
10. 不提交生成的二进制、build tree、`.app`、模型缓存或安装目录。
11. 不随意升级 Qt、OpenCV、OpenEB、模型运行时或其他依赖版本。
12. 当前应对齐 OpenEB / Metavision SDK 5.2.0；不得为了通过 CMake 而把要求降为 5.1.1。
13. OpenEB 5.1.1 的 macOS 修改只能作为参考；移植到 5.2.0 前必须逐项评估差异和必要性。
14. 不假设 5.1.1 补丁可以原样应用到 5.2.0。
15. 任何平台差异必须通过代码注释、提交说明或开发文档解释原因和适用范围。

## OpenEB environment protection

- 已验证的 OpenEB / Metavision SDK 5.1.1 是稳定系统环境，默认只允许只读检查。
- 仓库自带的 OpenEB 5.2.0 必须使用独立的源码、build directory、install prefix 和运行环境。
- 禁止把 5.2.0 安装到 `/usr/local`，禁止使用 `sudo cmake --install ...` 或 `sudo make install`。
- 禁止覆盖 5.1.1 的命令、头文件、动态库、CMake package files 或 HAL plugins。
- 禁止为 5.2.0 创建指向 `/usr/local/bin` 或 `/usr/local/lib` 的全局链接。
- 禁止把 5.2.0 的 `PATH`、`CMAKE_PREFIX_PATH`、`DYLD_LIBRARY_PATH` 或 HAL plugin path 永久写入全局 shell 配置。
- 构建失败时必须调查配置和兼容性问题，不得通过覆盖稳定的 5.1.1 环境绕过问题。
- 具体路径、命令和验证方法以 `docs/openeb_version_isolation.md` 为准。

## Testing principles

后续每个 milestone 至少应根据修改范围考虑：

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

- 不得仅根据“配置成功”或“编译成功”宣称功能完成。
- 测试报告必须区分实际执行、未执行、无法执行和失败的检查。
- 无法访问目标平台、相机、RAW 样本或模型时，应明确写出限制，不得推断验证通过。
- 平台特定修改应同时检查目标平台路径和 Linux 回归风险。

涉及相机时，应分别验证：

```text
Device enumeration
Device open
Live event stream
Facility access
Parameter changes
Clean shutdown
Reconnect
```

涉及 RAW 时，应分别验证：

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

## Required final report

每次完成任务后，必须报告：

```text
1. Current branch
2. git status --short
3. Files changed
4. Summary of behavior changes
5. Checks run
6. Checks not run
7. Known risks or unresolved issues
8. Whether commit or push was performed
```

报告要求：

- 使用实际分支名、实际状态和实际文件列表，不使用模糊占位说明。
- 如果没有行为变化，明确写明“无功能行为变化”。
- 如果没有进行构建或运行测试，明确写明“未构建”及“未运行程序”，不得写成“验证通过”。
- 明确说明是否修改了 Linux 行为、是否构建或安装 OpenEB、是否修改系统安装路径，以及是否执行 commit 或 push。
