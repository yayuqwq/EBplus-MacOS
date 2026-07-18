# OpenEB 5.2.0 macOS build audit

## 1. Purpose and scope

本文档记录 Milestone 2A 对仓库内 OpenEB / Metavision 5.2.0 的只读审计结果，目标是为下一轮 macOS Apple Silicon 隔离构建提供可执行边界、候选配置和空间预算。

本轮执行了 Git、源码、CMake、外部 OpenEB 5.1.1、现有系统依赖和磁盘占用检查。本轮没有执行 CMake configure、build、install、下载、Git submodule init/update、程序运行或真实设备测试；没有修改 `openeb/`、外部 5.1.1、`/usr/local` 或 `/opt/homebrew`。

本文中的构建参数和命令均为下一轮候选方案，不代表已经配置成功、编译成功或运行验证通过。

## 2. Audit metadata

| Item | Value |
| --- | --- |
| Branch | `build/macos-openeb-5.2-audit` |
| Repository commit | `51e40f322c92d643261853689927cb3681e1039d` |
| Audit date | 2026-07-18, Asia/Shanghai |
| Initial repository state | `main` clean，`origin/main...main = 0 0`，标准生成目录不存在 |
| Audit branch state before documentation edits | clean |
| Host | macOS 26.0.1 / Darwin 25.0.0 / arm64 |
| Repository OpenEB path | `/Users/yayuqwq/Metavision/EBplus-MacOS/openeb` |
| External stable source | `/Users/yayuqwq/Metavision/openeb` |
| External stable identity | branch `macos/openeb-5.1.1-working`，commit `01692d4c8a17c855358783eb4c21329f3492b6ef`，tag `macos-openeb-5.1.1-working` |
| External stable state | clean；只读检查前后未修改 |
| Configure performed | No |
| Build performed | No |
| Install performed | No |
| Download or submodule update performed | No |

## 3. OpenEB 5.2.0 source status

### Identity

`git -C openeb rev-parse --show-toplevel` 与根目录的 `git rev-parse --show-toplevel` 返回同一个 EBplus 根目录。`openeb/.git` 不存在，因此 `git -C openeb status`、`log`、`describe` 和 `remote` 都会向上解析到 EBplus 根仓库，不能被解释为独立 OpenEB 仓库状态或 OpenEB 自身的 tag/remote 证据。

综合 index、tree、导入历史和版本声明，当前身份如下：

| Item | Result | Evidence |
| --- | --- | --- |
| Version | OpenEB / Metavision 5.2.0 | `openeb/CMakeLists.txt:22` 声明 `project(metavision VERSION 5.2.0)` |
| Source form | EBplus 根仓库内 tracked subtree-style source snapshot | `git ls-tree HEAD openeb` 为普通 tree；`openeb/.git` 不存在 |
| Import commit | `613a498daeddecbe926741c78b7489740e867aa8` | 提交主题 `Add openeb as subtree (version 5.2.0)` |
| Imported OpenEB commit | `9003b5416676e78ba994d912087486cfa94fae73` | import commit 的第二父提交；主题为 5.2.0 release merge |
| Local OpenEB follow-up | `014f2454f7d0af0603e597b7cdb210b8e1418258` | 在 OpenEB 根 `CMakeLists.txt` 增加 5 行 GCC 15 `<cstdint>` workaround |
| Independent Git metadata | None | `openeb/.git` absent |
| Current dirty state | Clean | 根仓库 `git status --short -- openeb`、`git diff -- openeb` 均无输出 |
| Index composition | 1405 个 `100644`、1 个 `100755`、1 个 `160000` | `git ls-files --stage openeb` 统计 |

将 imported OpenEB commit 与当前 `openeb/` 比较，普通 tracked source 的已知差异只有上述 GCC 15 兼容修改。不能将这一结论扩展为“全部依赖完整”：HDF5 ECF gitlink 的内容当前缺失。

### EBplus integration

EBplus 顶层不会自动配置或构建仓库内 `openeb/`。顶层 `CMakeLists.txt:22` 仅执行：

```cmake
find_package(MetavisionSDK 5.2.0 REQUIRED COMPONENTS base core stream)
```

未找到 `add_subdirectory(openeb)`、FetchContent 或 ExternalProject 集成。因此后续必须先把 OpenEB 5.2.0 安装到项目隔离 prefix，再由 EBplus 显式发现该 prefix。

### Completeness conclusion

