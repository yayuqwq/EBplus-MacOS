# CenturyArks OpenEB 5.2 side-by-side overlay Phase 1 build and validation

## 1. Status and scope

This document records the Milestone 2C-B1 implementation and validation originally performed on branch:

```text
feat/centuryarks-openeb-5.2-overlay-phase1
```

The Phase 1 result is an isolated prepared OpenEB 5.2 source tree that always builds and installs two live-device HAL plugins beside one shared PSEE hardware layer:

```text
hal_plugin_prophesee
hal_plugin_centuryarks
metavision_psee_hw_layer
```

| Area | Result |
| --- | --- |
| Deterministic prepared source | Passed |
| Configure, arm64/Release | Passed |
| Bootstrap build | Passed |
| Full configured-profile build | Passed |
| Repository-local install | Passed |
| Standard Prophesee plugin retained | Passed |
| CenturyArks side plugin installed | Passed |
| Single shared hardware-layer ownership | Passed |
| Loader-only two-plugin probe | Passed |
| Three required CLI without `DYLD_*` | Passed |
| RAW and HDF5/ECF regression | Passed |
| Mach-O linkage and RPATH audit | Passed, 14 objects |
| Stable OpenEB 5.1.1 contamination | None found |
| Physical `31f7:0003` enumeration/open | Passed |
| Physical `31f7:0003` single-process reopen | Passed after 3 seconds |
| Physical `31f7:0002` and `31f7:0004` | Registered; hardware not tested |
| Live event delivery | Not run |
| Linux configure/build/runtime | Not run |

This is not a claim of complete CenturyArks camera support. Phase 1 validates source delivery, build/install topology, plugin loading, OpenEB 5.2 enumeration and device open for one physical `31f7:0003` camera. It does not validate event streaming, recording, EEPROM, pixel masks, facility-specific vendor behavior, parameter changes, firmware operations, multiple simultaneous cameras, physical reconnect, or the other two registered PIDs.

## 2. Delivered architecture

The canonical tracked `openeb/` tree is not patched in place. The delivery flow is:

```text
tracked openeb/
+ tracked CenturyArks source-delivery inputs
+ tracked two-hunk OpenEB 5.2 patch
        |
        v
.tmp/openeb-5.2.0-centuryarks-source/
        |
        v
.build/openeb-5.2.0-centuryarks-macos/
        |
        v
.deps/openeb-5.2.0-centuryarks-macos/
```

The validated target ownership is:

```text
metavision_psee_hw_layer_obj (55 object files, compiled once)
        |
        v
metavision_psee_hw_layer.dylib
        |                         |
        v                         v
hal_plugin_prophesee.dylib   hal_plugin_centuryarks.dylib
```

The plugin-common object library continues to be consumed through the existing OpenEB plugin loop. Neither plugin DSO directly embeds `metavision_psee_hw_layer_obj`; both link the single `metavision_psee_hw_layer` shared library. No `metavision_silky_hw_layer` target or dylib was created.

The standard plugin was neither replaced nor reduced. A normalized comparison of its link command, including its RPATH inputs, against the validated non-CenturyArks OpenEB 5.2 baseline produced a zero-byte diff.

## 3. Source identity and licensing

The tracked provenance record is:

```text
third_party/centuryarks/silkyevcam-openeb-5x/SOURCE_IDENTITY.md
third_party/centuryarks/silkyevcam-openeb-5x/manifests/vendor-source.sha256
```

Relevant audited identities are:

```text
Supplier package: SilkyEvCam_plugin_Source_for_MV511
Audited package: 21 regular files, 135,102 bytes
Audited OpenEB upstream commit: 9003b5416676e78ba994d912087486cfa94fae73
Canonical OpenEB tree: b407c407aa46d3b97edc9b2096fb120a96c8b465
OpenEB declared version: 5.2.0
HDF5 ECF commit: b982d908a0bc0afd9104d226607bedb1a11b2a95
```

Only two supplier-derived inputs were retained:

