# macOS Milestone 4A Validation

## 1. Status

- M4A-1: Passed
- M4A-2: Passed on rerun 1
- Milestone 4: Complete — Linux runtime gap explicitly accepted by maintainer

M4A records the macOS Apple Silicon idle-GUI portion of Milestone 4. M4A itself
does not provide native Linux validation. Milestone 4 was subsequently closed
by maintainer risk acceptance; Linux parity remains unverified.

## 2. Scope

Validated scope:

- the macOS Apple Silicon build-tree executable;
- Cocoa launch, main-window display, the five-menu idle surface, sidebar and
  idle panels;
- no-camera idle status and the idle 2D/OpenGL placeholder;
- horizontal and vertical resize;
- theme switching and persistence across an isolated rerun;
- normal close-button shutdown and Cmd+Q shutdown; and
- repository-local observed settings, layout, temporary and cache output with
  repository-local OpenEB 5.2 runtime plugin selection.

This scope excludes RAW, HDF5 GUI workflows, live camera, facilities,
algorithms, models, exports, installed-executable closure, portable dependency
closure, app bundles, packaging, signing and notarization.

## 3. Repository identity

The evidence was reviewed on branch feat/macos-gui-launch at commit
31ee991768ac157af6926267e22639f2c3364218. The retained M4A evidence is
ignored local evidence; it is not asserted to be distributed by Git.

## 4. M4A-1 implementation

The startup-environment adaptation is isolated in
gui/app/startup_environment.h and gui/app/startup_environment.cpp.
current_platform maps Apple to MacOS, Linux to Linux and all other targets to
Other.

compute_default_updates is a pure policy function over an explicit environment
map. On Linux it retains the previous four defaults only when their keys are
absent:

- MV_HAL_PLUGIN_PATH = /usr/local/lib/metavision/hal/plugins;
- HDF5_PLUGIN_PATH = /usr/local/lib/hdf5/plugin;
- QT_QPA_PLATFORM = xcb only when WAYLAND_DISPLAY exists and QT_QPA_PLATFORM
  does not; and
- QSG_RHI_BACKEND = opengl.

An existing key, including an empty string, is preserved. MacOS and Other add
none of these defaults. The applicator reads only the five policy keys and
applies only computed updates without overwriting existing values.

gui/main.cpp calls apply_defaults_for_current_platform before both
QSurfaceFormat setup and QApplication construction. The existing format
version, core profile, swap interval, application identity, MainWindow show,
and app-exit behavior remain in place. The helper is included in gui_core and
the production executable through gui/CMakeLists.txt; it is not Apple-only.

## 5. M4A-1 non-runtime validation

The retained M4A-1 evidence was reviewed without rerunning it:

- .logs/ebplus-macos-m4/m4a1-configure.log records successful configure with
  repository-local Metavision 5.2.0.
- .logs/ebplus-macos-m4/m4a1-build.log records the incremental production and
  startup-policy build.
- .logs/ebplus-macos-m4/m4a1-ctest-list.log records 10
  gtest_discover_tests registrations and 308 discovered tests.
- .logs/ebplus-macos-m4/m4a1-ctest-focused.log records 13/13 focused startup
  policy tests passing.
- .logs/ebplus-macos-m4/m4a1-ctest-full.log records 308/308 tests passing.

The retained preflight and Mach-O evidence records a non-fat arm64
gui_for_openeb executable, Metavision dylib version 5.2.0, and no
/usr/local OpenEB 5.1.1 provenance. These are build-tree/static facts, not
portable-application evidence.

## 6. Linux source-path preservation

The explicit Platform::Linux tests execute Linux policy cases by passing
Platform::Linux to the pure function; they do not depend on the macOS compiler
target selecting Linux. They cover absent defaults, pre-existing HAL, HDF5,
QPA and RHI values, missing and empty WAYLAND_DISPLAY behavior, empty-string
semantics, the allow-list, macOS, Other and the current compiler-target map.

This provides source-static evidence and macOS-executed pure-policy test
evidence that the Linux policy was retained. It is not Linux compilation,
Linux applicator runtime or Linux GUI runtime evidence.

## 7. M4A-2 runtime protocol

The rerun used the existing build-tree executable under env -i. HOME,
CFFIXED_USER_HOME, TMPDIR and XDG configuration, cache, data and state roots
were redirected to .tmp/ebplus-m4a2-rerun1. The process environment set Cocoa,
repository-local MV_HAL_PLUGIN_PATH and HDF5_PLUGIN_PATH, and Qt plugin
debugging only. It did not supply WAYLAND_DISPLAY, QSG_RHI_BACKEND,
QT_PLUGIN_PATH, LD_LIBRARY_PATH or DYLD variables.

Each run used a wrapper that recorded the actual GUI child PID and waited for
that child before recording the child exit code. A watchdog guarded the
interactive window period. The child PID and exit files are:

- .tmp/ebplus-m4a2-rerun1/run1.pid = 75698;
- .tmp/ebplus-m4a2-rerun1/run1.exit = 0;
- .tmp/ebplus-m4a2-rerun1/run2.pid = 76156; and
- .tmp/ebplus-m4a2-rerun1/run2.exit = 0.

