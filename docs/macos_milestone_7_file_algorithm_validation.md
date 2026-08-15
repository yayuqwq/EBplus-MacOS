# macOS Milestone 7 Slice 2 File-Source Algorithm Validation

## Status and scope

**Status:** Complete / Qualified for M7 Slice 2 only. Milestone 7 overall
remains **In progress**.

This report records deterministic file-source preprocessing and representative
algorithm qualification on macOS Apple Silicon. It combines:

- Slice 2A automated/file-source evidence on the tracked RAW fixture;
- Slice 2B bounded no-camera Cocoa GUI wiring and lifecycle evidence; and
- the later deterministic loop_flip test-only repair and full CTest
  requalification.

Validation was performed on the uncommitted
feat/macos-file-algorithm-qualification worktree above base
404a6e0290c424ee4809dd19b2f674af2ed264e6, not on bare HEAD alone. The
working-tree changes are test-only; no production algorithm or GUI behavior was
changed for this slice.

This is not a claim that all M7 work, all 26 self-developed algorithms, all
parameter combinations, physical-camera behavior, packaging, or Linux runtime
is complete.

## Identity and provenance

| Item | Value |
| --- | --- |
| Branch / committed base | feat/macos-file-algorithm-qualification / 404a6e0290c424ee4809dd19b2f674af2ed264e6 |
| Worktree implementation | algo/tests/CMakeLists.txt, algo/tests/test_raw_algos.cpp, algo/tests/test_phase8_10.cpp, algo/tests/test_loop_flip.cpp, and untracked algo/tests/test_filter_chain_semantics.cpp |
| Production diff relative to base | None in algo/ outside algo/tests/, or in gui/ |
| Build tree | .build/ebplus-macos, Release, arm64 |
| OpenEB profile | repository-local OpenEB / Metavision SDK 5.2 CenturyArks profile |
| ECF | b982d908a0bc0afd9104d226607bedb1a11b2a95, clean detached submodule at audit time |
| GUI binary | .build/ebplus-macos/gui/gui_for_openeb, arm64, UUID B05C69DC-3B29-3DBA-B942-549B28D63282 |
| Linkage boundary | no /usr/local OpenEB 5.1.1, x86_64, or OpenEB producer build-tree dylib observed; Metavision resolves through the repository-local 5.2 prefix |

The GUI binary is a build-tree binary. Its RPATH/linkage evidence does not
establish a standalone loader, .app, packaging, signing, or notarization
closure.

## Fixed fixture

The sole Slice 2 real recording is tracked algo/tests/sparklers.raw:

| Property | Value |
| --- | --- |
| File type | tracked regular file |
| Size | 2,109,142 bytes |
| SHA-256 | e84afbecdc07d2910ae846a4ae0ee246f5b9c97a53816c637d4f85c023d7c234 |
| Encoding | EVT2 |
| Geometry | 640x480 |
| CD events | 521,252 |
| Shifted timestamps | 0..95,871 us |

No additional recording was generated, copied, or used for this slice.

## Automated evidence

### Shared preprocessing and KNoise

The current shared preprocessing roster is:

~~~text
BAF
STCF
Refractory
DWF
AgePolarity
Harmonic
Repetitious
SpatialBP
KNoise
~~~

test_raw_algos.cpp retains real-RAW sane-rate checks for all nine modes. The
Slice 2 addition replays a fixed 8,192-event prefix after reset for each mode,
requires the replay output to match exactly, and checks every kept coordinate
and timestamp for validity. This is deterministic fixed-input reset/replay
evidence, not an all-parameter or all-algorithm-interaction claim.

KNoise evidence is intentionally more specific:

- default dt = 3000 us contract;
- real-RAW keep-rate evidence on the tracked fixture;
- synthetic correlated-event pass behavior;
- synthetic isolated-event rejection; and
- synthetic polarity behavior.

The GUI session proves selection and parameter-host wiring, not human numerical
correctness of any of the nine filters.

