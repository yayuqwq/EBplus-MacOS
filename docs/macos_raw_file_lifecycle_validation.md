# macOS RAW File Lifecycle Robustness Validation

## 1. Status

**Status:** Passed — narrow macOS RAW file-lifecycle robustness validation for the tested build-tree scenarios after the lifecycle fix.

Milestone 5 remains <code>Planned</code> and is not closed by this result.

## 2. Scope

This record is limited to:

- macOS Apple Silicon arm64;
- branch <code>fix/raw-file-lifecycle-robustness</code> based on main <code>8d69a2c669447d2b829d2fe9c8fe6efa49f7d364</code>;
- the build-tree executable <code>.build/ebplus-macos/gui/gui_for_openeb</code>;
- the repository-local OpenEB 5.2 CenturyArks profile;
- tracked <code>algo/tests/sparklers.raw</code>;
- generated unique copy, empty RAW, truncated RAW, and unsupported text samples; and
- two Terminal.app/Aqua sessions.

No installed executable, camera, facility, algorithm, model, export, Linux, HDF5, H5, or DAT workflow was exercised by this validation.

## 3. Original defect evidence

Before this fix, an empty RAW reproduction caused exit 134 / <code>SIGABRT</code> from an uncaught <code>Metavision::HalException</code> with Error 101000. The captured boundary was:

~~~text
DeviceDiscovery::open_raw_file
-> Camera::from_file
-> CameraController::connect_file
-> PlaybackController::open_file
-> MainWindow
~~~

This was an abort from an uncaught file-open business failure. It was not a <code>SIGSEGV</code>, Cocoa/plugin-loader failure, or camera/facility failure.

## 4. Implemented fix

The change is platform-neutral and contains no <code>__APPLE__</code> or Linux-specific branch.

1. <code>CameraController::connect_file()</code> catches <code>Metavision::BaseException</code> at the file-open boundary, emits <code>disconnected()</code> and the detailed error, then returns <code>false</code>.
2. <code>PlaybackController::open_file()</code> resets source-specific state before switching, commits the path only after successful connect/start, preserves loop preference, and disconnects the newly created source if start fails.
3. <code>MainWindow</code> no longer adds a second generic file-open failure dialog; the detailed <code>CameraController::error</code> dialog remains.

Live-camera connection paths were not changed by this diff.

## 5. Automated regression evidence

The following results are retained execution-report evidence; this document-only closure did not rerun them:

- incremental build: Passed;
- <code>CameraControllerLifecycle.EmptyRawFailureIsCaught</code>: 1/1 Passed; and
- full CTest: 309/309 Passed.

The build tree registers <code>CameraControllerLifecycle.EmptyRawFailureIsCaught</code>, and its CTest cost data contains 309 test entries. The exact focused and full-pass counts are attributed to the prior maintainer/Codex execution report, not to a new run in this phase.

## 6. Run 1

| Field | Value |
| --- | --- |
| PID | <code>62665</code> |
| Start | <code>2026-07-24T11:31:46Z</code> |
| End | <code>2026-07-24T11:40:36Z</code> |
| Exit | <code>0</code> |

Manual evidence records:

- valid original RAW open and auto-play;
- same-file reopen auto-resume;
- different-path unique copy switch and auto-play;
- unique copy entered Recent once by exact path;
- Recent reopen of the original and auto-play;
- unsupported file produced exactly one detailed error dialog, followed by valid-original recovery; and
- empty RAW produced exactly one controlled error dialog without abort, followed by valid-original recovery.

## 7. Truncated RAW

The generated truncated RAW was opened far enough to enter Recent and the application subsequently recovered to the valid original RAW without an observed crash or hang.

The precise UI classification was not transcribed during the manual run. It is therefore recorded as observed but not precisely classified.

This does not establish a short EOF pass, a controlled-error pass, or complete truncated-RAW handling.

## 8. Run 2 stale Recent

The unique copy was deleted only after Run 1 success and explicit combined authorization.

| Field | Value |
| --- | --- |
| PID | <code>64140</code> |
| Start | <code>2026-07-24T11:51:21Z</code> |
| End | <code>2026-07-24T11:55:06Z</code> |
| Exit | <code>0</code> |

At the next process start, the stale unique Recent item remained visible. Clicking it produced one <code>File no longer exists</code> warning without a crash. The item was removed after the warning, the original RAW remained available, and Recent reopen of the original auto-played.

## 9. QSettings / validation harness boundary

macOS Native QSettings used user-domain CFPreferences persistence. HOME/XDG runtime-root isolation did not isolate Native QSettings.

This was classified as a validation-harness limitation, not a demonstrated Recent product bug. The final validation instead used unique generated basenames and exact full-path Recent snapshots/differentials. Different absolute paths with the same basename are valid distinct Recent entries.

## 10. Recent differential

The pre-run snapshot contained historical paths. They were retained as baseline state and were not treated as failures.

Run 1 exact-path counts were:

- current unique copy: 1;
- empty RAW: 0;
- unsupported text: 0; and
- truncated RAW: 1.

Run 2 exact-path counts were:

- deleted unique copy: 0;
- empty RAW: 0;
- unsupported text: 0; and
- truncated RAW: 1.

The snapshots are retained under the repository-local final lifecycle log root. Paths are described here relative to the validation runtime root rather than copied as machine-specific absolute paths.

## 11. Automated logs

Both recorded runs exited 0. Their stderr logs contain no <code>SIGABRT</code>, <code>SIGSEGV</code>, uncaught termination, dyld fatal, or Qt fatal marker. Run 2 contains one observed non-fatal <code>IMKCFRunLoopWakeUpReliable</code> mach-port message; no broader interpretation is made.

## 12. Passed

- empty RAW crash regression;
- single detailed error-dialog behavior for the exercised unsupported and empty file-open failures;
- recovery after unsupported failure;
- recovery after empty RAW failure;
- same-file reopen auto-resume;
- different-path file switch;
- Recent original reopen;
- unique stale Recent warning and removal; and
- Run 1 and Run 2 clean exit.

## 13. Failed

None among the precisely asserted post-fix lifecycle contracts.

## 14. Observed but not precisely classified

- generated truncated RAW UI outcome.

## 15. Not run / remaining

- other RAW files and a broader corrupt/truncation corpus;
- permission-denied and missing HAL-plugin cases;
- HDF5, H5, DAT, and different-geometry file switch;
- manual disconnect/reopen, filters, algorithms, models, ONNX, and export;
- camera and facility paths;
- installed executable and portable loader closure;
- Linux runtime validation;
- performance and long-duration stability; and
- the complete layout matrix.

## 16. Filesystem boundary

The final lifecycle log root occupied 64 KiB and the runtime root occupied 96 KiB. The current unique copy was deleted as explicitly authorized; all other evidence was retained.

Complete filesystem write closure was not traced at syscall level.

## 17. Milestone impact

This closes the reproduced lifecycle robustness defects in the tested macOS build-tree scenarios.

It does not close Milestone 5.