- `LICENSE_OPEN`, copied byte-for-byte from the supplier package;
- `centuryarks_plugin.cpp`, minimally derived from the supplied `silky_common.cpp` entrypoint.

Hashes used by the preparation contract are:

```text
Supplied silky_common.cpp:
461a0c8d405d2820e31b3c08715974a9a7136cada2c8855e995b87fa46745fe4

Supplied and tracked LICENSE_OPEN:
ab1119dedc6ca90aef94f95ad78b10580cca0a1de76e3ca3052b166be8399f03

Tracked adapted centuryarks_plugin.cpp:
a818d8cb77e592da577a35bad0b5de93cea7f27451929bf134b471b3fa62d93b
```

The adapted entrypoint retains the Prophesee and CenturyArks copyright notices, the Apache-2.0 header, and a 2026 EBplus modification notice. The supplied license evidence permits redistribution under its stated terms; notices and modification identification must be retained. No official certification, endorsement, trademark permission, or broader legal conclusion is asserted.

The complete audited supplier manifest was regenerated before import and matched the earlier Milestone 2C-A manifest. Normal preparation does not read or depend on the external Downloads directory.

## 4. Minimal adapted entrypoint and patch

`centuryarks_plugin.cpp` performs only the Phase 1 registration work:

```text
Integrator: CenturyArks
Camera discovery: existing TzCameraDiscovery
USB IDs: 31f7:0002, 31f7:0003, 31f7:0004
Interface class: 0xff, supplied by TzCameraDiscovery
Interface subclass: 0x19
Interface protocol: 0x00, supplied by the existing Treuzell USB path
```

It has no serial filter. OpenEB forms the full identity from runtime values:

```text
CenturyArks:hal_plugin_centuryarks:<runtime serial>
```

It does not contain the tested serial, its hash, a product-name gate, firmware gate, PID-to-model mapping, EEPROM logic, pixel-mask logic, facility override, standard Prophesee VID/PID, or `PseeFileDiscovery` registration.

The tracked patch changes only:

```text
hal_psee_plugins/lib/CMakeLists.txt
hal_psee_plugins/src/plugin/CMakeLists.txt
```

Its two semantic hunks:

1. append `hal_plugin_centuryarks` to the existing unconditional plugin list;
2. add `centuryarks_plugin.cpp` as that target's entrypoint source.

The patch adds no option, hardware-layer target, direct object expansion, target-specific link command, install rule, or RPATH rule. Its two insertion hunks use zero context so the tracked patch file itself contains no trailing-whitespace context lines. The preparation script first verifies the exact canonical file hashes, then applies the patch with `--unidiff-zero`; offset and fuzz remain rejected. Patch checking and application completed cleanly.

## 5. Preparation script and integrity result

The repository script is:

```text
scripts/prepare_centuryarks_openeb_source.sh
```

It rejects detached HEAD, verifies the allowed worktree scope, canonical OpenEB tree and file hashes, version `5.2.0`, validated Apple RPATH hunks, the HDF5 ECF gitlink and checked-out commit, tracked vendor provenance inputs, adapted source hash, and exact two-hunk patch contract. It refuses to overwrite an existing prepared source. An existing preparation manifest is accepted only when it is a regular non-symlink file and is refreshed after successful materialization. Source identity is enforced by the exact canonical tree and file hashes rather than by a disposable feature-branch name, so the tracked tool remains usable after an ordinary merge and branch cleanup.

The script materializes canonical OpenEB using Git-controlled content, separately archives the pinned HDF5 ECF source as ordinary files, copies the tracked adapted entrypoint, checks the patch, and applies it. It does not copy root or submodule Git metadata, access Downloads, configure, build, download, or delete partial output.

The first invocation safely stopped before materialization because the script's exact patch-addition contract did not match the reviewed patch serialization. The failed contract check was preserved in `prepare.log`. After correcting that script assertion, the next invocation succeeded and generated `preparation-manifest.txt`. No partial source tree from the failed attempt had to be reused or overwritten.

