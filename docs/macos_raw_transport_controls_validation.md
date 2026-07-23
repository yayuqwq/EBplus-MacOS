# macOS RAW Transport Controls Validation

## 1. Status

**Status:** Passed — narrow macOS RAW transport-controls validation for one tracked <code>.raw</code> sample.

The broader RAW playback parity milestone remains <code>Planned</code> and is not closed by this validation.

## 2. Scope

This record is limited to:

- macOS Apple Silicon / arm64;
- <code>main</code> at <code>240ebf2154ae5ecab2dcb20a91c323deda786555</code>;
- the build-tree executable <code>.build/ebplus-macos/gui/gui_for_openeb</code>;
- the repository-local OpenEB 5.2 CenturyArks profile;
- one tracked sample, <code>algo/tests/sparklers.raw</code>; and
- one user-authorized Terminal.app/Aqua session.

The exercised transport workflow covered one Step, three consecutive Step operations, linked Window / Rate / multiplier controls, relative x0.5 / x1.0 / x2.0 playback advancement, one observed Loop wrap, loop-disabled natural EOF, seek near 0%, seek near EOF, and close-button exit.

## 3. Explicit evidence limitations

The initial Window, Rate, multiplier, duration and position values were visible to the user but were not transcribed.

The exact Window/Rate/multiplier values after each control change were not transcribed.

The start positions, end positions and numerical deltas for x0.5, x1.0 and x2.0 were not transcribed.

This record therefore supports observable control linkage, observable relative advancement ordering, and functional Step/Loop/EOF/seek behavior. It does not support an exact control-conversion formula, exact playback ratio, precise real-time timing, numerical timestamp correctness, or a performance benchmark.

## 4. Repository and binary identity

| Field | Value |
| --- | --- |
| Branch at validation time | <code>main</code> |
| HEAD | <code>240ebf2154ae5ecab2dcb20a91c323deda786555</code> |
| Parents | <code>568110818c48abb8705809df38c16dd5cd435445</code>, <code>f3fa51cfa5674e623e80d866837e01e7300161c2</code> |
| Executable | <code>.build/ebplus-macos/gui/gui_for_openeb</code> |
| Architecture | Mach-O arm64 |

No configure, build, CTest, or install was performed during this validation.

## 5. Dependency and sample identity

The recorded run used OpenEB / Metavision SDK 5.2.0 through the repository-local CenturyArks profile, Qt Cocoa runtime, the repository-local HAL plugin directory, and the repository-local HDF5 ECF plugin directory.

| Sample field | Value |
| --- | --- |
| Path | <code>algo/tests/sparklers.raw</code> |
| Size | 2,109,142 bytes |
| Git state | tracked regular file |
| Symlink | no |

Only this RAW file was opened and decoded during the recorded GUI run. This document-only phase did not reopen or decode a data file.

## 6. Launch and automated process evidence

| Field | Value |
| --- | --- |
| LOG_ROOT | <code>.logs/ebplus-macos-raw-transport-20260723T064600Z</code> |
| RUNTIME_ROOT | <code>.tmp/ebplus-macos-raw-transport-20260723T064600Z</code> |
| PID | <code>15516</code> |
| Start | <code>2026-07-23T06:52:09Z</code> |
| End | <code>2026-07-23T06:57:00Z</code> |
| Exit | <code>0</code> |

The retained stderr records <code>libqcocoa.dylib</code> loaded; stdout is empty. The logs contain no no-screens warning, dynamic-loader, Qt-platform, or OpenGL fatal marker; no crash/abort marker; no RAW decoder/runtime error marker; and no camera/facility marker.

The retained stderr also contains these non-fatal macOS/IMK observations:

~~~text
Warning: Window move completed without beginning
error messaging the mach port for IMKCFRunLoopWakeUpReliable
~~~

The recorded child exit is <code>0</code>. These messages are not generalized to other macOS versions or workflows.

## 7. Step result

User manual evidence records that one Step advanced playback while remaining paused, updated the event display, and that three additional Step operations advanced monotonically without entering continuous playback. No crash, hang, or blocking dialog was observed.

This does not verify an exact Step delta, different accumulation-window values, Step exactly at EOF, or repeated stress operation. It does not complete <code>RAW-008</code>.

## 8. Linked Window / Rate / multiplier controls

User manual evidence records that the controls remained operable, changing one control produced observable linked updates, no invalid, negative, or visibly corrupt value was observed, and no crash or blocking dialog occurred.