The lsof captures identify wrapper PIDs 75696 for Run 1 and 76154 for Run 2.
The rerun protocol recorded wrapper exit code 0 for both runs, matching the
actual GUI child exit files. No run1.timeout or run2.timeout file was created.

## 8. Initial blocked attempt

The initial M4A-2 attempt remains Blocked. Its only blocking reasons were:

- the manual checklist was incomplete;
- resize was not performed; and
- the actual GUI exit code was not reliably retained.

It was not blocked because of a Cocoa, loader or OpenGL failure. Limited
retained evidence shows a Cocoa GUI launch, actual libqcocoa.dylib loading,
pink selection followed by a normal close, and repository-local layout.json
output. Those observations do not make the initial attempt a passing
validation.

## 9. Successful rerun

M4A-2 rerun 1 met the rerun exit and watchdog gates. Run 1 used GUI PID 75698
with wrapper PID 75696; Run 2 used GUI PID 76156 with wrapper PID 76154.
Both child exit files contain 0, the recorded wrapper exit codes were 0, and
neither watchdog triggered.

The rerun is the passing M4A-2 result. The user-observed checks below remain
separate evidence from the process, plugin and filesystem records.

## 10. Run 1 result

Actual Cocoa GUI runtime evidence:

- the GUI child remained alive for the required initial observation period;
- its recorded exit code was 0;
- the close-button shutdown was used; and
- no watchdog file was created.

User-observed visual evidence:

- Run 1 checklist items 1 through 16 were all PASS;
- a Cocoa window appeared within 30 seconds and no additional XQuartz, X11,
  XCB or Wayland window appeared;
- the main window was complete, with File, View, Theme, Tools and Help menus,
  no Camera menu, a sidebar, idle panels, no-camera status and an idle
  placeholder;
- the window was resized both horizontally and vertically, and the menus,
  sidebar, panels and placeholder relaid out normally;
- menus were opened only for observation, without opening files, cameras,
  facilities, models, algorithms or export workflows;
- pink was selected and visually applied; and
- the normal window close button was used.

## 11. Run 2 result

Actual Cocoa GUI runtime evidence:

- the Run 2 child exit file contains 0;
- the wrapper recorded 0;
- Cmd+Q was used; and
- no watchdog file was created.

User-observed visual evidence:

- Run 2 checklist items 1 through 9 were all PASS;
- a Cocoa window appeared within 30 seconds;
- pink was restored automatically without reselecting it;
- the main window, menus, sidebar, panels and placeholder were normal;
- no additional XQuartz, X11, XCB or Wayland window and no abnormal modal
  dialog appeared;
- no RAW, camera, algorithm, model or export workflow was used;
- the original theme was restored and visually confirmed; and
- Cmd+Q performed the normal exit.

## 12. Cocoa and loader evidence

Both .logs/ebplus-macos-m4/m4a2r1-run1.log and
.logs/ebplus-macos-m4/m4a2r1-run2.log record libqcocoa.dylib as a loaded
library. The corresponding lsof captures also map libqcocoa.dylib. This is Qt
plugin runtime evidence, distinct from the user observation that a window was
visible.

Neither run has evidence of actual libqxcb or Wayland QPA backend loading, a
Qt platform-plugin fatal failure, dynamic-loader failure, symbol failure,
crash, abort or uncaught exception. Transitive libxcb or libX11 entries in
lsof are not interpreted as Qt using an XCB QPA backend. The captures show
repository-local OpenEB 5.2 dylibs and HAL plugins and no OpenEB 5.1.1
/usr/local provenance.

## 13. OpenGL evidence

AppleMetalOpenGLRenderer and its metallib resources were mapped in each rerun
lsof capture. The idle placeholder passed user visual inspection.

AppleMetalOpenGLRenderer mapping is runtime-library evidence, and the
placeholder result is user-observed visual evidence. Event-data rendering
correctness and performance were not tested.

## 14. Settings and runtime isolation

Run 1's retained runtime-tree record reports 12 KiB and Run 2's reports
20 KiB under .tmp/ebplus-m4a2-rerun1. The observed application settings/layout
path is:

.tmp/ebplus-m4a2-rerun1/home/Library/Preferences/GUI-for-openEB/GUI for openEB/layout.json

No plist file was observed. The tree also contains the injected home, tmp and
XDG roots plus the run PID and exit control files. layout.json records
geometry/state; its existence was not used to infer theme persistence. Theme
persistence was confirmed by user-observed visual evidence.

All observed application-controlled runtime files were contained in the
repository-local runtime root.

Complete filesystem write closure was not traced at the syscall level.

## 15. Camera boundary

The maintainer confirmed that CenturyArks, Prophesee and other supported event
cameras were physically disconnected before the runs. The application still
executed SDK device enumeration at startup. No camera was opened or started,
and no facility was accessed or modified.

