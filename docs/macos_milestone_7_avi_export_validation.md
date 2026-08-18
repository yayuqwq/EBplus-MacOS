# macOS Milestone 7 Slice 4B AVI Export Validation

## Status and scope

**Status:** `Complete / Qualified` for M7 Slice 4B. Together with the earlier CSV and RAW-clip Slice 4A evidence, M7 Slice 4 is `Complete / Qualified` for its bounded tracked-source export scope. Milestone 7 remains **In progress** because Slice 3 model work, calibration after M6, and processed-recording work remain outside this slice.

This report qualifies the current macOS Apple Silicon AVI path for one tracked RAW fixture. It combines deterministic `ExporterController` tests, a bounded Cocoa `Export Recording` workflow, repository-local OpenCV/FFMPEG readback, and a final full build and CTest run. AVI is lossy media; no pixel-exact image claim is made.

Qualification was performed on the then-uncommitted `feat/macos-avi-export-qualification` worktree above committed base `888e4e9cf564e8387d6c679f67951f025c23bf02`. This is a validation-time historical fact: the base identifies the committed parent, not a claim that the test addition was contained in bare `HEAD` at that time.

The validated worktree change is test-only: `gui/tests/test_exporter_controller.cpp` (`497` additions, `3` deletions). There was no Slice 4B diff relative to the base in `gui/exporter/`, `gui/app/`, `gui/panels/`, `gui/main_window.*`, CMake, or documentation. No production behavior was changed for this slice.

## Identity and provenance

| Item | Value |
| --- | --- |
| Validation branch / committed base | `feat/macos-avi-export-qualification` / `888e4e9cf564e8387d6c679f67951f025c23bf02` |
| Build tree | Existing `.build/ebplus-macos`, Release arm64 tree; no second build or dependency tree |
| GUI binary | `.build/ebplus-macos/gui/gui_for_openeb`, arm64, 2,084,576 bytes, UUID `A8A45BF1-F3B8-3965-8DE3-724C14F2AA4E` |
| OpenEB profile | Repository-local OpenEB / Metavision SDK 5.2 CenturyArks profile |
| ECF | `b982d908a0bc0afd9104d226607bedb1a11b2a95` |
| GUI linkage | Repository-local OpenEB 5.2 RPATH; no `/usr/local` OpenEB 5.1.1, x86_64, or OpenEB producer-build-tree dylib observed |
| Video environment | OpenCV 4.12.0 with the FFMPEG video I/O backend |

The build-tree executable is not a standalone loader, `.app`, packaging, signing, or notarization qualification. Its OpenEB provenance and codec availability are properties of this repository-local build/runtime profile.

## Fixed fixture and current AVI architecture

The only real recording used is tracked regular file `algo/tests/sparklers.raw`:

| Property | Value |
| --- | --- |
| Size | 2,109,142 bytes |
| SHA-256 | `e84afbecdc07d2910ae846a4ae0ee246f5b9c97a53816c637d4f85c023d7c234` |
| Encoding / geometry | EVT2 / 640x480 |
| CD events | 521,252 |
| Shifted timestamps | 0..95,871 us |

The current application route is:

```text
Settings -> File Tools -> Export Recording
-> ExportDialog -> ExportParams -> ExporterController::start()
-> background worker -> ExporterController::run_avi()
```

`ExportDialog` is shown from `MainWindow::on_export_dialog()` and pre-fills the currently opened playback file while still exposing a source-file chooser. AVI is the first/default format. Its actual controls are a source field, output field, format selector, FPS spin box (1..120, default 30), accumulation spin box (1..1,000,000 us, step 100, default 33,000 us), codec-select spin box (1..100, default 90), Color checkbox (default checked), progress bar, status label, Start, Cancel, and Close.

The codec-select control is a policy selector, not encoder quantization: `quality >= 50` first requests H264; `quality < 50` requests MJPG directly. If the high-quality H264 writer cannot open, the current implementation logs a warning and retries MJPG. `p.fps` is the AVI container playback FPS; it is independent of `p.accumulation_us`, which is passed directly as the `PeriodicFrameGenerationAlgorithm` period.

`run_avi()` uses that generator and a direct synchronous `cv::VideoWriter` on the SDK read callback. It fills quiet gaps with black frames and `force_generate()` flushes the trailing partial accumulation window. Color output is written as BGR and grayscale output as one channel, matching the writer's `isColor` setting. This is not the historical `CDFrameGenerator` / `CvVideoRecorder` path.

