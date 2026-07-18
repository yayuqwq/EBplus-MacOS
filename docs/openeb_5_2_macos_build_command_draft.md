# OpenEB 5.2.0 macOS build command draft

> **Status: Not ready to execute**
>
> The repository-local `hdf5_ecf` gitlink is empty and the EBplus root repository does not yet have a reproducible mapping for its pinned source. Resolve that prerequisite and obtain explicit authorization for the selected dependency-recovery operation and any network/download it entails before running any configure command below.

These are candidate Milestone 2B commands only. They were not executed during Milestone 2A.

The primary profile below preserves the current Milestone 2 requirement for built-in OpenEB CLI validation. Because OpenEB has no fine-grained CLI switch, this requires `BUILD_SAMPLES=ON` and the `ui` module in addition to `base`, `core`, and `stream`.

## Preflight

Run from the EBplus repository on the approved Milestone 2B branch:

```bash
REPO_ROOT="$(git rev-parse --show-toplevel)"

OPENEB_SOURCE="$REPO_ROOT/openeb"
OPENEB_BUILD="$REPO_ROOT/.build/openeb-5.2.0-macos"
OPENEB_PREFIX="$REPO_ROOT/.deps/openeb-5.2.0-macos"
OPENEB_TMP="$REPO_ROOT/.tmp/openeb-5.2.0-macos"
OPENEB_HOME="$OPENEB_TMP/home"
OPENEB_ARTIFACTS="$REPO_ROOT/.artifacts/openeb-5.2.0-macos"

git status --short --branch
git branch --show-current
git rev-parse HEAD

df -h "$REPO_ROOT"
df -k "$REPO_ROOT"
du -sh "$REPO_ROOT"
du -sk "$REPO_ROOT"

find "$REPO_ROOT" -maxdepth 1 \
  \( -name '.build' \
  -o -name '.deps' \
  -o -name '.venv' \
  -o -name '.cache' \
  -o -name '.tmp' \
  -o -name '.logs' \
  -o -name '.downloads' \
  -o -name '.artifacts' \) \
  -print
```

Before creating any directory, report current sizes, expected growth, duplicate-tree risk and the remaining-space protection calculation. The evidence-based conservative Milestone 2A estimate is 0.75 GiB and does not itself reach the 1 GiB authorization threshold. If the refreshed expected or upper estimate reaches 1 GiB, obtain separate operation-specific authorization before continuing.

## Source completeness prerequisite

The dependency recovery must be handled as a separate reviewed and approved step. Do not allow configure to perform an implicit download.

After the approved recovery, verify all of the following before continuing:

```bash
HDF5_ECF_SOURCE="$OPENEB_SOURCE/sdk/modules/stream/cpp/3rdparty/hdf5_ecf"
HDF5_ECF_COMMIT="b982d908a0bc0afd9104d226607bedb1a11b2a95"

git ls-files --stage \
  openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf

if [ ! -f "$HDF5_ECF_SOURCE/CMakeLists.txt" ]; then
  echo "hdf5_ecf source is incomplete" >&2
  exit 1
fi

if [ ! -e "$HDF5_ECF_SOURCE/.git" ]; then
  echo "No independent hdf5_ecf Git metadata; use the separately reviewed vendored tree/checksum verification instead" >&2
  exit 1
fi

HDF5_ECF_TOPLEVEL="$(git -C "$HDF5_ECF_SOURCE" rev-parse --show-toplevel)" || exit 1
if [ "$HDF5_ECF_TOPLEVEL" != "$HDF5_ECF_SOURCE" ]; then
  echo "hdf5_ecf Git lookup resolved outside the dependency directory" >&2
  exit 1
fi

RECOVERED_HDF5_ECF_COMMIT="$(git -C "$HDF5_ECF_SOURCE" rev-parse HEAD)" || exit 1
if [ "$RECOVERED_HDF5_ECF_COMMIT" != "$HDF5_ECF_COMMIT" ]; then
  echo "hdf5_ecf commit does not match the root gitlink" >&2
  exit 1
fi

git submodule status \
  openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf || exit 1
```

The checks above apply to the preferred restored-submodule design. If the approved design vendors ordinary tracked source instead, replace them with a separately reviewed tree/checksum verification tied to the same pinned source; do not let `git -C` resolve upward to the EBplus repository. Document and review any different dependency-management design before configure.

## Dependency inputs