For reproducibility closure, the authorized prepared-source directory was removed and regenerated with the final tracked script and final provenance inputs. `prepared-source-before-final-script.tsv` and `prepared-source-after-final-script.tsv` each contain 1,418 SHA-256, size and relative-path records; they are byte-for-byte identical and share manifest SHA-256 `54a730b7c9d3946339b535c955543666d7d65e0d3fae3e7725c88a006f4c78f1`. The regenerated `preparation-manifest.txt` records the final `SOURCE_IDENTITY.md`, adapted entrypoint and patch hashes. A second script invocation returned nonzero through the existing-output guard and left the prepared source unchanged. The existing build tree then successfully rebuilt `hal_plugin_prophesee` and `hal_plugin_centuryarks` from the final prepared source without duplicate-symbol or OpenEB 5.1.1 contamination evidence.

Because the regenerated prepared source is byte-identical to the source used for the completed full build, install, no-DYLD, RAW/HDF5, linkage and physical-camera acceptance, those prior results remain applicable without repeating the full validation suite. Live event delivery remains unvalidated.

Prepared-source comparison confirmed:

- the prepared source declares OpenEB `5.2.0`;
- `psee_universal.cpp` is byte-for-byte identical to canonical OpenEB;
- the existing Apple RPATH changes are present;
- `centuryarks_plugin.cpp` has the same hash as its tracked authoritative source;
- HDF5 ECF source is present at the pinned commit;
- no `.git` metadata is present;
- the only semantic OpenEB differences are the two reviewed CMake hunks and the new entrypoint, apart from the expected submodule materialization form.

## 6. Reproduction preflight

Run only from a non-detached branch whose canonical OpenEB identity matches the script contract, and only with empty output paths. Do not remove or reuse an existing output tree without a separate review and authorization.

```bash
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

PREPARED_SOURCE="$REPO_ROOT/.tmp/openeb-5.2.0-centuryarks-source"
BUILD_DIR="$REPO_ROOT/.build/openeb-5.2.0-centuryarks-macos"
INSTALL_PREFIX="$REPO_ROOT/.deps/openeb-5.2.0-centuryarks-macos"
LOG_DIR="$REPO_ROOT/.logs/openeb-5.2.0-centuryarks-macos"
ARTIFACT_DIR="$REPO_ROOT/.artifacts/openeb-5.2.0-centuryarks-macos"
RUNTIME_HOME="$REPO_ROOT/.tmp/openeb-5.2.0-centuryarks-runtime-home"
RUNTIME_TMP="$REPO_ROOT/.tmp/openeb-5.2.0-centuryarks-runtime-tmp"

test -n "$(git branch --show-current)"

git status --short --branch
git diff --check
test -z "$(git status --porcelain=v1 --untracked-files=all -- openeb)"

test "$(git -C openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf rev-parse HEAD)" = \
  "b982d908a0bc0afd9104d226607bedb1a11b2a95"
test -z "$(git -C openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf status --short)"

for output_path in \
  "$PREPARED_SOURCE" \
  "$BUILD_DIR" \
  "$INSTALL_PREFIX" \
  "$LOG_DIR" \
  "$ARTIFACT_DIR" \
  "$RUNTIME_HOME" \
  "$RUNTIME_TMP"
do
  test ! -e "$output_path"
done

df -h "$REPO_ROOT"
df -k "$REPO_ROOT"
du -sh "$REPO_ROOT"
du -sk "$REPO_ROOT"
```

Apply the repository workspace protection policy before proceeding. This validation estimated 0.15 GiB expected growth and a 0.30 GiB conservative upper bound; a future run must recalculate rather than reuse those values blindly.

## 7. Prepare, configure, build and install runbook

### Prepare source

The script creates the prepared source and writes its ignored preparation manifest after successful materialization. The validation created `LOG_DIR` first for external-package preflight evidence. If an earlier regular, non-symlink preparation manifest exists, the successful run refreshes it; the prepared-source existing-output guard remains authoritative.

```bash
mkdir -p "$LOG_DIR"
bash -n scripts/prepare_centuryarks_openeb_source.sh
scripts/prepare_centuryarks_openeb_source.sh
```