## Deterministic automated qualification

The test extends the existing `gui_core`-linked `test_exporter_controller` target; it does not compile a second production implementation. The retained direct gate was `3/3` passed in `0.85 s`; the retained focused set was `5/5` passed, 0 failed, 0 timeout, in `1.11 s`.

The final full-tree CTest below is the fresh machine authority. It includes the existing HDF5 geometry/metadata regression plus these four AVI contracts:

| Contract | Parameters and result |
| --- | --- |
| Direct MJPG, color and gray | `quality=49`, 30 FPS. Color used 20,000 us accumulation and produced 5 oracle / 5 decoded frames, 640x480, 119,228 bytes. Gray used 40,000 us and produced 3 / 3 frames, 640x480, 89,964 bytes. The shorter accumulation produced more frames. |
| Frame completeness / FPS | An independent test-side `Camera` plus `PeriodicFrameGenerationAlgorithm` oracle uses `real_time_playback(false)` and `time_shift(true)`, mirrors quiet-gap filling and final `force_generate()`, and is independent of `ExporterController` and `VideoWriter`. Decoded frames are counted by actual `VideoCapture::read()`, not only metadata. Readback FPS is 30.0 within a 0.1 FPS tolerance. |
| Color / grayscale presentation | The color case has more than 1,000 representative BGR pixels with channel difference greater than 12. The gray case permits decoder BGR presentation but requires its channels to agree within the lossy tolerance (maximum difference 3). |
| High-quality path | `quality=90`, color, 20,000 us, 30 FPS completed and reopened as actual `h264`, 640x480, 5 decoded frames, 45,698 bytes. The test contract permits MJPG only as the product's H264-unavailable fallback. |
| Deterministic error | A deliberately absent, repository-local output parent caused `started` then `failed`, no `completed`, and no readable AVI. The exact OpenCV backend wording is not part of the assertion. |

The standalone capability probe is intentionally narrower than EBplus export: it writes eight synthetic 64x48 frames through the currently linked OpenCV. Both codecs opened and reopened through FFMPEG at 30 FPS:

| Probe | FourCC | Decoded frames | File size |
| --- | --- | --- | --- |
| MJPG | `MJPG` | 8/8 | 9,286 bytes |
| H264 | `h264` | 8/8 | 7,332 bytes |

The CTest synthetic probe uses a distinct frame sequence, so its retained 7,666-byte MJPG and 6,852-byte H264 test artifacts are not substituted for the standalone-probe sizes. The error-path run has an expected OpenCV writer-open warning; it is not described as a zero-warning test.

No cancellation test was added. The fixture finishes too quickly and no deterministic public seam establishes that the AVI worker is active before a cancel request; a sleep-and-race test would not be valid evidence. The small decode polling sleep in the independent oracle is not a cancellation test.

## Bounded Cocoa Export Recording evidence

The authoritative Cocoa restart session used repository-local runtime roots:

| Item | Value |
| --- | --- |
| Runtime roots | `.tmp/m7-slice4b-gui-restart-20260817T221434+0800/`, `.logs/m7-slice4b-gui-restart-20260817T221434+0800/`, `.artifacts/m7-slice4b-gui-restart-20260817T221434+0800/` |
| Wrapper / GUI PID | `50790` / `50792` |
| Lifecycle | Started `2026-08-17T22:15:32+0800`, GUI exit `0`, finished `2026-08-17T22:23:18+0800` |
| stdout | 0 bytes |
| stderr limitations | Font substitution, window-move, IMK mach-port, and CapsLock diagnostics |
| Fatal-marker scan | No `SIGSEGV`, `SIGABRT`, `QScreen`, `no screens`, fatal, dyld, or device-error marker |

The maintainer opened the tracked RAW in the main Cocoa window, used File Tools to open `Export Recording`, and exported twice from that process:

| GUI case | Settings | Retained output and readback |
| --- | --- | --- |
| H264 / color | AVI, 30 FPS, 20,000 us, quality 90, Color enabled | `.artifacts/m7-slice4b-gui-20260817T175327+0800/sparklers-gui-h264-color.avi`, 45,698 bytes; FFMPEG readback `h264`, 640x480, 30 FPS, 5 decoded frames |
| MJPG / gray | AVI, 30 FPS, 40,000 us, quality 49, Color disabled | `.artifacts/m7-slice4b-gui-restart-20260817T221434+0800/sparklers-gui-mjpg-gray.avi`, 89,964 bytes; FFMPEG readback `MJPG`, 640x480, 30 FPS, 3 decoded frames |