- 普通 tracked source 与导入的 5.2.0 release tree 可追踪，且当前无 dirty 修改。
- 不存在无法由根仓库解释的 nested `.git` 元数据。
- 源码依赖不完整：`hdf5_ecf` 是空且未填充的 gitlink，根仓库当前不能复原其锁定对象。

## 4. HDF5 gitlink investigation

### Confirmed facts

| Item | Result |
| --- | --- |
| Git index/tree mode | `160000` |
| Pinned commit | `b982d908a0bc0afd9104d226607bedb1a11b2a95` |
| Working-tree children | 0；目录存在但为空 |
| Root `.gitmodules` | absent |
| `openeb/.gitmodules` URL | `https://github.com/prophesee-ai/hdf5_ecf.git` |
| Path recorded by `openeb/.gitmodules` | `sdk/modules/stream/cpp/3rdparty/hdf5_ecf` |
| Root object database contains pinned commit | No；`git cat-file -e ...^{commit}` 返回 128 |
| Root `git submodule status` | 失败：没有完整根路径对应的 mapping |
| External 5.1.1 comparison | 同一 URL、同一 pinned commit |

`openeb/.gitmodules` 中的 path 是相对于原 OpenEB repository 的路径，不是 EBplus 根仓库 gitlink 的完整路径。它能够证明上游声明的来源 URL，但不能直接充当根仓库 mapping，也不能证明机械复制就是正确的长期管理方式。

当前错误可重复为：

```text
fatal: no submodule mapping found in .gitmodules for path 'openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf'
```

由于 URL 和 pinned commit 已通过仓库历史及外部 5.1.1 交叉确认，本轮不使用“source location unknown”的结论。未解决的是 EBplus 根仓库如何可复现地声明、获取和管理该 gitlink。

### Build impact

- `openeb/CMakeLists.txt:233-238` 在未设置 `HDF5_DISABLED` 时执行非 REQUIRED 的 `find_package(HDF5 1.10.2 COMPONENTS CXX)`。
- 当前系统已安装 HDF5 1.14.6，因此下一轮未禁用 HDF5 时预计会进入 HDF5 路径。
- `openeb/sdk/modules/stream/cpp/3rdparty/CMakeLists.txt:1-8` 在找到 HDF5 且 `hdf5_ecf/CMakeLists.txt` 缺失时，会在 configure 阶段执行 `git submodule update --init`，随后执行 `add_subdirectory(hdf5_ecf)`。
- 这会把依赖获取和工作树填充隐含在 configure 中；当前根 mapping 缺失，因此 HDF5-enabled configure 不是可复现的安全操作。

### Feature relationship

| Capability | HDF5 ECF required by static source | Result |
| --- | --- | --- |
| RAW file open/playback | No | 使用 `OfflineRawPrivate` 和 HAL rawfile discovery，不受 `HAS_HDF5` 控制 |
| Live camera and RAW recording | No | USB/HAL stream 与 `I_EventsStream::log_raw_data` 独立于 HDF5 |
| HDF5/H5 open | Yes | `.hdf5/.h5` path 受 `HAS_HDF5` 控制 |
| HDF5 export | Yes | EBplus 直接使用 `HDF5EventFileWriter` |
| ECF codec/plugin | Yes | `metavision_sdk_stream` 在 `HDF5_FOUND` 时链接 `hdf5_ecf_codec` |

### Resolution priority

1. 优先使用已确认的原项目 URL 与 pinned commit，设计适用于 EBplus 根仓库完整路径的可复现 dependency mapping。
2. 如果原 gitlink 模型无法可靠恢复，再评估将同一 pinned commit vendoring 为普通 tracked source；该路线变更更大，需单独审核来源、许可和更新方式。
3. `HDF5_DISABLED=ON` 只能作为明确接受 HDF5/H5 读取和导出缺失的诊断或阶段性方案，不能作为功能对齐的最终方案。

本轮没有创建根 `.gitmodules`、没有初始化 gitlink、没有下载依赖。

## 5. Candidate minimal build configuration

### Option audit

