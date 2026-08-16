# macOS Milestone 7 Slice 3A E2VID Heuristic Fallback Validation

## Status and scope

**Status:** `Complete / Qualified` for M7 Slice 3A only. M7 Slice 3 and
Milestone 7 overall remain **In progress**.

This report records the macOS Apple Silicon qualification of the E2VID
no-successfully-loaded-model path. It combines a deterministic production
lifecycle repair, focused automated fallback coverage, one bounded Cocoa
file-source session, and a final current-worktree full build and CTest run.

Validation applies to the uncommitted
`feat/macos-e2vid-fallback-qualification` worktree above committed base
`faec7455e1a8757a56876ee827d101a1f7c59578`. The base SHA identifies the
committed parent only; it does not claim that the repair or tests are already
contained in bare `HEAD`.

This slice qualifies **E2VID heuristic fallback**, not neural E2VID
inference. No ONNX Runtime, valid ONNX model, recurrent model, converted
model, model download, or model conversion was used.

## Identity and provenance

| Item | Value |
| --- | --- |
| Branch / committed base | `feat/macos-e2vid-fallback-qualification` / `faec7455e1a8757a56876ee827d101a1f7c59578` |
| Validated production diff | `algo/analytics/event_to_video.h` only |
| Validated test diff | `algo/tests/test_phase8_10.cpp`, `algo/tests/test_raw_algos.cpp`, `algo/tests/test_playback_e2v.cpp` |
| Build tree | `.build/ebplus-macos`, Release, arm64, reused without cleaning or creating a second tree |
| OpenEB profile | repository-local OpenEB / Metavision SDK 5.2 CenturyArks profile |
| ECF | `b982d908a0bc0afd9104d226607bedb1a11b2a95` |
| GUI binary | `.build/ebplus-macos/gui/gui_for_openeb`, arm64, UUID `A8A45BF1-F3B8-3965-8DE3-724C14F2AA4E`, 2,084,576 bytes, mtime `2026-08-15T21:20:35+0800` |
| GUI linkage | repository-local OpenEB 5.2 RPATH; no `/usr/local` OpenEB 5.1.1, x86_64, OpenEB producer build-tree dylib, or `libonnxruntime` observed |
| ONNX configuration | `EBPLUS_ONNXRUNTIME_MODE=AUTO`; `EBPLUS_ONNXRUNTIME_ROOT` empty; no `GUI_ALGO_HAS_ONNXRUNTIME` in configured targets |
| ONNX/model inventory | no configured compatible arm64 ONNX header/library pair; no tracked or worktree `.onnx`, `.pth`, or `.pth.tar`; `models/e2vid_lightweight.onnx` absent |

The GUI executable was rebuilt during the bounded GUI gate after the
`event_to_video.h` repair. Its `analytics_backends.cpp.o` and `libgui_core.a`
timestamps are later than the repaired header and match the executable's
mtime. A current dependency dry run reports the GUI target up to date, so the
final whole-tree build did not need to relink it again.

## Fixed input and no-model paths

The only real recording used by this slice is the tracked regular file
`algo/tests/sparklers.raw`:

| Property | Value |
| --- | --- |
| Size | 2,109,142 bytes |
| SHA-256 | `e84afbecdc07d2910ae846a4ae0ee246f5b9c97a53816c637d4f85c023d7c234` |
| Encoding / geometry | EVT2 / 640x480 |
| CD events | 521,252 |
| Shifted timestamps | 0..95,871 us |

No second RAW fixture, image dump, video, model, or fake ONNX file was
created. The automated missing-model tests use the repository-local path
`.tmp/m7-e2vid-fallback/nonexistent-model.onnx`; they assert that it is absent
before and after the load attempt. The Cocoa session used the separate absent
path
`.tmp/m7-e2vid-gui-20260815T214201+0800/nonexistent-model.onnx`, also absent
before and after the session.

## Deterministic lifecycle defect and repair

### Reproduced defect

The regression was added before production repair and run against the prior
implementation. It uses public `EventToVideo` behavior with an 8x8 geometry:

```text
pending A: OFF at (2,2), t=100 and t=200
transition: E2VID -> BardowVariational -> E2VID
intended B: ON at (5,4), t=1000 and t=1100
comparison: switched instance versus fresh E2VID B-only instance
```

Before the repair, pending A contaminated the next E2VID reconstruction. The
maximum `CV_8UC1` pixel difference from the fresh B-only reference was `59`.
The result was deterministic and established a real product lifecycle defect,
not a source-inspection hypothesis or a timing-sensitive test-design incident.

### Minimal repair and preserved state

On a cross-mode `EventToVideo::set_mode()` transition, the existing
`reset_state()` is now followed by:

```cpp
e2vid_.reset();
e2vid_event_buffer_.clear();
intensity_rescaler_.reset();
```