Do not rerun it over an existing prepared source. A normal successful run must not require the supplier Downloads directory.

### Configure environment

The observed validation used Homebrew as the explicitly supplied dependency prefix and `/usr/local` only as the stable OpenEB prefix to ignore. Reusable commands keep both locations caller-controlled:

```bash
: "${OPENEB_DEPENDENCY_PREFIX:?Set the reviewed dependency prefix}"
: "${STABLE_OPENEB_PREFIX:?Set the stable OpenEB prefix to ignore}"

MAKE_PROGRAM="${MAKE_PROGRAM:-$(command -v gmake)}"
test -n "$MAKE_PROGRAM"

PKG_CONFIG_LIBDIR_VALUE="$OPENEB_DEPENDENCY_PREFIX/lib/pkgconfig:$OPENEB_DEPENDENCY_PREFIX/share/pkgconfig"
DEPENDENCY_ENV=(
  env
  PKG_CONFIG_PATH=
  PKG_CONFIG_LIBDIR="$PKG_CONFIG_LIBDIR_VALUE"
)

mkdir -p "$LOG_DIR" "$ARTIFACT_DIR" "$RUNTIME_HOME" "$RUNTIME_TMP"
```

### Configure

```bash
"${DEPENDENCY_ENV[@]}" cmake \
  -S "$PREPARED_SOURCE" \
  -B "$BUILD_DIR" \
  -G "Unix Makefiles" \
  -DCMAKE_MAKE_PROGRAM="$MAKE_PROGRAM" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
  -DCMAKE_PREFIX_PATH="$OPENEB_DEPENDENCY_PREFIX" \
  -DCMAKE_IGNORE_PREFIX_PATH="$STABLE_OPENEB_PREFIX" \
  -DBUILD_TESTING=OFF \
  -DBUILD_SAMPLES=ON \
  -DCOMPILE_PYTHON3_BINDINGS=OFF \
  -DGENERATE_DOC=OFF \
  -DGENERATE_DOC_PYTHON_BINDINGS=OFF \
  -DCODE_COVERAGE=OFF \
  -DLFS_DOWNLOAD_COMPILATION_RESOURCES=OFF \
  -DLFS_DOWNLOAD_VALIDATION_RESOURCES=OFF \
  '-DMETAVISION_SELECTED_MODULES=base;core;stream;ui' \
  -DUSE_PROTOBUF=ON \
  -DUSE_OPENGL_ES3=OFF \
  2>&1 | tee "$LOG_DIR/configure.log"
```

The validated cache contained:

```text
Generator: Unix Makefiles
GNU Make: explicitly selected
Architecture: arm64
Build type: Release
Modules: base;core;stream;ui
BUILD_SAMPLES: ON
USE_PROTOBUF: ON
HDF5: enabled
BUILD_TESTING: OFF
Python: OFF
Documentation: OFF
Coverage: OFF
LFS downloads: OFF
USE_OPENGL_ES3: OFF
HAS_V4L2: FALSE
```

Configure generated all three required targets and no CenturyArks feature option or renamed hardware-layer target.

### Bootstrap and full build

```bash
cmake --build "$BUILD_DIR" \
  --parallel 8 \
  --verbose \
  --target \
    hal_plugin_prophesee \
    hal_plugin_centuryarks \
    metavision_psee_hw_layer \
    metavision_platform_info \
    metavision_file_info \
    metavision_file_to_hdf5 \
  2>&1 | tee "$LOG_DIR/build-bootstrap.log"

cmake --build "$BUILD_DIR" \
  --parallel 8 \
  --verbose \
  2>&1 | tee "$LOG_DIR/build-full.log"
```

Both stages completed successfully. No duplicate-symbol linker failure or warning was observed.

### Install

```bash
test "$(grep '^CMAKE_INSTALL_PREFIX:PATH=' "$BUILD_DIR/CMakeCache.txt")" = \
  "CMAKE_INSTALL_PREFIX:PATH=$INSTALL_PREFIX"

cmake --install "$BUILD_DIR" \
  2>&1 | tee "$LOG_DIR/install.log"
```

