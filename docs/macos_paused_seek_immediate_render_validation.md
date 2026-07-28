# macOS Paused Seek Immediate-render Validation

## Status

**Passed -- bounded macOS arm64 build-tree paused file-seek presentation
regression for one RAW fixture and one ROI FilterChain configuration.**

Milestone 5 remains `Planned`.

## Baseline and defect

- Baseline: `main` at `7bb8213160493e982f07c2a8d29aeb261d47ae1d`.
- Input: tracked `algo/tests/sparklers.raw`.
- The pre-fix ROI lifecycle smoke found that paused forward and backward
  timeline seeks changed position, but left the displayed frame unchanged
  until Resume.
- The pre-fix behavior did not crash or hang the application, but violated
  the immediate seek presentation contract.

## Root cause and fix

`PlaybackControls` handled only `QSlider::sliderMoved`. A timeline click,
page-step, or keyboard action can change `QSlider::valueChanged` without
emitting that drag-specific signal, bypassing `PlaybackController::seek()`.

The fix listens to `QSlider::valueChanged` and blocks the slider signal while
initializing its open-file value. Controller-driven position updates already
block slider signals, so normal playback does not feed back into seek.

The change is shared C++ with no Apple- or Linux-specific conditional, delay,
event-loop pumping, play/pause workaround, file reopen, or OSC seek.

## Automated validation

- `PausedSeekImmediateRender.FramesAndDisplayUpdateSynchronously` passed
  1/1 in 0.36 s.
- The test uses a synthetic 4x4 event buffer with two distinct timestamp
  windows while playback is paused. It requires the synchronous order
  `seeked -> events_window_ready -> frame_ready -> position_changed`, correct
  target position, different output frames, and immediate replacement of
  `EventDisplayWidget::current_frame()`.
- Full CTest passed 311/311 in 23.88 s.

## Post-fix runtime validation

- Platform: macOS Apple Silicon arm64 build-tree GUI with repository-local
  OpenEB 5.2 CenturyArks.
- GUI UUID: `3CCE16B3-E5F9-3995-8486-B88E2D56FC91`.
- Terminal wrapper PID: `60317`; direct GUI child PID: `60334`; GUI exit: `0`.
- With playback paused and ROI Filter enabled, one forward and one backward
  seek each immediately updated the displayed target window without Resume.
  Position and displayed frame were qualitatively observed to correspond.
- Resume playback, ROI Filter enable/disable and re-enable, and same-file
  reopen with consistent ROI state all passed.
- The fatal-marker scan found no `SIGSEGV`, `SIGABRT`, `EXC_BAD_ACCESS`,
  uncaught exception, `102113`, device-unavailable, Qt fatal, dyld fatal, seek,
  or FilterChain error marker.

## Observed limitations

- One RAW fixture and one ROI filter configuration only.
- Qualitative displayed-frame correspondence only.
- No numerical pixel or event-correctness measurement.
- One non-fatal Cocoa/IMK mach-port warning was recorded.

## Not run

- Physical camera workflows.
- Model-backed workflows.
- Export and AVI workflows.
- Long-duration stability testing.
- Linux compilation and runtime comparison.

This result does not establish numerical pixel/event correctness, all seek or
filter configurations, all playback controls, long-term stability, or Linux
parity.