Do not hardcode a package-manager, stable OpenEB or personal prefix. Supply the dependency prefix and the stable 5.1.1 prefix explicitly for the current machine. They must be different paths:

```bash
: "${OPENEB_DEPENDENCY_PREFIX:?Set OPENEB_DEPENDENCY_PREFIX explicitly}"
: "${STABLE_OPENEB_PREFIX:?Set STABLE_OPENEB_PREFIX explicitly}"

MAKE_PROGRAM="${MAKE_PROGRAM:-$(command -v gmake)}"
if [ -z "$MAKE_PROGRAM" ]; then
  echo "GNU Make was not found" >&2
  exit 1
fi

if [ ! -d "$OPENEB_DEPENDENCY_PREFIX" ]; then
  echo "OPENEB_DEPENDENCY_PREFIX does not exist" >&2
  exit 1
fi

if [ ! -d "$STABLE_OPENEB_PREFIX" ]; then
  echo "STABLE_OPENEB_PREFIX does not exist" >&2
  exit 1
fi

OPENEB_DEPENDENCY_PREFIX="$(cd "$OPENEB_DEPENDENCY_PREFIX" && pwd -P)" || exit 1
STABLE_OPENEB_PREFIX="$(cd "$STABLE_OPENEB_PREFIX" && pwd -P)" || exit 1

if [ "$OPENEB_DEPENDENCY_PREFIX" = "$STABLE_OPENEB_PREFIX" ]; then
  echo "Dependency and stable OpenEB prefixes must be different" >&2
  exit 1
fi

OPENEB_PKG_CONFIG_LIBDIR="$OPENEB_DEPENDENCY_PREFIX/lib/pkgconfig:$OPENEB_DEPENDENCY_PREFIX/share/pkgconfig"

DEPENDENCY_ENV=(
  env
  PKG_CONFIG_PATH=
  PKG_CONFIG_LIBDIR="$OPENEB_PKG_CONFIG_LIBDIR"
)

cmake --version
"$MAKE_PROGRAM" --version
clang --version
pkg-config --version

"${DEPENDENCY_ENV[@]}" pkg-config --modversion libusb-1.0
"${DEPENDENCY_ENV[@]}" pkg-config --modversion opencv4
"${DEPENDENCY_ENV[@]}" pkg-config --modversion protobuf
"${DEPENDENCY_ENV[@]}" pkg-config --modversion hdf5
```

`PKG_CONFIG_PATH` is deliberately cleared and `PKG_CONFIG_LIBDIR` is derived only from the explicit dependency prefix. Run configure through the same process-local environment so CMake's pkg-config probes cannot silently reuse the stable 5.1.1 prefix.

`Unix Makefiles` is the compatibility-first candidate because the stable 5.1.1 environment used it on this host. Ninja remains an unvalidated alternative.

## Workspace creation

Only after source, disk and authorization preflight succeeds:

```bash
mkdir -p "$OPENEB_TMP/tmp"
mkdir -p "$OPENEB_HOME"
mkdir -p "$OPENEB_ARTIFACTS"
```

CMake will create the single approved build tree. Installation must remain inside `OPENEB_PREFIX`.

## Configure

> **Not ready to execute until `hdf5_ecf` is complete.**

Primary M2B CLI-validation profile:

```bash
"${DEPENDENCY_ENV[@]}" cmake -S "$OPENEB_SOURCE" \
  -B "$OPENEB_BUILD" \
  -G "Unix Makefiles" \
  -DCMAKE_MAKE_PROGRAM="$MAKE_PROGRAM" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_INSTALL_PREFIX="$OPENEB_PREFIX" \
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
  -DUSE_OPENGL_ES3=OFF
```

Do not add `HDF5_DISABLED=ON` to this primary profile. HDF5 is part of the intended EBplus playback/export range, and its locked source must be repaired before configure.

Reduced diagnostic profile, only if the user explicitly accepts that built-in OpenEB CLI validation and HDF5 functionality are not part of that run:

```text
BUILD_SAMPLES=OFF
METAVISION_SELECTED_MODULES=base;core;stream
HDF5_DISABLED=ON
```

That reduced profile is not sufficient to mark Milestone 2 complete.

## Review configure output

Stop before building if any OpenEB/Metavision package or install output resolves to the stable system OpenEB prefix.