The install manifest contained 521 entries. The standard plugin directory contained:

```text
libhal_plugin_prophesee.dylib
libhal_plugin_centuryarks.dylib
libmetavision_psee_hw_layer.dylib
```

It did not contain a renamed Silky hardware layer or `libsilky_common_plugin.dylib`.

## 8. Target, object and registration audit

The generated target graph and build files established:

```text
hardware_object_target_dirs=1
hardware_shared_target_dirs=1
hardware_object_files=55
centuryarks_entrypoint_objects=1
prophesee_entrypoint_objects=1
direct plugin references to hardware-layer objects=0
shared hardware-layer references to hardware-layer objects=55
standard normalized link/RPATH comparison bytes=0
```

Registration was checked at several levels: adapted source, prepared target source lists, verbose compile and link commands, object symbols, arm64 instruction immediates, plugin metadata loading, and physical camera discovery.

The CenturyArks entrypoint contains exactly:

```text
31f7:0002 subclass 0x19
31f7:0003 subclass 0x19
31f7:0004 subclass 0x19
```

The shared Treuzell path supplies vendor-specific interface class `0xff` and protocol `0x00`. The compiled entrypoint contains the three VID/PID/subclass immediate sequences and one `initialize_plugin` symbol. It contains no file-discovery registration or serial filter.

The unchanged Prophesee entrypoint retained its existing registrations and compiled immediates, including its existing `03fd`, `04b4`, and `1fc9` registrations. It contains no `0x31f7` registration. The physical `31f7:0003` trace later confirmed that the standard plugin did not claim the device.

OpenEB's existing discovery implementation compares a caller-supplied serial against each board's runtime serial. An empty serial retains OpenEB's first-discovered-device behavior; enumeration order is not treated as a stable API. No tested serial or serial hash is stored in implementation source.

## 9. Repository-local runtime environment

All accepted OpenEB 5.2 runtime checks used repository-local plugin paths, `PLUGIN_PATH_ONLY`, and no dynamic-loader override:

```bash
PLUGIN_PATH="$INSTALL_PREFIX/lib/metavision/hal/plugins"
HDF5_PLUGIN_PATH_VALUE="$INSTALL_PREFIX/lib/hdf5/plugin"

RUNTIME_ENV=(
  env
  -u DYLD_LIBRARY_PATH
  -u DYLD_FALLBACK_LIBRARY_PATH
  HOME="$RUNTIME_HOME"
  TMPDIR="$RUNTIME_TMP"
  PATH="$INSTALL_PREFIX/bin:/usr/bin:/bin:/usr/sbin:/sbin"
  MV_HAL_PLUGIN_PATH="$PLUGIN_PATH"
  MV_HAL_PLUGIN_SEARCH_MODE=PLUGIN_PATH_ONLY
  HDF5_PLUGIN_PATH="$HDF5_PLUGIN_PATH_VALUE"
)

"${RUNTIME_ENV[@]}" sh -c '
  test -z "${DYLD_LIBRARY_PATH-}"
  test -z "${DYLD_FALLBACK_LIBRARY_PATH-}"
'

"${RUNTIME_ENV[@]}" \
  "$INSTALL_PREFIX/bin/metavision_platform_info" --software
```

The software command exited successfully and reported OpenEB/Metavision `5.2.0`.

## 10. Loader-only plugin smoke

An ignored, local probe loaded the installed plugin directory without enumerating or opening hardware. It reported:

```text
hal_plugin_centuryarks  CenturyArks  1 camera discovery  0 file discoveries
hal_plugin_prophesee    Prophesee    2 camera discoveries 1 file discovery
```

This proves that both DSOs can be loaded and initialized in the isolated install and that the CenturyArks plugin does not register file discovery. It is not a live-camera or event-delivery test.

## 11. RAW and HDF5 regression

The tracked `algo/tests/sparklers.raw` was copied to the profile artifacts directory. The tracked source and copy both had SHA-256:

```text
e84afbecdc07d2910ae846a4ae0ee246f5b9c97a53816c637d4f85c023d7c234
```