| Option / variable | Default | Purpose | Required for EBplus GUI | Required for RAW playback | Required for live camera | Required for CLI validation | Disk impact | Recommended M2B value | Evidence |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `CMAKE_BUILD_TYPE` | unset 时 OpenEB 强制 `Release` | 单配置构建类型 | Yes | Yes | Yes | Yes | Release 较低 | 显式 `Release` | `openeb/CMakeLists.txt:10-16` |
| `CMAKE_OSX_ARCHITECTURES` | OpenEB 未设置 | macOS architecture | Yes | Yes | Yes | Yes | arm64 避免 universal 近似双份产物 | `arm64` | CMake standard variable；5.1.1 cache 为空 |
| `CMAKE_INSTALL_PREFIX` | CMake 平台默认 | 安装根目录 | Yes | Yes | Yes | Yes | install footprint | `$REPO_ROOT/.deps/openeb-5.2.0-macos` | `openeb/CMakeLists.txt:61-67` |
| `BUILD_TESTING` | `OFF` | GTest/pytest/tests/datasets | No | No | No | No | High；测试数据文档约 1.5 GB | `OFF` | `openeb/CMakeLists.txt:18,225-367` |
| `BUILD_SAMPLES` | `ON` | 全部 HAL/SDK/standalone samples | No | library path No | library path No | OpenEB built-in CLI Yes | Medium/High；约 35 个 executable definitions | CLI profile `ON`；minimal SDK profile `OFF` | `openeb/CMakeLists.txt:191,487-496` 及各 module sample gates |
| `COMPILE_PYTHON3_BINDINGS` | `ON` | Python bindings，并追加 `core_ml` | No | No | No | C++ CLI No | High | `OFF` | `openeb/CMakeLists.txt:193,225-227,382-385` |
| `GENERATE_DOC` | `OFF` | Doxygen | No | No | No | No | Medium | `OFF` | `openeb/CMakeLists.txt:196,240-243,500-502` |
| `GENERATE_DOC_PYTHON_BINDINGS` | `GENERATE_DOC` 开启时默认 `ON` | Python binding docs | No | No | No | No | Low/Medium | `OFF` | `openeb/CMakeLists.txt:197` |
| `CODE_COVERAGE` | Release 下 `OFF` | coverage flags/output | No | No | No | No | High relative to Release | `OFF` | `openeb/CMakeLists.txt:194-195,369-378` |
| `LFS_DOWNLOAD_COMPILATION_RESOURCES` | `ON` | compilation LFS resources | No | No | No | No | Potentially high | `OFF` | `lfs_download.cmake:10-15`；当前快照函数会因 `GIT_LFS_NOT_AVAILABLE=True` 返回 |
| `LFS_DOWNLOAD_VALIDATION_RESOURCES` | `ON` | validation/test resources | No | No | No | tests only | Potentially very high | `OFF` | 同上 |
| `METAVISION_SELECTED_MODULES` | 依据 Python/platform 生成候选列表 | SDK module selection | `base/core/stream` | `base/core/stream` | stream + HAL | 取决于 sample graph | UI/core_ml 有明显增量 | minimal SDK：`base;core;stream`；CLI profile：`base;core;stream;ui` | `openeb/CMakeLists.txt:380-410` |
| `USE_PROTOBUF` | `ON` | camera/device state serialization | Camera config save/load 需要 | No | open/stream No | 当前候选 CLI No | Low/Medium | `ON` | `stream/cpp/src/protobuf/CMakeLists.txt:24-77`；关闭时 save/load 为 no-op |
| `HDF5_DISABLED` | 未定义/false；不是 `option()` | 跳过 HDF5 discovery | HDF5 GUI paths 要求未禁用 | RAW No | No | RAW CLI No | Medium + dependency uncertainty | 完整 profile 不设置；当前 prerequisite 未解决 | `openeb/CMakeLists.txt:233-238` |
| `USE_OPENGL_ES3` | `OFF` | UI module GL backend | EBplus Qt UI 不使用 | No | No | CLI sample profile 的 UI graph | Low；UI dependencies 另计 | CLI profile `OFF` | `sdk/modules/ui/cpp/lib/CMakeLists.txt:38-71` |
| `HAS_V4L2` | Linux header 探测生成 cache bool | Linux V4L2/DMA path | No | No | macOS USB path No | No | Low | 不手工设置；macOS 应自动为 false | `hal_psee_plugins/CMakeLists.txt:15-28` |
| `UDEV_RULES_SYSTEM_INSTALL` | 仅非 Apple 平台定义 | Linux udev rules | N/A on macOS | N/A | N/A | N/A | None on macOS | 不传 | `hal_psee_plugins/resources/rules/CMakeLists.txt:10-30` |

未找到通用 benchmark build option。`BUILD_APPS` 只被读取但没有定义；`BUILD_INTERNAL_PLUGINS` 只在 internal source 存在时派生；两者均不得写入候选命令。HAL 与 `hal_psee_plugins` 由根 CMake 自动加入，没有公开的总开关或逐传感器开关。

### Samples and CLI constraint

`BUILD_SAMPLES=ON` 会无条件配置 Core samples，其中多个直接链接 `MetavisionSDK::ui`。因此以下组合不能被静态证明为安全：