```bash
CACHE="$OPENEB_BUILD/CMakeCache.txt"

grep -E \
  'CMAKE_BUILD_TYPE|CMAKE_GENERATOR|CMAKE_MAKE_PROGRAM|CMAKE_OSX_ARCHITECTURES|CMAKE_INSTALL_PREFIX|CMAKE_PREFIX_PATH|CMAKE_IGNORE_PREFIX_PATH|BUILD_TESTING|BUILD_SAMPLES|COMPILE_PYTHON3_BINDINGS|GENERATE_DOC|CODE_COVERAGE|LFS_DOWNLOAD|METAVISION_SELECTED_MODULES|USE_PROTOBUF|USE_OPENGL_ES3|HDF5|LIBUSB|OpenCV|Boost|Protobuf|HAS_V4L2' \
  "$CACHE"

grep -i -E 'Metavision|OpenEB|HDF5|libusb|OpenCV|Boost|Protobuf' \
  "$CACHE"

test "$(grep '^CMAKE_BUILD_TYPE:STRING=' "$CACHE")" = \
  'CMAKE_BUILD_TYPE:STRING=Release' || exit 1

test "$(grep '^CMAKE_OSX_ARCHITECTURES:STRING=' "$CACHE")" = \
  'CMAKE_OSX_ARCHITECTURES:STRING=arm64' || exit 1

test "$(grep '^CMAKE_INSTALL_PREFIX:PATH=' "$CACHE")" = \
  "CMAKE_INSTALL_PREFIX:PATH=$OPENEB_PREFIX" || exit 1
```

Review the configure log for:

```text
HDF5 ECF source and target discovery
METAVISION_SDK_MODULES_AVAILABLE
HAS_V4L2=FALSE on macOS
AppleClang and arm64
OpenGL, GLEW and GLFW from the explicit dependency environment
No Python, tests, documentation or LFS download scheduling
No dependency on the stable OpenEB installation
```

## Build

First build the minimum targets needed to expose SDK, plugin, RAW and live-camera issues:

```bash
BUILD_JOBS="$(sysctl -n hw.logicalcpu)"

"${DEPENDENCY_ENV[@]}" cmake --build "$OPENEB_BUILD" \
  --parallel "$BUILD_JOBS" \
  --target \
    hal_plugins \
    metavision_platform_info \
    metavision_file_info \
    metavision_file_to_hdf5
```

Review output and disk growth before the wider build required for a reliable default install:

```bash
du -sh "$OPENEB_BUILD"
du -sk "$OPENEB_BUILD"
df -h "$REPO_ROOT"
df -k "$REPO_ROOT"
```

Then build all targets enabled by the reviewed profile:

```bash
"${DEPENDENCY_ENV[@]}" cmake --build "$OPENEB_BUILD" \
  --parallel "$BUILD_JOBS"
```

## Install

Confirm the cached prefix again immediately before installation:

```bash
test "$(grep '^CMAKE_INSTALL_PREFIX:PATH=' "$OPENEB_BUILD/CMakeCache.txt")" = \
  "CMAKE_INSTALL_PREFIX:PATH=$OPENEB_PREFIX" || exit 1
```

Install without `sudo`:

```bash
cmake --install "$OPENEB_BUILD"
```

Inspect the repository-local installation:

```bash
find "$OPENEB_PREFIX" -maxdepth 4 -type f -print | sort
du -sh "$OPENEB_PREFIX"
du -sk "$OPENEB_PREFIX"
```

## Environment-isolated validation

Use a process-local repository HOME and temporary directory so OpenEB resource code cannot create project-controlled state in the normal macOS user profile:

```bash
OPENEB_PLUGIN_PATH="$OPENEB_PREFIX/lib/metavision/hal/plugins"
OPENEB_HDF5_PLUGIN_PATH="$OPENEB_PREFIX/lib/hdf5/plugin"

if [ ! -f "$OPENEB_HDF5_PLUGIN_PATH/libH5Zecf.dylib" ]; then
  echo "HDF5 ECF plugin was not installed" >&2
  exit 1
fi

ISOLATED_ENV=(
  env
  HOME="$OPENEB_HOME"
  TMPDIR="$OPENEB_TMP/tmp"
  PATH="$OPENEB_PREFIX/bin:/usr/bin:/bin:/usr/sbin:/sbin"
  DYLD_LIBRARY_PATH="$OPENEB_PREFIX/lib"
  MV_HAL_PLUGIN_PATH="$OPENEB_PLUGIN_PATH"
  MV_HAL_PLUGIN_SEARCH_MODE=PLUGIN_PATH_ONLY
  HDF5_PLUGIN_PATH="$OPENEB_HDF5_PLUGIN_PATH"
)
```