The final regression uses the same input and requires exact equality
(`NORM_INF == 0`) between the switched and fresh frames. It passed after the
repair and again in the final full CTest.

| Classification | State |
| --- | --- |
| Invalidated on cross-mode change | Bardow/InteractingMaps reconstruction buffers through `reset_state()`, E2VID voxel/inference temporal state, pending E2VID events, and intensity-rescaler bounds/history |
| Preserved | selected new mode, model path, loaded model/session state, number of bins, internal downsample setting, hot-pixel mask, Auto HDR setting, unsharp parameters, and bilateral parameters |
| Same-mode behavior | unchanged no-op because `set_mode()` still acts only when `mode_ != m` |
| Deliberately outside this repair | `current_t_` and `last_frame_t_` are not reset by a mode transition; the qualified contract uses monotonic source timestamps and does not establish behavior after a mode switch followed by timestamp rollback |

`E2VIDInference::reset()` resets voxel state and, in a future ONNX-enabled
build, recurrent transient buffers. It does not unload a model or clear its
path/configuration. `IntensityRescaler::reset()` clears adaptive bounds while
preserving its Auto HDR setting. The repair therefore invalidates temporal
history without turning a mode change into a complete application reset.

## Automated qualification

The focused Slice 3A execution recorded `33/33` passed, `0` failed, `0`
timeout, in `9.47 s`. A raw log for that exact focused selection was not
retained, so it is historical execution context rather than the final machine
authority. The final authority is the fresh full CTest below, whose
`LastTest.log` includes all listed test registrations.

| Area | Current automated contract |
| --- | --- |
| Cross-mode lifecycle | `EventToVideoTest.ModeSwitchClearsPendingE2VIDTemporalState` compares the switched 8x8 frame with the fresh B-only frame exactly |
| Direct fallback | `E2VIDInferenceTest.HeuristicInference` feeds ON/OFF events at multiple coordinates/times with `model_loaded == false` and requires a non-empty sensor-sized `CV_8UC1` frame in range 0..255 plus expected polarity ordering |
| EventToVideo integration | `EventToVideoTest.E2VIDModeHeuristic` exercises `Mode::E2VID`, valid output, no loaded model, and exact reset/replay equality |
| Missing model | `E2VIDInferenceTest.ModelLoadFailure` and `EventToVideoTest.E2VIDModelLoadFailure` use the absent repository-local path, require load failure and `model_loaded == false`, then require usable heuristic output without creating a file |
| Reset / bins / downsample | deterministic `E2VIDInference::reset()` replay; default 5-bin construction, clamp to 1..20, alternate valid 10-bin inference; and internal downsample true/false both retain sensor dimensions |
| Mask / helper boundary | voxel/rescaler helper tests run; the hot-pixel test exercises mask setup, bin rebuild, and successful infer, but does not independently assert effective mask preservation or image quality |
| Tracked RAW | `AllModes/EventToVideoRawTest.ProducesNonFlatFiniteFrame/2` runs E2VID with `model_loaded == false` on centered real RAW activity and requires frames, correct dimensions/type, finite bounds, and at least one non-flat frame |
| Playback simulation | `playback_e2v` feeds 1,352 ROI events in 100 us scaled-timestamp windows through all three modes with exit assertions; E2VID heuristic produced 959 frames: 493 non-flat, 463 black empty-window, and 3 gray frames |
| Bardow / InteractingMaps regression | the same playback run produced 959 non-flat frames for each non-E2VID mode; the retained `test_raw_e2v` diagnostic for modes 0/1 also exited 0 |

The black E2VID empty-window frames in the scaled playback diagnostic are
expected when no events are supplied for a window. The pass criterion is not
"every frame is non-flat"; it is valid frame shape/type plus at least one
non-flat frame, so the diagnostic does not misrepresent empty input as image
quality failure.

## Bounded Cocoa GUI evidence

No GUI was launched during this final full-tree/documentation gate. The one
earlier bounded no-model Cocoa session used fresh repository-local roots:

| Item | Value |
| --- | --- |
| Runtime roots | `.tmp/m7-e2vid-gui-20260815T214201+0800/`, `.logs/m7-e2vid-gui-20260815T214201+0800/`, `.artifacts/m7-e2vid-gui-20260815T214201+0800/` |
| Wrapper / GUI PID | `58232` / `58235` |
| Exit | `0`; wait returned and subsequent `kill -0` found no GUI process |
| stdout | 0 bytes |
| Fatal-marker scan | no `SIGSEGV`, `SIGABRT`, `QScreen`, `no screens`, fatal, dyld fatal, onnxruntime load, or device-error marker |
| stderr limitations | Qt plugin debug, one window-move warning, and macOS IMK mach-port diagnostics only |