The no-DYLD RAW result was:

```text
Integrator: Prophesee
Plugin: hal_plugin_prophesee
Encoding: EVT2
Duration: 95,871 us
CD events: 521,252
First timestamp: 0
Last timestamp: 95,871
```

This also confirms the intended division of responsibility: the CenturyArks plugin has no file discovery, so the existing Prophesee plugin continues to open the RAW file.

`metavision_file_to_hdf5` produced a 1,820,474-byte HDF5 file. OpenEB readback reported ECF encoding with the same duration, event count, first timestamp, and last timestamp. `h5dump` loaded the repository-local ECF plugin and reported `/CD/events` with a dataspace of 521,252 events.

Reusable commands are:

```bash
RAW_COPY="$ARTIFACT_DIR/input/sparklers.raw"
HDF5_OUTPUT="$ARTIFACT_DIR/openeb-5.2.0-centuryarks-validation.hdf5"

"${RUNTIME_ENV[@]}" \
  "$INSTALL_PREFIX/bin/metavision_file_info" \
  --input-event-file "$RAW_COPY"

"${RUNTIME_ENV[@]}" \
  "$INSTALL_PREFIX/bin/metavision_file_to_hdf5" \
  --input-path "$RAW_COPY" \
  --output-path "$HDF5_OUTPUT"

"${RUNTIME_ENV[@]}" \
  "$INSTALL_PREFIX/bin/metavision_file_info" \
  --input-event-file "$HDF5_OUTPUT"

env -u DYLD_LIBRARY_PATH -u DYLD_FALLBACK_LIBRARY_PATH \
  HOME="$RUNTIME_HOME" \
  TMPDIR="$RUNTIME_TMP" \
  PATH="$OPENEB_DEPENDENCY_PREFIX/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
  HDF5_PLUGIN_PATH="$HDF5_PLUGIN_PATH_VALUE" \
  h5dump -d /CD/events -s 0 -c 1 "$HDF5_OUTPUT"
```

Every output path must be checked before use; do not overwrite an earlier validation artifact.

## 12. Mach-O linkage, RPATH and contamination

Fourteen installed Mach-O objects were audited with `file`, `otool -D`, `otool -L`, and `otool -l`:

- the three required CLI executables;
- all installed `libmetavision*.dylib` objects in the validated profile;
- `libhdf5_ecf_codec`;
- the HDF5 ECF plugin;
- both HAL plugin dylibs;
- the shared PSEE hardware-layer dylib.

Validated relative RPATH behavior was:

| Installed object | RPATH |
| --- | --- |
| Three required CLI | `@executable_path/../lib` |
| SDK, HAL and HAL discovery dylibs | `@loader_path` |
| `libmetavision_psee_hw_layer.dylib` | `@loader_path/../../..` |
| `libhal_plugin_prophesee.dylib` | `@loader_path`, `@loader_path/../../..` |
| `libhal_plugin_centuryarks.dylib` | `@loader_path`, `@loader_path/../../..` |
| `libH5Zecf.dylib` | `@loader_path/../..` |

`libhdf5_ecf_codec` has no repository-local `@rpath` dependency that requires its own `LC_RPATH`.

The final install audit found no:

```text
/usr/local/lib/libmetavision*
/usr/local/lib/metavision/hal/plugins
build-tree runtime path
repository-absolute runtime dependency
OpenEB 5.1.1 library, package or plugin selection
```

Homebrew dependency libraries and macOS system libraries/frameworks remain allowed external dependencies. The stable OpenEB prefix was used only as a configure ignore boundary and was not modified.

## 13. Physical `31f7:0003` camera validation

Hardware validation began only after source integrity, configure, build, install, object ownership, registration, no-DYLD runtime, RAW/HDF5, RPATH, and contamination checks had passed.

The host USB layer confirmed:

```text
VID: 0x31f7
PID: 0x0003
Interface: 0xff / 0x19 / 0x00
```

The complete device serial remains only in ignored local logs. The stable anonymized identity used in tracked documentation is:

```text
serial SHA-256 prefix: cb823604ea92
```

The camera commands used the same no-DYLD repository-local environment plus `MV_LOG_LEVEL=TRACE`:

```bash
CAMERA_ENV=(
  "${RUNTIME_ENV[@]}"
  MV_LOG_LEVEL=TRACE
)

"${CAMERA_ENV[@]}" \
  "$INSTALL_PREFIX/bin/metavision_platform_info" --short

"${CAMERA_ENV[@]}" \
  "$INSTALL_PREFIX/bin/metavision_platform_info" --system

sleep 3

"${CAMERA_ENV[@]}" \
  "$INSTALL_PREFIX/bin/metavision_platform_info" --short
```

The first `--short` trace established:

- both plugin dylibs loaded;
- `hal_plugin_centuryarks` advertised one camera discovery and zero file discoveries;
- `hal_plugin_prophesee` advertised two camera discoveries and one file discovery;
- the CenturyArks Treuzell discovery matched `31f7:0003`;
- the standard Prophesee FX3 and Treuzell discoveries did not recognize the camera;
- the camera opened successfully through `hal_plugin_centuryarks`;
- the integrator was `CenturyArks`;
- the runtime identity had the form `CenturyArks:hal_plugin_centuryarks:<runtime serial>`;
- the command exited zero without timing out.

Observed device information was:

```text
Product descriptor: SilkyEvCam HD v03.09.00C
Sensor: IMX636
Connection: USB, 5000 Mb/s reported
Current encoding: EVT3
Available encodings: EVT3, EVT21
System release: 3.9.0
Compatible device: psee,ccam5_imx636
```

The product descriptor naturally contains `v03.09.00C`, while the system release was reported as `3.9.0`. Phase 1 did not import, implement, or synthesize the supplier's `-C` system-information override. These values are runtime hardware observations, not source gates or PID-to-model logic.

`--system` opened the same camera, reported the CenturyArks/IMX636 system and default bias information, exited zero, and did not time out. No explicit command to change biases, ROI, trigger, anti-flicker, ERC, encoding, firmware, EEPROM, or pixel-mask state was issued. Phase 1 adds no facility override; normal upstream device-open initialization remains in effect.

After both commands exited normally, the test waited three seconds and ran `--short` once more. The same CenturyArks plugin matched and opened the camera, the standard plugin again did not claim it, and the command exited zero. This is a single-process reopen smoke, not a physical unplug/replug test, stress test, simultaneous multi-camera test, or full reconnect-lifecycle result.

PID status after this validation is:

```text
31f7:0002  registered from supplier source; hardware not tested
31f7:0003  enumeration and open verified on one physical camera
31f7:0004  registered from supplier source; hardware not tested
```

## 14. Live-stream candidate audit

No installed executable met all required conditions for an authorized Phase 1 stream test: headless operation, an explicit short duration/event bound, no interactive GUI, no recording ambiguity, and no device-parameter or hardware-register writes.

The review found:

- camera stream slicers and the viewer are interactive GUI programs without a total runtime bound;
- the synced slicer and viewer expose recording paths;
- `metavision_riscv_logger` is unbounded and performs hardware-register writes for a different sensor family;
- active-pixel detection is interactive and may change ROI/pixel-mask state;
- remaining installed file tools are offline only.

Therefore:

```text
Safe bounded live-stream candidate: not found
Live event delivery: not run
RAW recording: not run
```

Event delivery requires a separately reviewed bounded tool and separate authorization.

## 15. Linux and standard-device impact

The canonical OpenEB source remains unchanged, so its preserved Linux baseline is unaffected by merely retaining these overlay inputs. The prepared overlay intentionally appends an always-built CenturyArks plugin on every platform where that prepared source is configured; it inherits the existing non-Apple plugin-loop RPATH and install behavior.

No Linux configure, build, install, loader, USB, V4L2, udev, or runtime test was performed. It is therefore not valid to claim that the prepared overlay has passed Linux regression. Static review established that the two-hunk patch does not remove or edit the existing Prophesee target, registration source, V4L2 condition, install rule, hardware-layer target, or non-Apple RPATH branch.

