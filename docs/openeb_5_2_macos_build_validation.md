# OpenEB 5.2.0 macOS isolated build validation

## Status

This document records the Milestone 2B base build and Milestone 2B.1 Apple install-RPATH validation performed on branch `build/macos-openeb-5.2-isolation`.

```text
Validation base commit: 883f63ee726166006331c192039f31d2399a4163
HDF5 ECF pinned commit: b982d908a0bc0afd9104d226607bedb1a11b2a95
Platform: macOS arm64
```

| Area | Result |
| --- | --- |
| Configure | Passed |
| Bootstrap build | Passed |
| Full configured-profile build | Passed |
| Repository-local install | Passed |
| Three required CLI tools without `DYLD_LIBRARY_PATH` | Passed |
| RAW validation | Passed |
| HDF5 write/readback and ECF decode | Passed |
| HAL discovery loading without a camera | Passed |
| Stable OpenEB 5.1.1 contamination | None found |
| CenturyArks/SilkyEvCam integration | Not started |
| Linux runtime regression | Not run |

These results do not complete Milestone 2. Real-camera acceptance and the separately scoped CenturyArks decision remain outstanding.

## Scope and preserved baseline

The first OpenEB 5.2 build used the primary `Release`/arm64 profile and installed to repository-local paths. It passed configure, bootstrap build, full build, install, CLI identity, RAW, and HDF5 validation when the process supplied the repository library directory through `DYLD_LIBRARY_PATH`.

That successful baseline was preserved unchanged. The RPATH fix was rebuilt and installed in separate repository-local validation directories:

```text
$REPO_ROOT/.build/openeb-5.2.0-macos-rpath
$REPO_ROOT/.deps/openeb-5.2.0-macos-rpath
$REPO_ROOT/.tmp/openeb-5.2.0-macos-rpath
$REPO_ROOT/.logs/openeb-5.2.0-macos-rpath
$REPO_ROOT/.artifacts/openeb-5.2.0-macos-rpath
```

No system prefix, external OpenEB installation, or CenturyArks source was used or modified.

## Baseline RPATH evidence and root cause

The baseline installation contained 23 Mach-O objects: 13 executables and 10 dylibs. All 13 installed executables and the SDK/HAL dylibs in `$prefix/lib` lacked `LC_RPATH`. The standard Prophesee HAL plugin was the only object with an RPATH, and it contained only `@loader_path`.

The install names were already correct `@rpath/...` names. Build products received CMake-generated absolute build-tree RPATH entries, but the install scripts removed those entries without adding a relative install-tree replacement because the relevant targets had an empty `INSTALL_RPATH`.

The original direct device-observation launch therefore failed before discovery with:

```text
Library not loaded: @rpath/libmetavision_hal_discovery.5.dylib
Reason: no LC_RPATH's found
```

No OpenEB/Metavision 5.1.1 header, library, package, or plugin selection was involved in this failure.

## Apple-only CMake patch

The fix sets target-specific, relative install RPATH entries and appends rather than overwrites any pre-existing target value.

| Target class | Installed location | Apple install RPATH |
| --- | --- | --- |
| `metavision_platform_info`, `metavision_file_info`, `metavision_file_to_hdf5` | `$prefix/bin` | `@executable_path/../lib` |
| SDK shared modules | `$prefix/lib` | `@loader_path` |
| HAL and HAL discovery dylibs | `$prefix/lib` | `@loader_path` |
| Prophesee hardware layer | `$prefix/lib/metavision/hal/plugins` | `@loader_path/../../..` |
| Prophesee HAL plugin | Same plugin directory | `@loader_path` and `@loader_path/../../..` |
| HDF5 ECF plugin | `$prefix/lib/hdf5/plugin` | `@loader_path/../..` |

The Prophesee plugin retains `@loader_path` to resolve the hardware-layer dylib in the same directory. The additional three-level path reaches `$prefix/lib`. The HDF5 plugin needs the two-level path because it directly depends on the repository-local `libhdf5_ecf_codec.1.dylib` installed in `$prefix/lib`.

The pinned `hdf5_ecf` submodule was not modified. Its target property is set by the tracked parent `3rdparty/CMakeLists.txt` after the dependency target is created.

Only `INSTALL_RPATH` behavior changed. Normal build-tree RPATH generation, install destinations, install names, target names, plugin discovery APIs, and dependency discovery remain unchanged.

## Linux and non-Apple behavior

Every new RPATH assignment is guarded by `if(APPLE)`. The existing non-Apple Prophesee-plugin `${ORIGIN}` branch is unchanged. The patch does not alter Linux linker flags, UDEV/V4L2/USB logic, install locations, or target construction.

Linux configure, build, install, and runtime regression checks were not executed in this milestone. The conclusion is therefore source-conditional analysis, not a Linux runtime test result.

## Configure and build profile

The RPATH validation reused the successful primary profile with only two intentional input differences: the Apple-only CMake patch and the new repository-local output paths.

