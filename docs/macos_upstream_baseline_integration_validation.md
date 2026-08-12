# macOS frozen upstream baseline integration validation

## Status and scope

**Status:** qualified integration candidate; Git closure is deliberately
separate from this evidence record.

This report records the macOS evidence obtained for an active, uncommitted
no-fast-forward merge on `integration/upstream-f72fdf7`. It is a validation
layer for the merged working source state, not a rewrite of the historical M3,
M4, or M5 reports and not a claim that the merge has been committed.

The integration is intentionally pinned to a **frozen upstream integration
baseline**, not to whatever `upstream/main` may contain later. A later upstream
advance is a separate synchronization task.

## Identity

| Item | Value |
| --- | --- |
| Fork baseline / merge first parent | `d01f1c1a632dece8be10618d2212d6c3f76aeb23` |
| Frozen upstream integration baseline / merge second parent | `f72fdf750ab82c09eb1d11ba828a4ac0601a2ea9` |
| Merge base | `e0439b79f4b272f249cb096f8daf7f73824ca788` |
| Fork-only / upstream-only commits at integration planning | `50 / 32` |
| Integration branch | `integration/upstream-f72fdf7` |
| Integration state during qualification | active no-ff merge, no unmerged index entries |

The candidate source reconciliation remained in the merge index while this
documentation layer was drafted separately. The frozen baseline must not be
described as the current live upstream branch.

## Source reconciliation summary

The integration resolved three textual conflicts in:

- `algo/analytics/particle_counter.h`;
- `algo/tests/CMakeLists.txt`; and
- `gui/main_window.cpp`.

The `particle_counter` reconciliation retained strict C++17 syntax and the
upstream stale-track behavior: removing a stale `Particle` also erases its
`prev_cy_` entry. It did not select either whole file mechanically.

Other material reconciliations included:

- CMake/test integration and current GTest discovery;
- suppression of duplicate file-open error dialogs and transactional file
  lifecycle cleanup;
- generic-offline HDF5/H5/DAT facility degradation without assuming a HAL
  `Device`;
- paused seek presentation and source/geometry reset behavior;
- a bounded very-short-file direct-connected-start regression covering EOF and
  loop ordering;
- HDF5 source metadata preservation: `ExporterController` calls
  `writer.add_metadata_map_from_camera(*cam)` before event writes;
- `/tmp` workspace-policy reconciliation for project-controlled outputs; and
- current algorithm/UI baseline changes, including the seven-transform
  `FilterChain`, unified ROI, and a 33-entry registry.

The very-short regression uses an official `RAWEvt2EventFileWriter` to create
a repository/build-local 8x8 EVT2 fixture with two CD events at 0 and 1,000 us.
It is synthetic, non-zero-duration data, not a sensor recording. The two
focused cases are:

- `CameraControllerLifecycle.VeryShortRawDirectConnectedStartLoopOffStopsOnce`;
- `CameraControllerLifecycle.VeryShortRawDirectConnectedStartLoopOnWrapsWithoutTerminalEof`.

They exercise `connected -> start #1`, source `STOPPED` before the synchronous
file-connect handler returns, then loop-off terminal EOF or loop-on mirror/wrap
behavior. The current repository-local OpenEB 5.2 runtime made the first
direct start succeed and report the short source stopped before return; the
tests bound subsequent observation. They do not cover a zero-duration file or
general playback semantics.

## Build qualification

The qualified EBplus build used the existing repository-local CenturyArks
OpenEB 5.2 producer profile:

```text
prepared source: .tmp/openeb-5.2.0-centuryarks-source
OpenEB build:    .build/openeb-5.2.0-centuryarks-macos
OpenEB install:  .deps/openeb-5.2.0-centuryarks-macos
EBplus build:    .build/ebplus-macos
```

The corresponding ECF checkout was
`b982d908a0bc0afd9104d226607bedb1a11b2a95` and clean. The current EBplus
cache selected the same repository-local `MetavisionSDK`, `MetavisionHAL`, and
`hdf5_ecf` package paths; no `/usr/local` OpenEB 5.1.1 header or library
contamination was found in the recorded provenance checks.

| Qualification | Result |
| --- | --- |
| Configure, Release / arm64 | Passed |
| Normal complete build | Passed |
| Focused integration CTest selection | 42 / 42 passed |
| Full CTest | 341 / 341 passed (82.95 s) |
| GUI executable | `.build/ebplus-macos/gui/gui_for_openeb` |
| Mach-O architecture | arm64 |
| GUI UUID | `A465D48A-4D22-3073-9043-965B7758DD94` |
| Static OpenEB/linkage provenance | repository-local 5.2 CenturyArks; no `/usr/local` 5.1.1 evidence |