The CenturyArks VID `0x31f7` does not overlap the registrations observed in the standard plugin. Device-specific matching remains confined to the CenturyArks plugin's three VID/PID registrations.

## 16. Logs and artifacts

Complete generated evidence is ignored under:

```text
$REPO_ROOT/.logs/openeb-5.2.0-centuryarks-macos/
$REPO_ROOT/.artifacts/openeb-5.2.0-centuryarks-macos/
```

Important records include:

```text
prepare.log
prepare-retry-1.log
preparation-manifest.txt
preparation-manifest-before-final-script.txt
prepared-source-before-final-script.tsv
prepared-source-after-final-script.tsv
final-tracked-preparation-inputs.sha256
prepare-final-script.log
prepare-existing-output-guard.log
build-final-prepared-source-plugins.log
configure.log
build-bootstrap.log
build-full.log
install.log
install-file-list.txt
registration-ownership-audit.log
prophesee-link-comparison.diff
plugin-loader-probe.log
raw-file-info.log
file-to-hdf5.log
hdf5-file-info.log
h5dump.log
install-mach-o-list.txt
install-linkage.log
linkage-summary.txt
live-stream-candidate-audit.log
camera-validation-redacted.txt
camera-platform-short-host-first-redacted.log
camera-platform-system-host-redacted.log
camera-platform-short-host-reopen-redacted.log
```

Only the redacted camera logs are suitable for review without exposing the complete serial. The serial hash identifies one tested instance and must not become a build or runtime condition.

## 17. Disk and workspace result

The validation retained all earlier OpenEB comparison trees and added one new isolated profile. Final measured sizes were:

```text
Prepared source: 60,552 KiB
Build tree: 95,068 KiB
Install prefix: 15,480 KiB
Logs: 2,472 KiB
Artifacts: 3,936 KiB
Runtime HOME/TMP: 8 KiB
```

The repository `du` measurement increased from 411,820 KiB at preflight to 588,844 KiB after reproducibility closure: 177,024 KiB, or approximately 172.9 MiB. This remained below both the 0.30 GiB conservative estimate and the 1 GiB authorization threshold.

Filesystem-available space at that measurement was 46,852,636 KiB, approximately 44.68 GiB. The remaining margin above the 35,904,375 KiB protection line was 10,948,261 KiB, approximately 10.44 GiB.

No project-controlled file was written to `/usr/local`, `/opt/homebrew`, the external OpenEB 5.1.1 tree, or the external supplier source directory. Two transient HDF5 comparison lists were created under `/tmp` during an earlier read-only review and immediately deleted; no repository-external project artifact survives. This statement does not claim that macOS or toolchain components produced no system-managed cache activity.

## 18. Conclusion and remaining work

Phase 1 demonstrates that the audited CenturyArks entrypoint can be adapted to OpenEB 5.2 as an always-built side-by-side plugin without replacing the standard Prophesee plugin or duplicating the hardware layer. The repository-local OpenEB 5.2 installation loaded both plugins without DYLD overrides, preserved RAW/HDF5 behavior, and used `hal_plugin_centuryarks` to enumerate, open, report, close, and reopen one physical `31f7:0003` IMX636 camera.

The final tracked preparation script reproduced the validated prepared source byte-for-byte, passed its existing-output guard, and supported successful incremental builds of both plugin targets. This closes the tracked-input/prepared-source version gap without changing the functional scope.

Still unvalidated or deliberately excluded:

- live event delivery and bounded recording;
- EEPROM reads and pixel-mask behavior;
- CenturyArks-specific facility or system-information changes;
- bias, ROI, trigger, anti-flicker, ERC, or other parameter modification;
- firmware update, recovery, or reset;
- PIDs `0002` and `0004` on physical hardware;
- simultaneous multi-camera selection and streaming;
- physical disconnect/reconnect and extended shutdown stress;
- Linux build/runtime regression;
- EBplus GUI use of the new OpenEB prefix.