```text
Generator: Unix Makefiles
Make program: GNU Make
Build type: Release
Architecture: arm64
Modules: base;core;stream;ui
BUILD_SAMPLES: ON
USE_PROTOBUF: ON
HDF5: enabled
BUILD_TESTING: OFF
Python bindings: OFF
Documentation: OFF
Coverage: OFF
LFS downloads: OFF
HAS_V4L2: FALSE
```

Configure, the four-target bootstrap build, the full configured-profile build, and repository-local install all exited successfully. The install contained 506 files, matching the preserved baseline layout, and all 23 Mach-O relative paths and dylib install names matched the baseline.

The bootstrap log contained 160 non-fatal warnings and the remaining full-build phase contained four non-fatal warnings, matching the previously observed warning categories. No fatal compile or link signature was found.

## Runtime environment without DYLD overrides

The validation process explicitly removed both variables:

```text
DYLD_LIBRARY_PATH
DYLD_FALLBACK_LIBRARY_PATH
```

It used only repository-local `HOME`, `TMPDIR`, CLI path, HAL plugin path, and HDF5 plugin path. HAL loading was restricted with:

```text
MV_HAL_PLUGIN_SEARCH_MODE=PLUGIN_PATH_ONLY
```

`metavision_platform_info --software` ran directly from the RPATH-fixed prefix and reported OpenEB/Metavision `5.2.0` with exit code zero.

## RAW regression

The tracked `algo/tests/sparklers.raw` file was copied into the new artifacts directory before execution. The source and copy had the same SHA-256:

```text
e84afbecdc07d2910ae846a4ae0ee246f5b9c97a53816c637d4f85c023d7c234
```

The no-DYLD `metavision_file_info` result matched the successful baseline:

```text
Encoding: EVT2
Duration: 95,871 us
CD events: 521,252
First timestamp: 0
Last timestamp: 95,871
Plugin: hal_plugin_prophesee
```

The generated `.tmp_index` remained beside the repository-local artifacts copy; the tracked RAW file was not modified.

## HDF5 regression

`metavision_file_to_hdf5` produced a 1,820,474-byte repository-local HDF5 file without either DYLD variable. OpenEB readback reported ECF encoding and the same duration, event count, and timestamps as the RAW input.

Homebrew `h5dump`, using only the repository-local `HDF5_PLUGIN_PATH`, successfully decoded `/CD/events`, confirmed a dataspace of 521,252 events, and read the first event. This validates that the installed HDF5 ECF plugin can resolve its repository-local codec through its own RPATH.

## Discovery smoke

With no camera connected, direct no-DYLD execution produced:

```text
metavision_platform_info --short   -> No Device Found, exit 0
metavision_platform_info --system  -> empty systems list, exit 0
```

The commands reached discovery logic and no longer failed while loading `libmetavision_hal_discovery.5.dylib`. This is only a loader/discovery smoke result. Device open, live events, facilities, parameter changes, shutdown, reconnect, and CenturyArks support were not tested.

## Final linkage result

All 23 installed Mach-O objects were re-audited with `otool -D`, `otool -L`, and `otool -l`.

```text
Required CLI @executable_path RPATH entries: 3
Same-directory @loader_path entries: 7
HAL plugin-to-prefix/lib entries: 2
HDF5 plugin-to-prefix/lib entries: 1
Forbidden /usr/local Metavision runtime paths: 0
Build-tree runtime paths: 0
Repository absolute runtime paths: 0
Install-name differences from baseline: 0
Install-file-layout differences from baseline: 0
```

Homebrew libraries and macOS system libraries/frameworks remain allowed external dependencies.

The configured profile installs ten additional sample executables that were not part of the three-CLI portability patch: `metavision_active_pixel_detection`, `metavision_camera_stream_slicer`, `metavision_file_cutter`, `metavision_file_to_csv`, `metavision_file_to_dat`, `metavision_file_to_video`, `metavision_riscv_logger`, `metavision_software_info`, `metavision_synced_camera_streams_slicer`, and `metavision_viewer`. Their baseline no-RPATH behavior was intentionally not broadened in this minimal change and must not be described as fixed.

## Disk and workspace

The RPATH validation duplicate was explicitly required to preserve the successful baseline. Final measurements were:

```text
Build tree: 96,252 KiB
Install prefix: 13,476 KiB
Logs: approximately 1,988 KiB
Artifacts: 3,844 KiB
Temporary HOME/TMP and syntax-check file: 16 KiB
Repository before Milestone 2B.1: 298,716 KiB
Repository after validation and documentation: approximately 414,320 KiB
Actual repository growth: approximately 115,604 KiB (112.89 MiB)
```

Growth remained well below the 1 GiB authorization threshold. Final available space was approximately 44.05 GiB, about 9.81 GiB above the 34.24 GiB protection line. The larger change in filesystem-reported free space was not attributed to repository-controlled files; project-controlled growth is reported from the repository `du` measurements above.

## Remaining work

- Decide whether portability should later be extended to the other installed sample executables.
- Run Linux regression checks in an appropriate Linux environment.
- Define and execute real-camera acceptance with hardware available.
- Keep CenturyArks integration in Milestone 2C; no CenturyArks source was read, copied, modified, or compiled here.