The maintainer's human workflow was:

```text
open sparklers.raw
enable Event -> Video (E2VID) and observe one standalone grayscale window
enter the absent model path
change Num bins: 5 -> 6 -> 5
switch: E2VID -> BardowVariational -> E2VID
seek: 250 -> 750 -> 250, then resume
disable and re-enable Event -> Video
Cmd+Q
```

The GUI registry exposes `Event -> Video (E2VID)` with modes
`0=BardowVariational`, `1=InteractingMaps`, and `2=E2VID`. In E2VID mode the
visible mode-specific parameters were `Model path (ONNX)`, `Num bins` (1..20,
default 5), `Auto HDR`, `Unsharp amount`, `Unsharp sigma`, and `Bilateral
sigma`; `Output fps` is common.

The session is human Cocoa wiring/lifecycle evidence: file playback,
standalone presentation, parameter visibility, mode switching, seek,
enable/disable lifecycle, and clean exit remained operable. It does not make
a neural-inference, numerical-frame-correctness, image-quality, performance,
or long-duration-stability claim. The known no-model state comes from the
build/model provenance and automated `model_loaded == false` contracts, not
from interpreting a visible grayscale frame as neural output.

## Designed ROI and shared-downsample coordination

The observed ROI/downsample change when enabling `Event -> Video (E2VID)` is
expected GUI design behavior, not an anomaly. The automation key is
`event_to_video`, so it applies to that algorithm rather than exclusively to
enum mode 2:

1. `MainWindow` saves the prior unified ROI state and calls
   `camera_.set_unified_roi(true, -1, -1, 256, 144)`.
2. It saves the prior shared `preproc_downsample` value and enables it.
3. `EventToVideoBackend::rebuild()` uses the shared preprocessor dimensions
   and explicitly calls `set_e2vid_downsample(false)` and
   `set_downsample(false)`, avoiding double coordinate halving.
4. Disabling `event_to_video` restores the saved ROI and shared-downsample
   states.

`AlgorithmsPanel::algo_defaults_to_roi()` applies the same class of
heavy-algorithm ROI policy to Time Surface and other listed heavy algorithms.
This session confirms designed GUI wiring and lifecycle restoration only; it
does not by itself prove full ROI coordinate or preprocessing numerical
correctness.

## Final whole-tree qualification and workspace

| Check | Result |
| --- | --- |
| Full build | `cmake --build .build/ebplus-macos --parallel 4` passed without warnings; existing tree only |
| Full CTest discovery | 378 tests |
| Full CTest | `ctest --test-dir .build/ebplus-macos --output-on-failure --parallel 2`: `378/378` passed, 0 failed, 0 timeout, 40.81 s |
| Fresh machine record | `.build/ebplus-macos/Testing/Temporary/LastTest.log`, mtime `2026-08-16T00:18:58+0800` |
| Disk preflight | 228.27 GiB total, 37.85 GiB available, 34.24 GiB protection line, approximately 3.61 GiB margin; whole-tree build/CTest budget below 100 MiB |
| Post-build/CTest workspace | 37.84 GiB available; repository 503.54 MiB, `.build` 196.82 MiB, `.deps` 17.09 MiB, `.logs` 9.37 MiB, `.tmp` 83.23 MiB, `.artifacts` 13.21 MiB |

No model or large image/video artifact was created. Existing GUI and test
evidence roots were preserved. No project-controlled files were written
outside the repository.

## Evidence boundary and closure result

This Slice 3A closure supports:

- deterministic cross-mode invalidation of pending E2VID temporal state;
- no-model and absent-model-path heuristic fallback;
- valid bounded fallback frames for synthetic input, tracked RAW, and scaled
  file-playback simulation;
- reset/replay, representative bins, internal downsample true/false, and
  limited hot-mask/rescaler helper contracts;
- retained BardowVariational and InteractingMaps regressions; and
- one real Cocoa file-source no-model E2VID wiring/lifecycle session with a
  clean exit.

It does **not** support claims for ONNX Runtime, plain or recurrent neural
E2VID inference, model conversion, neural image quality, all parameter
combinations, exact ROI numerical semantics, performance, long-duration
stability, physical camera/M6, export, packaging, Linux inference, or Linux
runtime.

**Linux remains Not run / unverified.** No Linux risk acceptance is added by
this macOS-only evidence.

M6 remains **Planned / Paused — physical CenturyArks camera currently
unavailable**. This file-source/GUI work does not substitute for camera,
facility, live-stream, recording, or reconnect qualification.

M7 Slice 3A heuristic-fallback qualification and documentation closure
preparation are complete. At the time this validation report was prepared, the
next recommended action was a final read-only pre-commit audit; this report
does not authorize staging, commit, push, pull request, merge, or a later
E2VID neural/model sub-phase.