```text
BUILD_SAMPLES=ON
METAVISION_SELECTED_MODULES=base;core;stream
```

当前未发现“只启用若干 CLI”的 configure option。可用的两个 profile 为：

| Profile | Modules | Samples | Result |
| --- | --- | --- | --- |
| Minimal EBplus SDK | `base;core;stream` | `OFF` | 最小 library/HAL/plugin profile，但没有 OpenEB built-in CLI targets，不能单独满足当前 M2B CLI 验收 |
| M2B CLI validation candidate | `base;core;stream;ui` | `ON` | 保留 OpenEB CLI；额外需要 OpenGL、GLEW、GLFW，并配置全部 sample targets |

由于当前 Milestone 2 完成标准明确要求 OpenEB C++ CLI、RAW 和真实相机结果，下一轮的主候选采用第二个 profile。建议先只构建以下最小验证 targets，再决定是否执行满足完整 install rules 的全量 enabled-target build：

| Target | Purpose | Install behavior |
| --- | --- | --- |
| `hal_plugins` | 构建 PSEE hw layer 和 `hal_plugin_prophesee` | plugins 安装到 prefix 的 `lib/metavision/hal/plugins` |
| `metavision_platform_info` | `--software` 报告 SDK 版本；`--short`/`--system` 枚举并打开设备 | 安装到 `bin` |
| `metavision_file_info` | 打开、读取并统计 RAW/DAT/HDF5 文件 | 安装到 `bin` |
| `metavision_file_to_hdf5` | 在 `HDF5_FOUND` 时把 RAW/DAT 转换为 HDF5，用于验证 ECF 写入与回读链路 | 安装到 `bin` |

### Candidate profile summary

```text
Generator: Unix Makefiles
Architecture: arm64
Build type: Release
Build tree: $REPO_ROOT/.build/openeb-5.2.0-macos
Install prefix: $REPO_ROOT/.deps/openeb-5.2.0-macos
Modules: base;core;stream;ui
Samples: ON, because current M2B requires built-in CLI validation
Tests: OFF
Python bindings: OFF
Documentation: OFF
Coverage: OFF
LFS downloads: OFF
Protobuf: ON
OpenGL ES3: OFF
HDF5: intended enabled after hdf5_ecf source completeness is resolved
HAL/PSEE plugins: automatically included
```

`Unix Makefiles` is selected as the compatibility-first candidate because the stable 5.1.1 build used it with CMake 4.1.2 and GNU Make on the same arm64 host. Ninja 1.13.1 is present but has no existing OpenEB success evidence.

## 6. OpenEB 5.1.1 patch comparison

Commit `01692d4` contains 19 affected files. Its dominant purpose is CenturyArks/SilkyEvCam device support rather than a generic macOS port. In particular, it does not change generic libusb include paths, claim/release calls or timeout types.

### Category summary

| Category | Finding |
| --- | --- |
| Build-system compatibility | A small warning-suppression hunk needs investigation; sample alias/install changes are not required |
| libusb/macOS portability | No generic macOS libusb patch in `01692d4`; 5.2 uses cross-platform libusb APIs |
| Dynamic library/plugin handling | 5.2 already has Apple `.dylib` loading, skips ELF `-z,defs`, and uses `@loader_path` RPATH |
| Device identification | CenturyArks system-info and VID/PID logic are absent in 5.2 and require adapted porting if that hardware is in scope |
| Vendor/product IDs | `31f7:0002/0003/0004` are not registered by 5.2 |
| Compiler compatibility | Only braces/unused-variable suppression was found; necessity is unproven without a build |
| Sample-only changes | Extra branded target, deleted standalone recipe and link rename are not part of the minimal port |
| Ignore/local environment changes | No action |

### Per-file mapping