The H264 artifact uses the earlier preplanned artifact-root name, but its mtime is `2026-08-17T22:17:41+0800`, inside the authoritative restart wrapper's 22:15:32..22:23:18 interval. Together with the maintainer's contemporaneous observation that both exports completed before Cmd+Q, this associates the file with the successful restart session. Mtime alone is not treated as a substitute for the human GUI observation.

An earlier detached launch with wrapper PID `50542` and GUI PID `50545` did not show a usable visible window and did not retain a GUI exit code. It is discarded/non-qualifying lifecycle evidence and is not counted as a second successful Cocoa session. Its matching H264 path is an output location, not evidence that that detached launch succeeded.

The dialog status uses one current `Done: <path>` label, not a historical per-export log. It remains visible after a successful export until a later operation changes it (for example, Start changes it to `Exporting...`). Thus a previous `Done:` path observed while preparing the second export is current UI state, not a second output or an export-error signal.

The Cocoa evidence proves real dialog wiring, setting transfer, output creation, bounded lifecycle, and clean process exit. Exact frame-count/oracle semantics, codec capability, color/gray properties, FPS, and deterministic failure behavior remain the automated evidence layer. No attempt was made to reopen an AVI as an EBplus input; retained post-session readback uses OpenCV/FFMPEG.

## Final whole-tree qualification and workspace

| Check | Result |
| --- | --- |
| Full incremental build | `cmake --build .build/ebplus-macos --parallel 4` passed using the existing tree; no new production source change or second build tree |
| Full CTest discovery | 387 tests |
| Fresh full CTest | `ctest --test-dir .build/ebplus-macos --output-on-failure`: 387/387 passed, 0 failed, 0 timeout, 80.02 s |
| Fresh console record | `.logs/m7-slice4b-full-ctest-20260818T1321+0800.log` |
| Fresh CTest record | `.build/ebplus-macos/Testing/Temporary/LastTest.log`, mtime `2026-08-18T13:24:48+0800` |

The fresh full CTest includes all four AVI registrations. The pre-existing `LastTestsFailed.log` entry from an older Slice 2 `loop_flip` failure was preserved as a historical stale artifact; the new full CTest console record shows 100% passed and 0 failed.

The completion workspace audit measured 228.274 GiB total and 41.008 GiB available, against a 34.241 GiB protection line (6.767 GiB margin). It measured approximately 589.10 MiB for the repository, 273.08 MiB for `.build`, 17.09 MiB for `.deps`, 9.52 MiB for `.logs`, 83.39 MiB for `.tmp`, and 21.93 MiB for `.artifacts`. The existing build/dependency roots and historical evidence were retained. All project-controlled runtime roots, logs, test artifacts, and AVI outputs are repository-local. No project-controlled files were written outside the repository.

## Evidence boundary and remaining work

This Slice 4B closure supports:

- deterministic tracked-RAW MJPG color and grayscale AVI export/readback;
- independent expected-frame-count versus decoded-frame-count evidence for two bounded accumulation periods, including quiet-gap and trailing-window rules;
- 30 FPS container readback, 640x480 geometry, bounded color/gray presentation checks, and deterministic missing-parent failure behavior;
- current-environment H264 direct output for high quality, plus separate MJPG and H264 OpenCV capability probes; and
- one bounded macOS Cocoa Export Recording workflow with two outputs and clean process exit.

It does not support claims for H264-to-MJPG fallback **at runtime** (H264 was available; fallback is source-inspected only), cancellation, disk-full, large-file/performance behavior, AVI audio, all input formats, live-camera recording/export, all OpenCV/FFMPEG installations, general algorithm-result export, physical camera/M6, packaging, or Linux AVI parity.

**Linux remains Not run / unverified.** No Linux risk acceptance is created by this macOS-only qualification. M6 remains **Planned / Paused — physical CenturyArks camera currently unavailable**.

## Closure result

M7 Slice 4B AVI qualification and documentation closure preparation are complete. M7 Slice 4 is `Complete / Qualified` within its CSV, RAW-clip, and AVI tracked-source export scope. Milestone 7 remains **In progress**.
