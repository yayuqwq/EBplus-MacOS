# HDF5 ECF dependency recovery

## Purpose

EBplus 根仓库原先已经在 `openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf` 跟踪一个 gitlink，但缺少与该完整根路径对应的根级 `.gitmodules` mapping。因此 Git 无法从根仓库初始化锁定依赖，OpenEB 的 HDF5-enabled configure 也不能安全依赖其 configure-time submodule update。

本轮建立了根级 mapping，并且只对该路径执行了 submodule 初始化，检出根 gitlink 已锁定的提交。本轮没有执行 CMake configure、build、install 或任何运行验证。

## Dependency identity

| Item | Value |
| --- | --- |
| Path | `openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf` |
| URL | `https://github.com/prophesee-ai/hdf5_ecf.git` |
| Gitlink mode | `160000` |
| Pinned commit | `b982d908a0bc0afd9104d226607bedb1a11b2a95` |
| Checked-out commit | `b982d908a0bc0afd9104d226607bedb1a11b2a95` |
| Upstream project version | `1.0.1` |
| Dependency license | Apache License 2.0 (`LICENSE`) |
| Worktree state | Clean, detached HEAD |

主要构建文件为 `CMakeLists.txt` 和 `hdf5_ecf-config.cmake.in`。源码包含 ECF codec、HDF5 filter/plugin 以及对应 headers；恢复时工作树大小为 128 KiB。

## Recovery method

根 index 已经包含 gitlink，因此没有使用 `git submodule add`。重新 add 会与现有 mode `160000` entry 的所有权和锁定提交发生冲突。

根级 `.gitmodules` 使用完整 EBplus 路径声明：

```ini
[submodule "openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf"]
	path = openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf
	url = https://github.com/prophesee-ai/hdf5_ecf.git
```

初始化仅限目标路径：

```bash
HDF5_ECF_PATH="openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf"

git submodule sync -- "$HDF5_ECF_PATH"
git submodule update --init -- "$HDF5_ECF_PATH"
```

没有使用 `--remote`、无路径限制的 update 或 recursive 下载，也没有改变根 gitlink 的 pinned commit。`git submodule sync`/`update --init` 在根 `.git/config` 中注册了当前 submodule，并将 Git metadata 放入根仓库的 `.git/modules/` 管理区域。

## Verification

| Check | Result |
| --- | --- |
| Root mapping path | 与现有根 gitlink 完整路径一致 |
| Root mapping URL | 与已审计的 Prophesee URL 一致 |
| `git submodule status` | 前导字符为空格，commit 为 `b982d908...` |
| Actual `HEAD` | 等于 pinned commit |
| Worktree status | Clean |
| Worktree Git mode | Detached HEAD；未创建 tracking branch |
| Remote | `origin` fetch/push URL 均为已审计 URL |
| Worktree location | `$REPO_ROOT/openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf` |
| Git directory | `$REPO_ROOT/.git/modules/openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf` |
| Git metadata size at recovery | 148 KiB |
| Root gitlink after recovery | mode 和 commit 均未变化 |

`.git/config` 和 `.git/modules/...` 的变化属于 Git 管理的仓库内部状态，不是 tracked 文件变化。普通构建产物、日志和缓存不得写入 `.git/modules/`。

根 `.gitmodules` mapping 与本恢复记录由同一个 dependency-recovery revision 跟踪；该 revision 是首次在根仓库历史中声明完整 submodule 路径和 URL 的版本。

Fresh-clone submodule configuration was statically validated from the root mapping, URL, gitlink and checked-out pinned commit. A separate fresh clone was not executed in this milestone.

这里的静态验证是对提交前工作树中 mapping、URL、gitlink 和本地检出提交的一致性核对；本记录所在 revision 随后纳入了该 mapping，但没有执行独立 fresh clone，因此不能宣称实际 clone 行为已经测试成功。

## Fresh clone instructions

取得包含根 `.gitmodules` mapping 的 revision 时，新 clone 可以使用：

```bash
git clone --recurse-submodules <repository-url>
```

已有 clone 只需初始化当前锁定依赖：

```bash
git submodule update \
  --init \
  -- openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf
```

默认说明不要求无关 submodule 的递归下载，不应增加 `--remote`。

## Scope limitations

- 依赖源码完整性恢复不等于 OpenEB 已经可以编译或运行。
- 尚未执行 OpenEB CMake configure、build 或 install。
- 尚未验证 CMake 4.1、Boost、OpenCV、Protobuf 或 HDF5 的组合兼容性。
- 尚未验证 ECF codec/plugin 的编译、安装路径或运行时发现。
- 尚未验证 CLI、RAW、HDF5 读写、HAL plugin、真实相机或 CenturyArks 支持。