| 5.1.1 file | Purpose | Equivalent 5.2.0 status | Likely action | Risk and required validation |
| --- | --- | --- | --- | --- |
| `.gitignore` | Ignore `prophesee/` and `.DS_Store` | File exists; entries absent | No action | Local hygiene only; do not mix into OpenEB port |
| `hal/cpp/samples/metavision_platform_info/CMakeLists.txt` | Add `silkyevcam_platform_info` alias target | Standard target exists; alias absent | Not applicable | Extra sample scope and no install rule |
| `hal/cpp/samples/metavision_platform_info/CMakeLists.txt.install` | Delete standalone install recipe | 5.2 recipe exists | No action | Deletion would reduce Linux/package capability |
| `hal/cpp/samples/metavision_platform_info/metavision_platform_info.cpp` | Add CenturyArks IDs to Linux USB diagnosis | Same file exists; diagnosis is Linux-only | Not applicable | Does not affect macOS enumeration |
| `hal_psee_plugins/lib/CMakeLists.txt` | Rename hw layer and replace standard plugin with vendor plugin | Standard target/plugin remain; Apple RPATH fix already present | Port with adaptation | Adapt only vendor-plugin construction if required; the global hw-layer rename itself is No action |
| `hal_psee_plugins/psee_hw_layer_headers/include/metavision/psee_hw_layer/boards/treuzell/tz_libusb_board_command.h` | Add pixel-mask EEPROM API | Class exists; method absent | Port with adaptation | Validate protocol, error handling and real hardware |
| `hal_psee_plugins/psee_hw_layer_headers/include/metavision/psee_hw_layer/boards/utils/psee_libusb.h` | Add 4-byte EEPROM wrapper | Class exists; method absent | Port with adaptation | Vendor feature, not generic libusb fix |
| `hal_psee_plugins/resources/rules/ca_device.rules` | Linux udev permission rule for VID 31f7 | File absent | Not applicable | macOS has no udev; Linux support is separate |
| `hal_psee_plugins/samples/metavision_imx636_facility_casting_sample/CMakeLists.txt` | Link renamed vendor hw layer | Sample exists; standard target used | Not applicable | Only needed if target rename is adopted, which is not recommended |
| `hal_psee_plugins/src/boards/treuzell/tz_hw_identification.cpp` | Custom firmware/system identity fields | Generic implementation exists; vendor behavior absent | Port with adaptation | Fragile name parsing; validate three vendor models and non-vendor devices |
| `hal_psee_plugins/src/boards/treuzell/tz_libusb_board_command.cpp` | Decode EEPROM pixel masks | File exists; hunk absent | Port with adaptation | Must fit 5.2 device/facility lifecycle |
| `hal_psee_plugins/src/boards/utils/psee_libusb.cpp` | Implement EEPROM read using `I2cEeprom` | Generic `I2cEeprom::read` exists; wrapper absent | Port with adaptation | Review buffer/error semantics and USB behavior |
| `hal_psee_plugins/src/devices/CMakeLists.txt` | Remove `v4l2` from device list | 5.2 keeps it but guarded subdirs return when `HAS_V4L2` is false | No action | Equivalent platform isolation already exists; confirm configure result later |
| `hal_psee_plugins/src/devices/imx636/imx636_tz_device.cpp` | Initialize 64 digital masks from EEPROM | 5.2 creates mask facility in `spawn_facilities()` | Port with adaptation | Old constructor hunk cannot be copied; initialize the registered facility instance |
| `hal_psee_plugins/src/devices/imx646/imx646_tz_device.cpp` | Same EEPROM mask behavior | Same 5.2 lifecycle change | Port with adaptation | Validate coordinates, failures and non-vendor devices |
| `hal_psee_plugins/src/devices/others/CMakeLists.txt` | Move `i2c_eeprom.cpp` into hw-layer object | 5.2 still places it in plugin object | Port with adaptation | Only with EEPROM feature; avoid duplicate symbols/link regressions |
| `hal_psee_plugins/src/plugin/CMakeLists.txt` | Replace universal plugin source | 5.2 uses standard `psee_universal.cpp` | Port with adaptation | Prefer extension or side-by-side vendor plugin; do not disable standard discovery |
| `hal_psee_plugins/src/plugin/silky_common.cpp` | Register VID/PID 31f7 and CenturyArks integrator | File/IDs absent; custom integrator overload exists | Port with adaptation | Avoid duplicate RAW discovery and plugin load conflicts |
| `hal_psee_plugins/src/utils/tz_device_control.cpp` | Add braces and `(void)main_dev` | Original structure remains | Needs investigation | No AppleClang diagnostic was produced in this audit |

### Comparison conclusion

- Already present in 5.2: Apple plugin RPATH, `.dylib` loading, non-Apple-only `-z,defs`, V4L2 early return, custom integrator overload, generic I2C EEPROM and digital-mask facilities.
- Reusable only with adaptation: CenturyArks discovery, integrator identity, system information and EEPROM pixel-mask chain.
- Not applicable to the macOS port: Linux udev, Linux `lsusb` diagnostics, sample alias/install deletion and replacing the standard plugin wholesale.
- Current 5.2 plugin does not register `31f7:0002/0003/0004`; live-camera support for those devices is a product/hardware risk requiring a separate minimal patch and real-device validation.