The retained USB snapshot log paths are present but zero bytes, so the
physical-disconnection condition is recorded from the maintainer confirmation,
not inferred from those empty files.

No claim is made that launch avoided all SDK hardware enumeration.

## 16. Disk impact

At the M4A-3 documentation audit, retained M4A evidence occupied 268 KiB in
.logs/ebplus-macos-m4, 4 KiB in .tmp/ebplus-m4a2-runtime and 20 KiB in
.tmp/ebplus-m4a2-rerun1. These are observed retained sizes, not a reconstructed
per-run growth calculation. No cleanup was performed.

The M4A-3 document-only update was expected to add less than 1 MiB and creates
no build tree, dependency prefix, runtime root or artifact.

## 17. Warnings and observations

Run 1 recorded an IMKCFRunLoopWakeUpReliable mach-port message. Run 2 recorded
a Window move completed without beginning warning. Both observations were
non-fatal, did not affect an exit code, and did not invalidate the
user-observed checklist. They are not evidence that all macOS versions are
unaffected.

## 18. Passed

- M4A-1 startup-environment isolation implementation, configure/build evidence
  and focused/full macOS CTest evidence passed.
- M4A-2 rerun 1 passed the actual child exit-code, watchdog, Cocoa plugin,
  no-fatal-loader/context-record and isolated-runtime-output gates.
- The user-observed idle-window, resize, pink theme persistence and normal
  shutdown checks passed.

## 19. Failed

No in-scope M4A-2 rerun 1 gate failed. The separate initial M4A-2 attempt is
retained as Blocked, not reclassified as Failed.

## 20. Not run

- Native Linux compilation was not run.
- Linux XCB/Wayland GUI runtime regression was not run.
- RAW playback and HDF5 GUI workflows;
- live camera, camera open/start, facilities and physical
  disconnect/reconnect;
- algorithms, event-data OpenGL rendering, models, ONNX loading/inference and
  exports;
- installed-executable runtime, portable dependency closure, app bundle,
  packaging, code signing and notarization.

## 21. Maintainer risk acceptance and Milestone 4 closure

- Decision date: 2026-07-22
- Decision owner: repository maintainer
- M4A-1: Passed
- M4A-2: Passed on rerun 1
- Linux compilation: Not run
- Linux GUI runtime regression: Not run
- Residual Linux risk: Accepted by maintainer
- Milestone 4: Complete by maintainer decision

macOS Apple Silicon idle Cocoa launch, interaction, resize, theme persistence
and clean shutdown passed. Native Linux compilation was not run, and Linux
XCB/Wayland GUI launch regression was not run.

Source-static review and macOS-executed pure-policy tests indicate that the
pre-existing Linux environment defaults were preserved, but this is not native
Linux build or runtime evidence. The maintainer explicitly accepts the
remaining Linux regression risk and closes Milestone 4 without Linux runtime
validation. Linux parity remains unverified.

The accepted residual risk includes:

- potential Linux compiler or system-header differences;
- the Linux setenv/applicator path not having been executed natively;
- unverified Qt XCB/Wayland application launch;
- unverified Linux OpenGL and window-system behavior; and
- Linux startup plugin-path defaults covered only by source review and
  pure-policy tests.

Risk acceptance is not validation evidence. It does not change any Linux
parity-matrix row to Verified. If a Linux environment becomes available or a
Linux regression is reported, the relevant work can be reopened.

## 22. Remaining verification beyond M4 closure

M4B, the Linux XCB/Wayland GUI launch regression, was not run and was
explicitly waived only for Milestone 4 closure by maintainer risk acceptance.
It remains an unverified future regression check. M4A is not a substitute for
later RAW, camera/facility, algorithm/model/export or portable-bundle
milestones.

## 23. Evidence classification

| Classification | Evidence and boundary |
| --- | --- |
| Source static evidence | Startup-environment source, main call placement, CMake wiring and explicit Linux policy review. |
| M4A-1 configure/build evidence | m4a1-configure.log and m4a1-build.log; no configure/build was rerun for this report. |
| M4A-1 CTest evidence | 13/13 focused startup-policy tests and 308/308 full CTest tests from retained logs. |
| Mach-O static evidence | Non-fat arm64 build-tree executable, Metavision 5.2.0 provenance and no OpenEB 5.1.1 static provenance; this does not prove portable closure. |
| Qt plugin runtime evidence | Both rerun logs and lsof captures record actual libqcocoa.dylib loading. |
| Actual Cocoa GUI runtime evidence | Two build-tree GUI child processes remained available for interaction, exited 0 and did not trigger their watchdogs. |
| User-observed visual evidence | Checklist results for window appearance, menus, sidebar, placeholder, resize, pink persistence and normal shutdown. |
| Observed filesystem output | The retained isolated runtime trees and layout.json path; no plist was observed. |
| Unverified inference | No syscall-level write closure, event-data rendering correctness, portable dependency closure or Linux behavior is inferred from M4A. Maintainer risk acceptance is a closure decision, not Linux validation evidence. |
| Not run | Linux GUI regression and every workflow listed in section 20. |