The values W0/R0/M0, W1/R1/M1, W2/R2/M2, and W3/R3/M3 were not transcribed. This record therefore does not establish an exact linkage formula or rounding behavior.

## 9. Relative multiplier behavior

At x0.5, x1.0, and x2.0, playback each advanced. The user observed the expected relative advancement ordering.

The numerical position deltas were not transcribed. This is manual relative-behavior evidence, not precise timing, performance, or exact 1:2:4 ratio evidence.

## 10. Loop result

Loop was enabled near EOF. One wrap to near the beginning was observed, playback continued after the wrap, the event display continued updating, and no crash or blocking dialog was observed.

Multiple consecutive loops, long-duration loop stability, algorithm reset, XYT cleanup, filter state, and timestamp semantics across the wrap remain unverified.

## 11. Loop-disabled EOF

Loop was disabled, playback reached natural EOF, position stopped advancing, and the application remained responsive with no crash or blocking dialog.

This does not establish the internal decoder-runtime-error versus EOF classification.

## 12. Seek boundaries

Seek to approximately 0% succeeded, Step could advance from near the beginning, seek to approximately 99% succeeded, and playback could reach EOF normally with Loop disabled. No negative or visibly out-of-range position was observed.

Exact timestamp boundaries, programmatic out-of-range seek, all EOF-after-seek cases, and corrupt timeline metadata remain unverified.

## 13. GUI visual boundary

The progress-indicator clipping state was not separately reported during this run and is Not observable for this validation record.

The previously recorded partially clipped circular handle remains a deferred GUI layout issue; this run neither confirms it was present nor confirms it was fixed. See [macOS RAW core playback validation](macos_raw_core_playback_validation.md).

## 14. Filesystem output boundary

The repository-local log root occupied 24 KiB and the runtime root occupied 8 KiB. The retained <code>run.command</code> is 1,527 bytes, and the runtime <code>layout.json</code> is 356 bytes.

The <code>layout.json</code> path is runtime-output evidence, not visual-layout correctness evidence.

Complete filesystem write closure was not traced at the syscall level.

## 15. Evidence classification

| Evidence type | Boundary |
| --- | --- |
| Git identity evidence | Validation-time branch, commit, and parents |
| Mach-O/dependency provenance | Existing arm64 build-tree executable and repository-local OpenEB profile |
| Automated process and exit evidence | Retained PID, UTC timing, child exit code, stdout/stderr markers |
| Qt Cocoa plugin runtime evidence | Retained <code>libqcocoa.dylib</code> load record |
| User manual GUI interaction evidence | Step, controls, relative advancement, Loop, EOF, seek, and close-button observations |
| Observed repository-local runtime output | Isolated log/runtime roots and <code>layout.json</code> path |
| Unverified numerical inference | Exact values, deltas, timing, ratio, timestamps, and performance |

## 16. Passed

- Step behavior, including one single Step and three additional Steps;
- observable linked-control behavior;
- relative x0.5/x1.0/x2.0 advancement ordering;
- one Loop wrap;
- loop-disabled EOF;
- near-start and near-EOF seek;
- close-button exit; and
- process exit <code>0</code>.

## 17. Failed

No tested transport-control behavior failed in this narrow run.

## 18. Not run

- exact initial control values, exact linked-control values, exact position deltas, exact timing ratio, and numerical timestamp correctness;
- extreme minimum/maximum rate, different accumulation-window Step values, Step at exact EOF, multiple loops, and loop stress;
- additional RAW files, HDF5/H5/DAT, recent files, file-to-file switch, disconnect/reopen, and invalid/corrupt/permission/plugin failure paths;
- filters, algorithms, models, ONNX, and exports;
- camera/facility paths;
- installed executable and portable loader closure;
- Linux validation;
- performance, long-duration stability, and event semantic/timestamp/image-quality validation; and
- the complete layout/scale/theme matrix.

## 19. Blocked

None for this narrow validation.

## 20. Remaining Milestone 5 scope

This record does not close Milestone 5.

Only algo/tests/sparklers.raw was opened and decoded during the recorded runtime validation.
No other RAW/HDF5/H5/DAT file was opened.
No camera was opened.
No facility was accessed or changed.
No filter, algorithm or model was intentionally enabled.
No export was performed.
No installed executable was run.
No Linux validation was performed.
Complete filesystem write closure was not traced at the syscall level.