## 7. Existing system dependencies

### Tools and libraries

| Dependency | Required version / use | Installed version | Discovery method | Architecture | Present | Potential conflict | Action before M2B |
| --- | --- | --- | --- | --- | --- | --- | --- |
| CMake | OpenEB minimum 3.5 | 4.1.2 | `/opt/homebrew/bin/cmake --version` | arm64 host | Yes | Much newer than project release; compatibility untested | None to install; validate at configure |
| GNU Make | Unix Makefiles generator | Homebrew gmake 4.4.1; system make 3.81 | command/cache | arm64 | Yes | Reusable commands must not hardcode its prefix | No install; select with `command -v gmake` and verify at configure |
| Ninja | Alternative generator | 1.13.1 | command | arm64 | Yes | No OpenEB success evidence | No action; not primary candidate |
| AppleClang | C++17 | 15.0.0 | `clang --version` | arm64 | Yes | No build evidence yet | Validate warnings/errors in M2B |
| LibUSB | REQUIRED, no minimum declared | 1.0.29 | pkg-config / `FindLibUSB.cmake` | arm64 dylib | Yes | macOS claim/detach behavior requires hardware test | No install action |
| Boost | program_options, timer, chrono, thread | 1.89.0 | Boost CMake config | arm64 dylibs | Yes | Newer than release-era baseline | Validate configure/build |
| OpenCV | core, highgui, imgproc, videoio, imgcodecs, calib3d, objdetect | 4.12.0 | OpenCVConfig/pkg-config | arm64 dylib | Yes | Unconditionally required even in minimal profile | Validate configure/build |
| HDF5 | >=1.10.2 CXX | 1.14.6 | HDF5 CMake config/pkg-config | arm64 dylib | Yes | Presence activates missing `hdf5_ecf` path | Resolve gitlink first |
| `hdf5_ecf` | exact pinned source commit | Working tree missing | Git index/history | N/A | No | Reproducible-source blocker | Requires separate source/download approval |
| Protobuf | Required when `USE_PROTOBUF=ON` | 33.0 | protobuf CMake config/pkg-config | arm64 dylib | Yes | Newer version; codegen compatibility untested | Validate configure/build |
| OpenGL | Required by CLI profile `ui` | macOS framework | CMake `find_package(OpenGL)` | arm64 system | Yes | Deprecated platform API, still available | Validate configure/build |
| GLEW | Required by `ui` | 2.2.0_1 | pkg-config/CMake | arm64 dylib | Yes | Only needed because samples pull `ui` | Validate configure/build |
| GLFW | Required by `ui` | 3.4 | pkg-config/CMake | arm64 dylib | Yes | Same as above | Validate configure/build |
| Python/GTest/Doxygen | Only selected features | Installed, but not required | Homebrew | arm64 where applicable | Yes | Would expand scope and disk | Keep disabled |

当前只读证据没有证明 M2B 前需要执行 `brew install` 或 `brew upgrade`。主要不确定性是这些依赖普遍比 OpenEB 5.2 发布期更新，必须通过实际 configure/build 才能判断兼容性。

### 5.1.1 CMakeCache intent

| Classification | Items |
| --- | --- |
| Reusable intent | Release；Unix Makefiles；AppleClang；tests OFF；docs OFF；Homebrew 提供 libusb/HDF5/Boost/OpenCV/Protobuf；需要 built-in CLI 时 samples ON |
| Machine-specific cached path | `/usr/local` install prefix、`/opt/homebrew` prefix/Cellar paths、外部 Python venv、外部 build/HDF5 plugin paths |
| Obsolete 5.1.1 option | 本轮核对的主要选项在 5.2 仍存在；没有确认可直接标为 obsolete 的候选参数 |
| Needs confirmation in 5.2.0 | Python 应从 ON 改 OFF；LFS 两项应从 ON 改 OFF；samples/UI coupling；Protobuf 33；HDF5 gitlink；explicit arm64；generator/make program |

不得复制整个 5.1.1 Cache，也不得把其中的个人、Cellar、`/usr/local` 或外部 venv 路径写入自动化命令。

## 8. Disk estimate

### Evidence and comparability

