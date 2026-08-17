# macOS Milestone 7 Slice 4A Offline Export Validation

## Status and scope

**Status:** Complete / Qualified for M7 Slice 4A only. M7 Slice 4 and
Milestone 7 overall remain **In progress**. AVI is a separate pending Slice
4B.

This report qualifies bounded macOS Apple Silicon CSV source-event export and
RAW clip export on one tracked RAW fixture. It combines deterministic automated
contracts, one bounded Cocoa File Tools session, a current-worktree full build,
and a fresh full CTest run.

Validation was performed on the uncommitted
`feat/macos-offline-export-qualification` worktree above committed base
`22198f42837f5b5c73f9516bc7430cada2bb994d`. The base SHA identifies the
committed parent only; it does not claim that the test additions are in bare
`HEAD`.

The qualified scope is limited to `FileConverter` CSV and RAW clip behavior.
It does not qualify AVI/H264/MJPG, live recording, general algorithm-result
export, all source formats, physical camera workflows, packaging, or Linux.

## Identity and provenance

| Item | Value |
| --- | --- |
| Branch / committed base | `feat/macos-offline-export-qualification` / `22198f42837f5b5c73f9516bc7430cada2bb994d` |
| Validated worktree diff | `gui/tests/CMakeLists.txt` plus untracked `gui/tests/test_file_converter.cpp` |
| Production diff relative to base | None in `gui/app/`, `gui/panels/`, `gui/exporter/`, or `gui/main_window.*` |
| Build tree | `.build/ebplus-macos`, existing Release/arm64 tree reused without cleaning or creating a second tree |
| OpenEB profile | repository-local OpenEB / Metavision SDK 5.2 CenturyArks profile |
| ECF | `b982d908a0bc0afd9104d226607bedb1a11b2a95` |
| GUI binary after final build | arm64, UUID `A8A45BF1-F3B8-3965-8DE3-724C14F2AA4E`, 2,084,576 bytes, mtime `2026-08-17T14:21:23+0800` |
| GUI linkage | repository-local OpenEB 5.2 RPATH; no `/usr/local` OpenEB 5.1.1, x86_64, OpenEB producer build-tree dylib, or `libonnxruntime` observed |

The build-tree executable is not a standalone loader, `.app`, packaging,
signing, or notarization closure. Its Metavision dylibs resolve through
`@rpath`; the relevant RPATH is the repository-local OpenEB 5.2 prefix. The
additional `/opt/homebrew/lib` RPATH is retained build provenance and is not an
OpenEB 5.1.1 fallback.

## Fixed fixture and source-event oracle

The sole real recording used by this slice is the tracked regular file
`algo/tests/sparklers.raw`:

| Property | Value |
| --- | --- |
| Size | 2,109,142 bytes |
| SHA-256 | `e84afbecdc07d2910ae846a4ae0ee246f5b9c97a53816c637d4f85c023d7c234` |
| Encoding / geometry | EVT2 / 640x480 |
| CD events | 521,252 |
| Shifted timestamps | 0..95,871 us |

The source oracle is decoded with FileConverter-equivalent OpenEB hints:
`real_time_playback(false)` and the default `time_shift=true`. CSV rows are
compared against that complete shifted source-event sequence. RAW clip payload
readback explicitly uses `time_shift(false)`, avoiding a secondary reader
presentation shift before comparison with the intended shifted export timeline.
This is reader handling, not writer timestamp rewriting.

## Deterministic automated qualification

`test_file_converter` links the existing `gui_core` target and registers five
tests through `gtest_discover_tests`; it does not rebuild a production
translation unit under a test-only policy.

| Contract | Retained result |
| --- | --- |
| CSV exact export | `FileConverter.CsvExportsEverySourceCdEventExactly` requires a regular nonzero CSV, `completed` without failure, exact header `t,x,y,p`, and a full row-by-row `t/x/y/p` and ordering comparison against all 521,252 source CD events |
| CSV error | `FileConverter.CsvMissingParentFailsWithoutCompleted` requires `failed("Cannot open CSV output file.")`, no `completed`, and no output when the dedicated output parent is absent |
| RAW nonzero interval | `FileConverter.RawCutPreservesExactInclusiveInterval` selected `[15,480, 66,235] us`; expected and read-back counts were 260,635; full `t/x/y/p` and order matched, every event satisfied both inclusive bounds, and output was 640x480 EVT2 |
| RAW zero-start interval | `FileConverter.RawCutStartAtZeroPreservesExactInterval` selected `[0, 41,454] us`; expected and read-back counts were 260,638; full payload/order comparison passed |
| RAW error | `FileConverter.RawCutMissingParentFailsWithoutCompleted` requires failed, no completed, and no output for an absent dedicated output parent |

The retained direct gate reported 2/2 passed. The retained focused set reported
9/9 passed, 0 failed, 0 timeout, in 0.90 s: the five FileConverter contracts,
the retained `ExporterController.Hdf5ExportPreservesGeometry` metadata/geometry
safety regression, and three CameraController file-lifecycle regressions.

The CSV output was 8,023,081 bytes, or 3.803955x the input RAW size for this
fixture. That ratio is fixture-specific, not a general CSV size claim.

The HDF5 retained regression is only metadata/geometry safety evidence. It is
not FileConverter HDF5 full event-content qualification and does not expand
the Slice 4A scope.

No cancellation test was added: there is no deterministic public seam proving
that a conversion is active, and this fixture completes too quickly for a
timing race. No legal tracked no-seek source was available. Therefore CSV
cancellation, RAW cancellation, and RAW no-seek-source fallback remain
**Not run**. The bounded decode polling in the oracle is not a cancellation
race test.

## Bounded Cocoa File Tools evidence

