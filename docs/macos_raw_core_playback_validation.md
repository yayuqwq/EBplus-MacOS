# macOS RAW Core Playback Validation

## 1. Status

**Status:** Passed — narrow macOS <code>.raw</code> core playback validation.

The broader RAW playback parity milestone remains <code>Planned</code> and is
not closed by this validation.

## 2. Scope

This record is limited to:

- macOS Apple Silicon / arm64;
- current <code>main</code> at <code>568110818c48abb8705809df38c16dd5cd435445</code>;
- the build-tree executable <code>.build/ebplus-macos/gui/gui_for_openeb</code>;
- the repository-local OpenEB 5.2 CenturyArks profile;
- tracked <code>algo/tests/sparklers.raw</code>; and
- two user-authorized Terminal.app/Aqua sessions.

The exercised workflow was File → Open File, <code>.raw</code> selection,
successful RAW open, non-empty changing event display, duration/position
observation, pause/resume, forward seek, backward seek, natural EOF, EOF
recovery, close-button exit, second open, and Cmd+Q exit.

## 3. Repository and build identity

| Field | Value |
| --- | --- |
| Branch at validation time | <code>main</code> |
| HEAD | <code>568110818c48abb8705809df38c16dd5cd435445</code> |
| Parents | <code>31ee991768ac157af6926267e22639f2c3364218</code>, <code>9fe07e0f60f2c9726801853836bcb932aed53bfe</code> |
| Executable | <code>.build/ebplus-macos/gui/gui_for_openeb</code> |
| Architecture | Mach-O arm64 |

No configure, build, CTest, or install was performed during the successful
Terminal.app validation runs.

## 4. Dependency provenance

The build-tree executable used the repository-local OpenEB / Metavision SDK
5.2.0 CenturyArks profile. The runtime used the repository-local HAL plugin
directory:

~~~text
.deps/openeb-5.2.0-centuryarks-macos/lib/metavision/hal/plugins
~~~

and the repository-local HDF5 ECF plugin directory:

~~~text
.deps/openeb-5.2.0-centuryarks-macos/lib/hdf5/plugin
~~~

The successful logs record the Qt Cocoa platform plugin. No <code>/usr/local</code>
OpenEB dependency provenance or x86_64 dependency contamination was observed.
Homebrew Qt/OpenCV dependencies are not OpenEB provenance.

## 5. Sample identity

| Field | Value |
| --- | --- |
| Path | <code>algo/tests/sparklers.raw</code> |
| Size | 2,109,142 bytes |
| Git state | tracked regular file |
| Symlink | no |

Only this RAW file was opened and decoded.

## 6. Launch-context discrimination

### Earlier failed launch

The earlier run retained at
<code>.logs/ebplus-macos-raw-core-20260722T124704Z</code> started at
<code>2026-07-22T12:48:20Z</code>, ended at <code>2026-07-22T12:48:24Z</code>,
and exited <code>139</code> before File → Open File. No RAW was selected or
decoded in that run. Its stderr included:

~~~text
no screens available, assuming 24-bit color
~~~

The matching crash stack recorded:

~~~text
QScreen::geometry()
QWidget::saveGeometry()
gui::LayoutManager::capture_default()
gui::MainWindow::MainWindow()
main
~~~

The earlier exit 139 occurred in the no-screen automation launch context used
by that run and did not reach the file-open path. It was not RAW decoder
evidence. The retained failed-run artifacts do not preserve a PPID, TTY, or
complete environment snapshot, so this statement does not generalize to every
automation context or establish a source-code root cause.

### Terminal.app/Aqua discrimination

The independent idle launch-context run retained at
<code>.logs/ebplus-macos-launch-context-20260722T130803Z</code> ran from
<code>2026-07-22T13:15:48Z</code> to <code>2026-07-22T13:16:28Z</code> with
exit <code>0</code>; the window was displayed and remained interactive. The
subsequent RAW validation used normal Terminal.app/Aqua sessions and also
exited <code>0</code>.

The earlier exit 139 was not reproduced in the normal Terminal.app/Aqua
session used for the subsequent validation.

## 7. Run 1

| Field | Value |
| --- | --- |
| PID | <code>98003</code> |
| Start | <code>2026-07-22T13:45:35Z</code> |
| End | <code>2026-07-22T13:47:55Z</code> |
| Exit | <code>0</code> |

User-observed checks passed:

- Cocoa window and complete main window;
- File → Open File and the <code>.raw</code> filter;
- successful <code>sparklers.raw</code> open with no crash or blocking dialog;
- non-empty changing event display and observable duration/position;
- pause and resume;
- forward seek at approximately 70–80% and backward seek at approximately
  20–30%;
- natural EOF, no observed crash at EOF, EOF state observation, and playback
  recovery after EOF; and
- close-button exit.

## 8. Run 2