| Evidence | Size | Quality | Comparability to M2B |
| --- | ---: | --- | --- |
| Repository OpenEB 5.2 source | 60,420 KiB / about 59 MiB | High, current source measurement | Already present; not future growth |
| External 5.1.1 build tree | 208,396 KiB / about 203.5 MiB | High, same-host measurement | Medium/Low: Release, samples ON, tests OFF, Python ON, different source/modules |
| External 5.1.1 install manifest existing files | 17,976 KiB / about 17.6 MiB | High historical install reference | Medium: does not include package-manager dependencies or system caches |
| External 5.1.1 total source | about 1.33 GiB | High measurement, poor relevance | Not usable as build estimate: includes about 989 MiB `prophesee/`, 108.5 MiB `.git`, and build tree |
| External same-commit `hdf5_ecf` reference | source 128 KiB；build CMake subtree 1,004 KiB；build plugin dir 76 KiB；matching regular dylibs about 40 KiB | High same-commit measurement | Medium comparability: 5.2 configure/build behavior and Git transfer metadata remain unmeasured |

The 5.1.1 figures are historical references only. The estimate adjusts downward for Python/tests/docs being disabled and upward for 5.2 uncertainty, samples/UI coupling, missing dependency recovery and unmeasured intermediate files.

### Estimated new growth

| Estimate | Build tree | Install prefix | Missing dependency/source | Logs/temp | General contingency | Total | Basis and uncertainty |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| Minimum likely | 0.20 GiB | 0.02 GiB | <0.01 GiB | 0.03 GiB | 0 GiB | **0.25 GiB** | Minimal successful path; rounds the same-commit dependency reference to a few MiB |
| Expected | 0.35 GiB | 0.04 GiB | 0.01 GiB | 0.03 GiB | 0.07 GiB | **0.50 GiB** | Release/arm64, tests/Python/docs off, CLI/UI profile and limited allowance for unmeasured 5.2 intermediates |
| Conservative upper bound | 0.50 GiB | 0.06 GiB | 0.02 GiB | 0.07 GiB | 0.10 GiB | **0.75 GiB** | More than doubles the measured 5.1.1 build allowance while accounting for 5.2/toolchain uncertainty, enabled CLI/UI samples and repository-local temporary/artifact space |

No Debug or universal build is planned. Release is expected to avoid default debug-symbol growth.

### Protection-line result

| Item | Value |
| --- | ---: |
| Disk total | 228.27 GiB |
| Current available | 46.38 GiB |
| 15% threshold | 34.24 GiB |
| 20 GiB threshold | 20 GiB |
| Effective protection line | 34.24 GiB |
| Projected available, minimum | 46.13 GiB |
| Projected available, expected | 45.88 GiB |
| Projected available, conservative | 45.63 GiB |
| Conservative remaining margin above protection line | about 11.39 GiB |

The remaining-space protection line is not a blocker. The current expected and conservative estimates are both below 1 GiB, so this evidence does not trigger the policy's size-based authorization threshold. M2B must repeat the estimate before creating directories; if the refreshed expected or upper estimate reaches 1 GiB, obtain separate operation-specific authorization.

## 9. Proposed Milestone 2B sequence

1. Resolve source dependency completeness before configure; do not rely on configure-time implicit submodule update.
2. Approve the selected dependency-recovery operation and any network/download it requires; re-estimate disk growth and obtain the separate >=1 GiB authorization only if the refreshed estimate reaches that threshold.
3. Re-run Git and disk preflight; confirm no duplicate OpenEB build tree.
4. Create only `$REPO_ROOT/.build/openeb-5.2.0-macos` and repository-local support directories needed by the approved operation.
5. Configure the arm64 Release M2B CLI profile with explicit dependency prefix, repository-local install prefix, tests/Python/docs/LFS disabled, Protobuf enabled and HDF5 source complete.
6. Review configure output and `CMakeCache.txt`, including compiler, architecture, HDF5 ECF, HAS_V4L2, modules, dependency paths and install prefix.
7. First build `hal_plugins`, `metavision_platform_info`, `metavision_file_info` and `metavision_file_to_hdf5` plus their dependencies.
8. Review disk growth and build output; then build the remaining enabled targets required for a reliable install.
9. Install only to `$REPO_ROOT/.deps/openeb-5.2.0-macos`.
10. Verify the ordinary terminal still resolves the stable 5.1.1 environment.
11. Validate 5.2 with `MV_HAL_PLUGIN_PATH` set to the project prefix and `MV_HAL_PLUGIN_SEARCH_MODE=PLUGIN_PATH_ONLY`.
12. Verify OpenEB version/CLI, RAW open/read, live device enumeration/open/stream/facilities/shutdown/reconnect and HDF5 behavior separately.
13. Inspect `.dylib` linkage and RPATH with `otool -L`/`otool -l`; confirm no 5.1.1 library or plugin mixing.
14. Record final build/install/log sizes and actual disk growth.