The recorded non-blocking warnings were the already-known Qt deprecated-API
and test-only unused-variable warnings; no warning indicated wrong OpenEB,
architecture, missing source, or linkage provenance.

Configure/build/CTest/Mach-O evidence is not Cocoa GUI evidence, standalone
loader closure, Linux evidence, or a physical-camera result.

## Post-integration macOS runtime requalification

The retained representative GUI session ran the qualified build-tree binary
through Cocoa with repository-local runtime roots. Wrapper PID `20073` and GUI
PID `20079` exited `0`; the scoped fatal-marker scan was clean.

### File-source and display lifecycle

The tracked RAW A fixture, `algo/tests/sparklers.raw`, was read as 640x480 EVT2
with 521,252 CD events spanning 0 through 95,871 us. The session observed:

- open, autoplay/dynamic display, pause/resume, natural EOF, same-file reopen
  and a Recent entry;
- forward and backward paused seek with immediate display update, without
  requiring Resume;
- no duplicate error dialog, unexpected load-time loop wrap, stale frame, or
  unresponsive GUI.

The current unified ROI UI was exercised on the file source using X/Y/W/H
settings and enable/disable/re-enable. This is representative software-crop
file-source evidence only; it is not hardware ROI/RONI evidence or numerical
ROI correctness.

Time Surface was enabled with the observed default selections Linear decay,
Hot palette, merged channels and 30 Hz refresh. Output was visibly non-empty
and dynamic through pause/resume and bidirectional paused seek, with no white
frame observed in this session. The exact initial decay-time value was not
confirmed after an accidental control-wheel change, and this qualitative
session is not parameter-complete or numerical validation.

### Geometry and generic-offline sources

The session exercised:

```text
RAW A (640x480)
-> file-source Disconnect / clear
-> reopen A
-> synthetic RAW B (320x240)
-> reopen A (640x480)
```

Geometry, displayed frame, duration/position, pause/resume and paused seek
updated across each transition without stale geometry/frame, crash, hang, or
dialog. B is a repository-local synthetic OpenEB-generated fixture, not a
second real sensor recording.

Previously retained valid HDF5, H5 and non-zero-CD DAT fixtures each completed
open, dynamic playback, pause/resume, bidirectional paused seek, EOF/stop and
clean source recovery. No `DeviceUnavailable` or error 102113 dialog was
observed. This is bounded generic-offline evidence, not a format corpus or
physical HAL-device result.

### Fresh HDF5 export round trip

A fresh GUI export from RAW A to HDF5 reached success, reopened, autoplayed,
paused/resumed, sought in both directions and reached EOF normally. The fresh
retained artifact is:

```text
.artifacts/m5-post-upstream-20260811T161247Z/sparklers-m5-post-upstream.h5
```

It is a regular HDF5 file (1,820,474 bytes). The readback comparison preserved
the 640x480 geometry, 521,252 CD events, timestamps 0 through 95,871 us and
duration. RAW EVT2 to HDF5 ECF is the expected format conversion, not an
invariant failure. This proves scoped source-event export/reopen/readback; it
does not prove general algorithm-result export.

The export dialog was functionally successful but visually cramped. This is a
non-blocking user-observed layout issue, not a claim of dialog visual quality.

## Evidence boundaries and limitations

Historical M3, M4 and M5 records remain historical provenance for their own
source states. The evidence above is a separate post-integration layer for the
frozen candidate and must not be retroactively substituted into their counts,
UUIDs or conclusions.

The following remain unverified:

- **Linux integration configure/build/runtime regression: Not run / unverified.**
  The maintainer accepted this residual risk for this frozen integration
  closure only. **Risk acceptance is not Linux validation evidence.** It does
  not extend automatically to M6, M7, or M8.
- EBplus GUI physical-camera lifecycle, hardware facilities, physical loss or
  automatic reconnect;
- ONNX/model inference, broader algorithms, and numerical correctness;
- AVI and processed recording; calibration runtime; standalone bundle/loader;
- stress/long-duration behavior, zero-duration files, and a second real
  sensor recording.

Representative file-source runtime is not all-algorithm evidence; synthetic
RAW B is not a real sensor recording; CTest is not GUI evidence; and macOS
evidence is not Linux evidence.

## Documentation and Git closure boundary

This report is intended to accompany the frozen source integration as a
separate documentation/evidence commit after the source merge is committed.
The recommended order is:

1. under separate authorization, complete the active source merge as the
   ancestry-preserving two-parent merge commit with the frozen upstream parent;
2. create a normal single-parent documentation/evidence commit containing this
   report and the reconciliation updates; then
3. under separately granted push/PR/merge authority, use a normal two-parent
   PR merge rather than squash or rebase.

No action in this report itself performs Git closure.