| Field | Value |
| --- | --- |
| PID | <code>98667</code> |
| Start | <code>2026-07-22T13:53:07Z</code> |
| End | <code>2026-07-22T13:54:21Z</code> |
| Exit | <code>0</code> |

User-observed checks passed:

- second Cocoa launch;
- normal File → Open File;
- reopening the same <code>sparklers.raw</code>;
- non-empty event display and short playback; and
- Cmd+Q exit without crash, abort, or unresponsiveness.

The recent-file workflow was not tested.

## 9. Automated runtime evidence

The successful runs retained stdout/stderr, PID, start, end, and exit records
under <code>.logs/ebplus-macos-raw-terminal-20260722T133325Z</code>, with
isolated runtime output under
<code>.tmp/ebplus-macos-raw-terminal-20260722T133325Z</code>.

Both successful stderr logs record <code>libqcocoa.dylib</code> loaded and both
child exit codes are <code>0</code>. Neither contains a no-screens warning,
dynamic-loader fatal error, Qt platform fatal error, OpenGL fatal error,
SIGSEGV/abort marker, RAW decoder/runtime error marker, or camera/facility
marker. Each contains one nonfatal-looking IMK mach-port message near shutdown;
the retained exit codes remain <code>0</code>.

Normal EOF was established by user observation; the logs did not emit a
separate EOF marker.

## 10. Manual visual evidence

The event display was visibly non-empty and changed during playback.
Duration/position was observable, and the exercised controls produced visible
playback-state changes.

This does not establish event semantic correctness, timestamp correctness,
image quality, algorithm correctness, performance, or long-duration stability.

## 11. Observed non-blocking GUI defect

### Playback progress indicator circular handle is partially clipped

The circular handle of the playback progress indicator was observed to be
partially clipped during the successful RAW playback validation.

It did not block RAW open, display, pause/resume, seek, EOF recovery, reopen,
or clean exit. It prevents this record from being described as complete GUI
layout validation.

Deferred visual validation should cover initial window size, horizontal resize,
vertical resize, near-minimum window size, default display scaling, other
available macOS display scaling settings, available themes, and states before
playback, during playback, paused, and EOF.

Static candidates for deferred investigation only are
<code>gui/recorder/playback_controls.cpp</code> and
<code>gui/resources/theme/base.qss.in</code>. This record proposes no repair.

## 12. Filesystem output boundary

The isolated runtime produced:

~~~text
home/Library/Preferences/GUI-for-openEB/GUI for openEB/layout.json
~~~

Its existence is runtime-output evidence, not visual-layout correctness
evidence.

Complete filesystem write closure was not traced at the syscall level.

## 13. Evidence classification

| Evidence type | Boundary |
| --- | --- |
| Git/repository identity | Branch, commit, parents, tracked sample identity |
| Mach-O static evidence | arm64 executable and repository-local OpenEB provenance |
| Dependency provenance | repository-local HAL/HDF5 ECF directories and Cocoa plugin logs |
| Automated process evidence | retained PID, UTC timing, child exit code, stderr markers |
| User manual GUI evidence | visible RAW workflow facets listed in Runs 1 and 2 |
| Observed filesystem output | isolated runtime output only |
| Unverified inference | event semantics, timestamps, image quality, performance, complete layout, and broader parity |

## 14. Passed

- One tracked RAW sample opened in two normal Terminal.app/Aqua sessions.
- Non-empty changing event display, duration/position observation,
  pause/resume, forward/backward seek, natural EOF, and EOF recovery were
  observed in Run 1.
- A second normal open and short playback were observed in Run 2.
- Close-button and Cmd+Q exits both returned <code>0</code>.

## 15. Failed

No core RAW playback function failed in the two successful Terminal.app runs.

The partially clipped progress-indicator circular handle is a non-blocking
visual defect, not a core RAW playback failure.

## 16. Not run

- other RAW files, HDF5/H5/DAT, metadata edge cases, and file error cases;
- single-step, loop, speed/rate controls, recent files, file-to-file switch,
  and disconnect/reopen sequences;
- filters, algorithms, models, ONNX Runtime, and exports;
- camera/facility paths;
- installed executable and portable loader closure;
- Linux validation;
- performance, long-duration stability, and event
  semantic/timestamp/image-quality validation; and
- the full layout/scale/theme matrix.

## 17. Blocked

None for this narrow validation.

## 18. Remaining unverified scope

This record does not close the broader RAW playback parity milestone. Its
unverified facets remain subject to the existing roadmap completion criteria
and Linux comparison requirement.

Only algo/tests/sparklers.raw was opened and decoded.
No other RAW/HDF5/H5/DAT file was opened.
No camera was opened.
No facility was accessed or changed.
No algorithm or model was intentionally enabled.
No export was performed.
No installed executable was run.
No Linux validation was performed.
Complete filesystem write closure was not traced at the syscall level.
