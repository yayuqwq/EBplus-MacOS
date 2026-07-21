# Milestone 3 macOS CMake Configuration Validation

## 1. Scope

Milestone 3 establishes a repeatable EBplus CMake configuration for macOS
Apple Silicon. Validation used branch `build/macos-cmake-configuration` with
base HEAD `ec0d11a166feb7a29efb87f8c28fbbdd4ab15604`.

The validated profile was macOS arm64, CMake 4.1.2, AppleClang 15, Unix
Makefiles, and Release. This milestone covers configure, build, test discovery,
CTest, repository-local install, and static Mach-O/RPATH inspection. It does
not cover application runtime behavior.

## 2. Final implementation

- Reject in-source builds and require an explicit macOS install prefix.
- Discover OpenEB 5.2, Qt, OpenCV, GTest, and optional ONNX Runtime through
  explicit CMake contracts.
- Preserve platform-specific install RPATHs: `@executable_path/../lib` on
  Apple platforms and `$ORIGIN/../lib` on Linux.
- Make testing conditional on `BUILD_TESTING`, retain GUI test control, and
  configure a 30-second GTest discovery timeout.
- Repair AppleClang-exposed strict C++17 portability issues without changing
  the public algorithm or GUI behavior.
- Keep GUI test JSON artifacts deterministic, case-specific, and inside the
  active build tree.

## 3. Dependency provenance

- OpenEB / Metavision SDK: 5.2.0.
- Repository-local prefix: `.deps/openeb-5.2.0-centuryarks-macos`.
- MetavisionSDK, MetavisionHAL, and HDF5 ECF package provenance passed.
- HDF5 ECF commit: `b982d908a0bc0afd9104d226607bedb1a11b2a95`.
- `/usr/local` OpenEB 5.1.1 header and library contamination: none.
- Qt: 6.9.3.
- OpenCV: 4.12.0.
- GTest: 1.17.0.
- ONNX Runtime was not found; the heuristic fallback was built.

## 4. macOS configure and build evidence

- Fresh Release/arm64 configure passed with Unix Makefiles.
- Fresh complete build passed with AppleClang 15.
- `gui_algo`, `gui_core`, and `gui_for_openeb` were built.
- 16 of 16 test and diagnostic binaries were built.
- All produced executables were non-fat arm64 Mach-O binaries.

The build retained the known Qt deprecated-API warnings and the existing
`test_raw_algos` unused-variable warning. It produced no portability, linker,
architecture, or GTest discovery timeout failure.

## 5. Testing evidence

- 9 of 9 GTest discovery targets passed.
- 295 tests were discovered.
- 295 of 295 CTest tests passed with parallelism 2.
- GUI test artifacts were isolated under the build tree.
- No test artifact files remained after the complete CTest run.
- The production executable and standalone diagnostic tools were not invoked
  by CTest.

## 6. Header isolation repair

The macOS configuration prioritizes explicitly selected imported-package
headers ahead of compiler-provided system include paths.

- `/usr/local/include/metavision` dependency files: 0.
- Repository-local OpenEB 5.2 dependency files: 87.

Static dependency and Mach-O audits found no OpenEB 5.1.1 contamination.

## 7. Test portability and isolation repairs

The GTest discovery timeout is configurable and was generated as 30 seconds
for all nine discovery targets. GUI ConfigManager and LayoutManager tests use
deterministic per-test JSON paths derived from the current suite and test name.
Their artifact root is the active build tree, stale files are removed per case,
and teardown removes only the current case file. Parallel CTest execution is
preserved without `RUN_SERIAL`, resource locks, sleeps, or retries.

## 8. Install and RPATH evidence

- Repository-local install passed.
- The install manifest contains exactly `bin/gui_for_openeb`.
- The installed executable is non-fat arm64.
- Installed `LC_RPATH` is exactly `@executable_path/../lib`.
- Build-tree absolute RPATH entries were removed from the installed copy.
- Dependency load commands otherwise remained unchanged.

Standalone loader closure is intentionally incomplete because OpenEB, Qt, and
OpenCV dependencies were not staged into the install prefix. Dependency
staging and a portable bundle remain Milestone 8 work.

## 9. Clean reproducibility

The canonical `.build/ebplus-macos` and `.deps/ebplus-macos` trees were
precisely removed while preserving the source tree, dependency prefixes, and
historical logs. Fresh configure, complete build, 295-test CTest, install,
header provenance, architecture, manifest, and installed RPATH audits all
passed from the newly created canonical trees.

The source diff SHA remained unchanged:
`c7191442a0f0ace0197612be6000905e5fa23ed89f60c749e73026f0d89484e7`.

## 10. Linux validation decision

Native Linux configure, build, CTest and install were not performed.

The maintainer decided to close Milestone 3 without native Linux runtime
regression. This is an accepted validation gap, not evidence that Linux
behavior was verified.

The implementation preserves Linux-specific branches and defaults by static
inspection, including `$ORIGIN` install RPATH, Linux ONNX search roots, default
test behavior and existing launcher behavior. These claims remain source-level
observations only.

## 11. Explicitly unvalidated areas

- `gui_for_openeb` was not executed.
- Qt Cocoa application launch was not tested.
- RAW/HDF5 GUI workflow was not tested.
- Camera application path was not tested.
- Facility and reconnect behavior were not tested.
- ONNX loading and inference were not tested.
- Dependency staging and a portable bundle were not tested.
- Linux runtime regression was not tested.

## 12. Final status

Complete — macOS arm64 validated; native Linux regression not run, risk
accepted by maintainer.