One Cocoa session used fresh repository-local roots:

| Item | Value |
| --- | --- |
| Runtime roots | `.tmp/m7-slice4a-gui-20260817T112926+0800/`, `.logs/m7-slice4a-gui-20260817T112926+0800/`, `.artifacts/m7-slice4a-gui-20260817T112926+0800/` |
| Wrapper / GUI PID | `26792` / `26794` |
| Exit | `0`; no residual `gui_for_openeb` process observed |
| stdout | 0 bytes |
| stderr limitations | font fallback, IMK/XPC, window-move, and CapsLock diagnostics only |
| Fatal-marker scan | no `SIGSEGV`, `SIGABRT`, `QScreen`, `no screens`, fatal, dyld, or device-error marker |

The human workflow opened `sparklers.raw`, selected Settings / Tools / File
Tools, used `Convert to CSV`, then used `File Cutter` with integer microsecond
inputs. The valid GUI cut interval was `20,000 us` through `70,000 us`
(`0.02 s` through `0.07 s`). The session then reopened the generated clip and
ended with Cmd+Q.

Fresh session outputs were retained under the Slice 4A artifact root:

| Output | Result |
| --- | --- |
| `sparklers-gui.csv` | regular, nonzero, 8,023,081 bytes, 521,253 lines including the header |
| `sparklers-gui-clip.raw` | regular, nonzero, 949,642 bytes; repository-local readback reports EVT2, 640x480, 234,248 CD events, stored timestamps 20,000..70,000 us |
| `sparklers-gui-clip.raw.tmp_index` | regular 851-byte indexing sidecar generated during the same session |

The sidecar supports that GUI reopen/indexing occurred. Together with the
maintainer observation of an opened clip, active playback, and no fatal/error
dialog, this is application-level RAW round-trip evidence. It is not a claim
of complete playback numerical correctness; exact event interval semantics are
the automated layer's authority.

The GUI evidence proves Cocoa File Tools/dialog wiring, background operation
lifecycle, success presentation, actual output creation, generated-clip
reopen/indexing, and clean process exit. CSV row/event semantic correctness,
full ordering, and RAW payload bounds remain automated evidence; a successful
dialog must not be treated as their substitute.

An invalid-range interaction was not retained in an independent screenshot or
machine log. It is not claimed as independently qualified evidence. The direct
FileConverter API also has no separately defined `start >= end` contract;
current GUI validation owns that input check.

The screenshot/status text containing a historical `m7-e2vid-gui-...` Done
path is not Slice 4A export evidence. Only the fresh `m7-slice4a-gui-...`
artifact root above is authoritative for this slice.

## Modeless dialog layout behavior

`Convert to CSV` opens a modeless `FileOpDialog` through `show()`. It is a
separate dialog window rather than an embedded control in the left File Tools
layout, and is not currently bound to main-window/sidebar move or resize
events. It does not automatically reposition when that layout changes.

This is current UI/layout behavior, not a CSV correctness or export lifecycle
failure, and it is not a Slice 4A blocker. A future change to dialog anchoring
or repositioning requires a separate UI scope. The File Tools panel itself is
inside the Settings sidebar's `QScrollArea`; the current screenshots do not
establish a deterministic trigger for any possible narrow-sidebar button-text
clipping issue.

## Final whole-tree qualification

| Check | Result |
| --- | --- |
| Full incremental build | `cmake --build .build/ebplus-macos --parallel 4` passed using the existing tree |
| Build warning | one existing `algo/tests/test_raw_algos.cpp:346` `finite_vectors` unused-variable warning; outside the Slice 4A diff and not changed |
| Full CTest discovery | 383 tests |
| Full CTest | `ctest --test-dir .build/ebplus-macos --output-on-failure --parallel 4`: 383/383 passed, 0 failed, 0 timeout, 22.21 s |
| Fresh machine record | `.build/ebplus-macos/Testing/Temporary/LastTest.log`, mtime `2026-08-17T14:22:45+0800` |

The full CTest includes all five new FileConverter registrations. No second GUI
session, AVI workflow, model/runtime installation, or physical-camera action
occurred during this final build/CTest/documentation gate.

## Evidence boundary and remaining work

This Slice 4A closure supports:

- tracked RAW to full source-event CSV export with exact `t/x/y/p` and order;
- deterministic CSV missing-parent error handling;
- deterministic zero-start and nonzero-start RAW clip payload, order, bounds,
  geometry, EVT2 encoding, and readback contracts;
- one bounded macOS Cocoa CSV/RAW File Tools workflow, generated output,
  RAW reopen/indexing, and clean exit; and
- current-worktree full build and full CTest qualification.

It does not support claims for cancellation, all filesystem failures,
disk-full, huge-file performance, all input source formats, no-seek source
fallback, AVI/H264/MJPG, live recording, general algorithm-result export,
physical camera/M6, packaging, or Linux export parity.

**Linux remains Not run / unverified.** No Linux risk acceptance is added by
this macOS-only evidence. M6 remains **Planned / Paused — physical
CenturyArks camera currently unavailable**.

## Workspace and closure result

The pre-build disk audit recorded 228.274 GiB total, 41.125 GiB available, and
a 34.241 GiB protection line. The existing build tree was reused; no second
build/dependency tree was created and no historical evidence was removed.
After full build and CTest, available space was approximately 41.077 GiB.
All project-controlled artifacts, runtime roots, logs, and test outputs remain
inside the repository. No project-controlled files were written outside the
repository.

M7 Slice 4A CSV + RAW-clip qualification and documentation closure preparation
are complete. At the time this validation report was prepared, the next
recommended gate is a final read-only pre-commit audit. This report does not
authorize staging, commit, push, pull request, merge, AVI work, or another GUI
session.