For live validation, `ResourcesFolder::get_user_path()` can create `~/Library/Application Support/Metavision/hal`, notably on GenX320 paths. M2B must use a reviewed repository-local process `HOME`/temporary strategy or obtain explicit authorization before running a path that would create this external directory.

## 10. Blocking issues

### Blocker

**HDF5-enabled configure is not currently reproducible.** 当前系统存在 HDF5 1.14.6，静态证据表明未禁用 HDF5 时预计会进入该路径；一旦 `HDF5_FOUND` 为真，OpenEB CMake 将尝试填充空的 `hdf5_ecf` gitlink，但 EBplus 根仓库没有匹配的 mapping，且缺少 pinned object。Configure was not executed in this audit.

This is not a static RAW or live-camera source blocker, but it blocks the intended HDF5-complete M2B configuration.

### Required execution prerequisites, not technical blockers

- The current 0.75 GiB conservative estimate does not trigger the size threshold, but M2B must refresh it before creating directories and stop for separate authorization if it reaches 1 GiB.
- If the selected dependency-recovery design fetches the pinned object from the remote URL, that network/download operation requires separate explicit approval.

### Risks and open questions

- Samples have no fine-grained configure switch; built-in CLI validation pulls the `ui` module and OpenGL/GLEW/GLFW.
- CMake 4.1.2, Boost 1.89, OpenCV 4.12, Protobuf 33 and HDF5 1.14.6 are newer than the release-era baseline and are unvalidated.
- CenturyArks IDs `31f7:0002/0003/0004` and EEPROM mask behavior are absent from 5.2; target hardware may require adapted porting.
- EBplus checks a `MetavisionSDK::hal` target, while this OpenEB source exports `Metavision::HAL` and `Metavision::HAL_discovery`; actual transitive HAL propagation must be checked later.
- No runtime evidence exists for libusb interface claiming, device shutdown, plugin loading, RAW, HDF5 or CLI behavior on this 5.2 source.

## 11. Go/no-go recommendation

**Go with prerequisites**

Reasons:

- The OpenEB 5.2 source identity and ordinary tracked source are traceable.
- Required system tools and libraries are present as arm64 installations; no install/upgrade action is currently proven necessary.
- RAW and live-camera paths are statically independent of HDF5 ECF.
- The intended HDF5-complete configuration is not ready until the locked dependency is restored reproducibly.
- M2B must receive explicit authorization for the selected dependency-recovery operation and any required network/download, and must account for samples/UI scope, dependency-version risk and target-camera vendor support.

Milestone 2B must not begin configure until the HDF5 source prerequisite is resolved and the refreshed disk estimate is recorded; separate size-based authorization is required if that estimate reaches 1 GiB.

## 12. Post-audit dependency recovery status

This section is a chronological follow-up to the original Milestone 2A snapshot above. It does not remove or rewrite the evidence and conclusions recorded at audit time.

On branch `build/macos-openeb-5.2-hdf5-dependency`, the root repository gained a tracked `.gitmodules` mapping for:

```text
Path: openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf
URL: https://github.com/prophesee-ai/hdf5_ecf.git
Pinned commit: b982d908a0bc0afd9104d226607bedb1a11b2a95
```

The path-limited `git submodule update --init -- <path>` operation checked out that exact commit. The submodule worktree is clean and detached, its remote URL matches the audited upstream, and its absolute Git directory is inside `$REPO_ROOT/.git/modules/`. The root gitlink remains mode `160000` at the same pinned commit.

The source-completeness portion of the original blocker is therefore resolved locally: the root mapping exists and the checked-out source no longer depends on configure-time implicit submodule initialization. The dependency uses Apache License 2.0; the recovered worktree measured 128 KiB and its Git metadata measured 148 KiB at recovery time.

Fresh-clone submodule configuration was statically validated from the root mapping, URL, gitlink and checked-out pinned commit. A separate fresh clone was not executed in this milestone.

That static cross-check was performed in the dependency-recovery worktree before commit. The same recovery revision now tracks the root mapping, but fresh-clone behavior remains unexecuted and must not be reported as an actual clone test.

Configure and build readiness remain unproven. This recovery did not run CMake configure, build or install, and it did not validate current CMake/Boost/OpenCV/Protobuf/HDF5 compatibility, ECF codec/plugin integration, HAL/plugin behavior, RAW/HDF5 I/O, CLI, live camera or CenturyArks support. The next build stage still requires renewed Git/dependency/disk preflight and separate user authorization.