### FilterChain

The current OpenEB FilterChain roster is:

~~~text
polarity_filter
polarity_invert
flip_x
flip_y
rotate
transpose
rescale
~~~

test_filter_chain_semantics.cpp contains five test cases covering all seven
stages with a 7x5 non-square synthetic geometry. The applicable assertions
cover event count, polarity, exact coordinates, swapped geometry/bounds for
transpose and orthogonal rotation, and OpenEB rescale semantics. The non-square
input makes width/height mistakes observable.

The new test target compiles its test source and links the existing gui_core
target. It does not recompile gui/algo_bridge/filter_chain.cpp under a
test-specific warning policy.

#### Test-build and test-input incidents

The initial target instead directly recompiled filter_chain.cpp while its test
target applied -Werror. That exposed unused parse<bool> and parse<std::string>
specializations. This was a test-build integration problem, not a production
FilterChain runtime failure. Linking the existing gui_core target resolved the
integration issue without changing production code.

The first exact-semantics input also assumed the wrong positional EventCD
ordering. The OpenEB constructor order used by the final test is (x, y, p, t);
the test now uses an explicit helper to avoid {x,y,t,p} ambiguity. This was a
test-input adaptation, not a production behavior repair.

Real-RAW FilterChain evidence additionally includes existing filter_in_render
and filter_integration coverage plus the repaired loop lifecycle test below.

### Arc, Time Surface, and representative algorithms

| Area | Automated evidence on the current worktree |
| --- | --- |
| Corner Detector Arc | synthetic geometry and stale-state tests; real-RAW Arc output is non-empty, finite, in bounds, has valid strength, and has bounded corner count |
| Time Surface | Linear and Exponential render evidence; Merged and Split channel behavior; reset/replay; palette mapping contract; refresh-interval parameter contract; real-RAW Linear/Exponential range evidence |
| Representative CV | real-RAW Blob Detector, ObjectTracker, Corner/Arc, and OpticalGyro checks retain bounded finite/in-bounds or valid-output evidence |
| Representative analytics | real-RAW ActiveMarker, ISI Analyzer, Particle Counter, and Frequency Detector checks retain representative finite/consistent analysis evidence |
| Representative stateful behavior | NoiseFilter reset/replay, Time Surface reset/replay, and ObjectTracker real-RAW evidence |

Arc is only one Corner Detector mode. Time Surface refresh evidence is a
parameter/interval contract; it is not a measurement that the application
achieves exact 30, 60, or other Hz presentation timing.

The current AlgoBridge mapping exposes Corner Detector mode 3 as Arc and maps
that selection to CornerDetector::Mode::Arc. The GUI observation therefore
checks the current Arc entry point, while the automated Arc assertions remain
the numerical/geometry authority.

### Earlier focused record and final machine authority

The prior Slice 2A working-session record reported 83/83 focused tests
passing. Closure audit did not find a retained raw machine log for that exact
focused selection, so it is preserved as historical execution context rather
than the final machine-verifiable authority.

The retained machine-verifiable final sequence is:

| Check | Result | Retained evidence |
| --- | --- | --- |
| Initial full build | Passed before the later loop_flip test-only repair | .logs/m7-slice2-final-20260814T142216Z/full-build.log |
| Initial full CTest | 374/375 passed, 1 failed, 0 timeout, 82.83 s | .logs/m7-slice2-final-20260814T142216Z/full-ctest.log |
| Repaired test_loop_flip incremental build | Passed; target-only rebuild | .logs/m7-slice2-loop-flip-repair-20260814T153642Z/build.log |
| Deterministic target | initial run plus five repeats: 6/6 passed | .logs/m7-slice2-loop-flip-repair-20260814T153642Z/targeted/ and repeats/ |
| Related focused regression | 24/24 passed, 0 failed, 0 timeout, 16.57 s | .logs/m7-slice2-loop-flip-repair-20260814T153642Z/focused/ctest.log |
| Fresh full CTest | 375/375 passed, 0 failed, 0 timeout, 78.51 s | .logs/m7-slice2-loop-flip-repair-20260814T153642Z/full-ctest/ctest.log and fresh LastTest.log |