Version and software identity:

```bash
"${ISOLATED_ENV[@]}" \
  "$OPENEB_PREFIX/bin/metavision_platform_info" --software
```

RAW validation requires a user-supplied known-good sample; do not download one implicitly:

```bash
: "${RAW_SAMPLE:?Set RAW_SAMPLE to an approved existing RAW file}"

"${ISOLATED_ENV[@]}" \
  "$OPENEB_PREFIX/bin/metavision_file_info" \
  --input-event-file "$RAW_SAMPLE"
```

HDF5 write and readback validation must keep its generated file inside the repository:

```bash
HDF5_OUTPUT="$OPENEB_ARTIFACTS/openeb-5.2.0-validation.hdf5"
if [ -e "$HDF5_OUTPUT" ]; then
  echo "Refusing to overwrite existing HDF5 validation artifact" >&2
  exit 1
fi

"${ISOLATED_ENV[@]}" \
  "$OPENEB_PREFIX/bin/metavision_file_to_hdf5" \
  --input-path "$RAW_SAMPLE" \
  --output-path "$HDF5_OUTPUT"

"${ISOLATED_ENV[@]}" \
  "$OPENEB_PREFIX/bin/metavision_file_info" \
  --input-event-file "$HDF5_OUTPUT"
```

Live-camera validation, only when the target camera and external-write policy have been reviewed:

```bash
"${ISOLATED_ENV[@]}" \
  "$OPENEB_PREFIX/bin/metavision_platform_info" --short

"${ISOLATED_ENV[@]}" \
  "$OPENEB_PREFIX/bin/metavision_platform_info" --system
```

These two commands cover device enumeration, open and reported device/system information only. They do **not** validate a live event stream, facility access, parameter changes, clean shutdown or reconnect. Select and review a separate hardware command or test for those behaviors before executing it or claiming live-camera validation. CenturyArks devices with VID `31f7` require the separately reviewed 5.2 vendor-port decision before a successful result can be expected.

## Verify stable 5.1.1 remains default

Run outside the isolated command environment:

```bash
STABLE_PLATFORM_INFO="$(command -v metavision_platform_info)" || exit 1
if [ "$STABLE_PLATFORM_INFO" != "$STABLE_OPENEB_PREFIX/bin/metavision_platform_info" ]; then
  echo "Ordinary PATH does not resolve to the declared stable OpenEB prefix" >&2
  exit 1
fi

metavision_platform_info --software
```

The ordinary terminal must continue to resolve the stable 5.1.1 environment. Do not add the repository prefix to global shell startup files.

## Linkage inspection

```bash
otool -L "$OPENEB_PREFIX/bin/metavision_platform_info"
otool -L "$OPENEB_PREFIX/bin/metavision_file_info"
otool -L "$OPENEB_PREFIX/bin/metavision_file_to_hdf5"
otool -L "$OPENEB_HDF5_PLUGIN_PATH/libH5Zecf.dylib"

find "$OPENEB_PREFIX/lib" \
  -maxdepth 1 \
  -name 'libmetavision*.dylib' \
  -type f \
  -exec otool -L {} \;

find "$OPENEB_PLUGIN_PATH" \
  -maxdepth 1 \
  -name '*.dylib' \
  -type f \
  -exec otool -L {} \;
```

Verify all Metavision libraries and plugins resolve to the repository-local 5.2 prefix or controlled `@rpath`/`@loader_path`, never to the stable 5.1.1 installation.

## Disk report

```bash
df -h "$REPO_ROOT"
df -k "$REPO_ROOT"
du -sh "$REPO_ROOT"
du -sk "$REPO_ROOT"

du -sh \
  "$OPENEB_BUILD" \
  "$OPENEB_PREFIX" \
  "$OPENEB_TMP" \
  "$OPENEB_ARTIFACTS" \
  2>/dev/null || true

du -sk \
  "$OPENEB_BUILD" \
  "$OPENEB_PREFIX" \
  "$OPENEB_TMP" \
  "$OPENEB_ARTIFACTS" \
  2>/dev/null || true
```

Report actual growth against the Milestone 2A baseline and state whether any tool or system behavior may have created external caches. Do not delete any existing output without separate path-specific authorization.