The post-repair build evidence is intentionally described as an incremental
test-target build, not as a new whole-tree build. Production source was
unchanged, and the fresh full CTest is the exact post-repair test authority.

## loop_flip failure, diagnosis, and deterministic repair

The initial full CTest's only failure was loop_flip. The old test sampled
asynchronous QImage pixels after wall-clock waits of roughly 200 ms, 200 ms,
and 3 s; it discarded the frame_ready timestamp and retained only the last
frame. It therefore compared colors at the same coordinate across unfixed
source windows rather than comparing the same input events.

The retained failure recorded:

~~~text
initial pixel:  (67,0)=FF407EC8
mirror target:  (572,0)
first flip target: FF407EC8
after observed 62 loops: target=FF1E2534
~~~

The diagnosis concluded **B. test assertion/timing-window design problem**:
62 was an observed scheduler/cadence result, not a product contract, and the
five isolated old-test reruns passed while tracking pixel (639,1), rather
than the failed sample at (67,0). Because the old test discarded timestamps,
its source windows were not recorded. No production regression evidence was
established.

The repaired test uses:

~~~text
events_window_ready(shared_ptr<vector<EventCD>>, timestamp)
~~~

In the current implementation, the timestamp is the start of the source event
window [ts, ts + accumulation_us). The test captures the unflipped [0,33000)
baseline via gen.seek(0), enables flip_x, and captures the same window again.
It then waits for a real looped() signal and accepts only the first post-loop
events_window_ready(..., ts == 0) window.

For this 640-pixel-wide fixture, the test verifies every comparable event:

~~~text
x' = 639 - x
y, p, t unchanged
event count and order unchanged
flip_x still enabled after the loop
~~~

The initial deterministic run exited 0 in about 3.16 s, observed one real
loop, and captured [0,33000) with 230,117 events before and after the loop.
All five repeats also exited 0, each observed one loop and the same window and
event count. The old timing-sensitive QImage assertion was replaced by a
deterministic same-event-window lifecycle invariant. Production behavior
required no repair.

The successful full CTest LastTest.log has mtime 2026-08-14T23:50:10+0800.
LastTestsFailed.log still contains the earlier 375:loop_flip entry with mtime
2026-08-14T22:33:32+0800; it is a retained stale artifact from the failed run,
not the fresh full CTest result, and was not deleted.

## Cocoa GUI evidence

Slice 2B used one tracked RAW fixture in bounded no-camera Cocoa sessions.
The control outcomes below are maintainer human observations; they are not
automated numerical assertions.

| Run | Evidence | Result |
| --- | --- | --- |
| Functional run | .logs/m7-file-algo-gui-20260814-161324/, matching .tmp/ and .artifacts/ roots; outer wrapper PID 75446, session wrapper PID 75873, GUI PID 75874 recovered from stderr | planned functional workflow observed as passed, but zsh:14: read-only variable: status prevented trustworthy machine process-exit capture |
| Evidence repeat | .logs/m7-file-algo-gui-20260814-173643/, matching .tmp/ and .artifacts/ roots; session wrapper PID 21574, GUI PID 21575 | same-scope human workflow repeated as passed; .tmp/.../gui.exit-code records 0 |

The second run did not add new algorithm or numerical coverage. It adds fresh
same-scope human observation, a machine-verifiable clean process exit, and a
fatal-marker scan. The scan found no SIGSEGV, SIGABRT, QScreen, no screens, dyld
fatal, or device-error marker. Both stderr logs contain nonfatal font fallback
and IMK mach-port messages; the second run also records a window-move warning.
The absence of scoped fatal markers must not be rewritten as an absence of all
warnings.

### Actual bounded workflow

| Feature | Maintainer observation |
| --- | --- |
| File source | sparklers.raw opened as File playback at 640 x 480; playback was usable |
| FilterChain | Flip X enable/disable changed presentation and recovered the baseline after seek/Step redraw |
| Shared preprocessing | all nine modes were selectable; stale parameter controls did not remain; KNoise showed dt = 3000 us |
| Corner Detector | enabled with Arc mode (3); observed Arc settings were 5000 and 1; disable/re-enable did not leave a stale overlay or crash |
| Time Surface state A | Linear, decay time 100000, Hot, Merged, 30 Hz; Time Surface dock showed updating output |
| Time Surface state B | Exponential, tau 100000, Plasma, Split, 60 Hz; parameter host changed from decay time to tau and output recovered |
| Stateful playback | Pause, forward seek, backward seek, and resume updated the display; Time Surface disable/re-enable created no duplicate or stale presentation |
| Analytics representative | Particle Counter with Line Y=-1 and Min area=10; enable/disable/re-enable worked, with count 0 accepted by the human checklist |
| Shutdown | Cmd+Q closed normally; the evidence-repeat process exit was 0 |

## Evidence taxonomy and limits

The following evidence types remain distinct:

- automated numerical and lifecycle evidence validates the fixed fixture,
  synthetic semantics, reset/replay, and exact event assertions described
  above;
- GUI wiring/lifecycle evidence validates selected controls, enable/disable,
  mode switching, playback lifecycle, presentation responsiveness, and clean
  process shutdown; and
- maintainer human observation records what was visibly observed in the bounded
  Cocoa session.

For example, GUI Flip X observation is not numerical GUI validation of all
seven transforms. Their exact semantics come from the non-square automated
tests. Nine selectable preprocessing modes and a visible KNoise parameter do
not constitute human numerical validation; KNoise numerical behavior comes from
the automated tests. Similarly, GUI Time Surface mode/lifecycle evidence and
automated mode/reset/range evidence can be combined only while preserving their
different meanings. Particle Counter is one representative analytics GUI
lifecycle, not analytics-category completion.

This Slice 2 closure supports:

- deterministic tracked-RAW preprocessing evidence for all current nine shared
  noise modes;
- KNoise representative automated behavior;
- all current seven FilterChain exact synthetic semantics and representative
  real-RAW lifecycle;
- Arc synthetic plus real-RAW evidence;
- Time Surface Linear, Exponential, Merged, Split, reset, palette/refresh
  parameter contracts, and representative real-RAW evidence;
- representative CV, analytics, and stateful automated evidence; and
- representative Cocoa file-source GUI wiring/lifecycle for Flip X,
  preprocessing/KNoise, Corner Detector Arc, Time Surface, Particle Counter,
  playback seek, and clean shutdown.

It does not support claims for:

- all 26 self-developed algorithm GUI runtime or numerical correctness;
- all parameter cross-products, all seven transforms manually validated in the
  GUI, or all nine noise modes manually validated numerically;
- measured exact Time Surface refresh timing, long-duration stability,
  performance, or stress behavior;
- physical camera, M6, ONNX/model-backed E2VID, CSV/RAW clip/AVI export,
  calibration, processed recording, packaging, standalone loader closure, or
  Linux runtime.

test_filter_in_render.cpp retains historical EventCD{x,y,t,p} aggregate
initialization. It passed the fresh full CTest but remains an observed test-code
consistency limitation. It shares no helper, state, or process with loop_flip,
and was not changed in this slice.

**Linux remains Not run / unverified.** No new Linux residual-risk acceptance
is made here.

M6 remains **Planned / Paused — physical CenturyArks camera currently
unavailable**. This file-source evidence does not substitute for a device,
facility, live-stream, recording, or reconnect qualification.

## Closure result

M7 Slice 2 documentation closure preparation is complete. The qualification
records a test-only working-tree change set and does not authorize staging,
commit, push, pull request, merge, or work on another slice. At the time this
validation report was prepared, the next recommended gate after maintainer
review was a final read-only pre-commit audit.
