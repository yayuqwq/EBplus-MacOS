# macOS Milestone 7 Slice 1 Config Persistence Validation

## Status and scope

**Status:** `Complete / Qualified` for M7 Slice 1 only. Milestone 7 overall
remains **In progress**.

This report records the macOS Apple Silicon evidence for the bounded
algorithm-catalog, configuration-persistence and migration slice on
`feat/macos-config-persistence-contract`. It covers current source behavior,
focused automated tests, a current-source full build and CTest run, and one
representative no-camera Cocoa GUI Save/Load session. It is not a claim that
all of Milestone 7, all algorithms, or Linux parity is complete.

## Identity and provenance

| Item | Value |
| --- | --- |
| Branch / committed baseline | `feat/macos-config-persistence-contract` / `504caad386d39369832cb40db2d737682221d574` |
| Build-tree GUI | `.build/ebplus-macos/gui/gui_for_openeb` |
| Mach-O | arm64, UUID `B05C69DC-3B29-3DBA-B942-549B28D63282` |
| OpenEB profile | repository-local OpenEB / Metavision SDK 5.2 CenturyArks profile |
| ECF | `b982d908a0bc0afd9104d226607bedb1a11b2a95`, clean at audit time |
| Linkage audit | no `/usr/local` OpenEB 5.1.1, x86_64, or OpenEB producer build-tree dylib |
| ONNX Runtime cache mode | `AUTO`; no complete compatible ORT header/library pair was available, so the existing heuristic fallback remains possible |

`504caad386d39369832cb40db2d737682221d574` was the checked-out committed
baseline (and local `origin/main`) during qualification. The M7 Slice 1
implementation was the uncommitted qualification working tree: 11 tracked
implementation/test modifications plus
`gui/tests/test_algorithms_panel_config.cpp` as an untracked test. Therefore,
the SHA identifies the committed parent baseline; the evidence in this report
applies to that current working-tree diff, not to a claim that Slice 1 was
already committed at that SHA.

The ONNX item is discovery/fallback evidence only, not ONNX inference or
model qualification.

## Current catalog and ownership boundary

The current registry contains **33** entries:

| Category | Count | Persistence ownership |
| --- | ---: | --- |
| `cv` | 19 | self-developed algorithm configuration model |
| `analytics` | 7 | self-developed algorithm configuration model |
| `openeb_filter` | 7 | catalog metadata only; runtime enable/filter state remains FilterChain-owned |

The resulting 26 self-developed entries own desired-enabled configuration
state, including when an algorithm is not live. The seven OpenEB FilterChain
entries are captured as catalog metadata but this slice does not transfer their
runtime ownership into `AlgoBridge`. `sensor_self_test` and calibration are
non-registry workflows.

The historical M1 inventory count of 60 is retained as historical source audit
context in the parity matrix; it is not the current registry count.

## Configuration contract

### Format, version and rejection behavior

The algorithm parameter document identity is:

```text
format:  GUI-for-openEB-algo-params
version: 1
```

- A wrong format, malformed entry, malformed known or unknown content,
  malformed `algorithms` object, unsupported/future explicit version, or
  non-integral/string version is rejected during structural preflight before
  live instances, caches or desired-enabled state are mutated.
- An explicit version is accepted only when it is the integral numeric value
  `1`.
- A missing version remains compatible with legacy v1 files.
- A well-formed unknown/obsolete algorithm is a compatible partial-apply case:
  known entries apply, a warning identifies unknown names, and the separate
  `accepted` result is true even though the complete-result return is false.

`category` is advisory metadata. A known algorithm's parameters remain
loadable when its stored category is stale, allowing future category relocation
without discarding otherwise valid configuration.

This is not a comprehensive strict JSON-schema promise. Unknown or obsolete
parameter keys are warned and forwarded through the existing backend
compatibility path; unsupported keys are not reintroduced by canonical catalog
capture.

### Migration, numeric policy and lazy state

The documented migration is scoped to `background_mask`:

```text
learning_rate -> learning_window_s
```

When both spellings are present, the canonical `learning_window_s` wins
regardless of JSON ordering. Parseable integer and floating-point values are
clamped to their registered range before live or lazy application; nonnumeric
values retain the existing backend handling path.

Parameters for non-live algorithms are cached and replayed when the algorithm
is later created. Desired-enabled state is captured, loaded and replayed for
self-developed algorithms without forcing instantiation solely for
persistence. A sparse legacy entry that omits `enabled` preserves existing
desired state. Registered GUI enable paths use the bridge as the common source
of truth.

### UI synchronization

After an accepted file load, the Algorithms panel passively refreshes parameter
widgets, checkbox state, parameter-host visibility and applicable status/
display state. `QSignalBlocker` prevents that refresh from becoming user
`toggled` input or an enable-signal recursion. The automated contract covers
the lazy/non-instantiating model behavior; the GUI session below supplies
representative visible synchronization evidence only.

## Automated and build evidence

The completed Slice 1 qualification recorded:

| Check | Result |
| --- | --- |
| Focused configuration/catalog qualification | `46/46` passed |
| Current-source full build | Passed |
| Full CTest discovery | 358 tests |
| Full CTest | `358/358` passed; 0 failed; 0 timeout |

The retained current `LastTest.log` contains the 358 completed test records,
including the config migration and panel-refresh regressions. Historical
integration evidence reporting `42/42`, `341/341`, and an earlier GUI UUID
belongs to its frozen source state and is not rewritten by this report.

Relevant test coverage includes current-catalog capture, format/version
rejection, legacy missing-version behavior, advisory category behavior,
well-formed unknown algorithms, malformed-content no-mutation behavior,
unknown/obsolete keys, canonical-key migration, numeric clamping, lazy
parameter and desired-enabled replay, exclusive self state, and passive panel
refresh behavior.

## Cocoa GUI evidence

All sessions used the current arm64 UUID above, repository-local OpenEB/HDF5
plugin paths, `QT_QPA_PLATFORM=cocoa`, repository-local HOME/CFFIXED/TMPDIR/
XDG runtime isolation, and DYLD/LD/QT plugin overrides unset. No session
opened or connected a camera, interacted with the Devices UI, mutated
facilities, recorded, or opened a file source. Source-inherent startup
enumeration, where reached, is distinct from those device interactions.

### Session 1: startup failure before interaction

| Item | Value |
| --- | --- |
| Evidence roots | `.tmp/m7-config-gui-smoke-20260813-164632/`, `.logs/m7-config-gui-smoke-20260813-164632/` |
| Wrapper / GUI PID | `51453` / `51459` |
| Exit | `139` |
| Crash | `EXC_BAD_ACCESS`, `SIGSEGV(11)`, address `0x8` |

The macOS crash report recorded the main-thread chain:

```text
QScreen::geometry()
-> QWidget::saveGeometry()
-> gui::LayoutManager::capture_default()
-> gui::MainWindow::MainWindow()
-> main
```

stderr included PasteBoard/XPC/HIServices/LaunchServices service errors and
`no screens available`. The failure occurred before any configuration
interaction and before `Camera::list_online_sources()` was reached. No stack
or timing evidence implicated `ConfigManager`, `AlgoBridge`,
`AlgorithmsPanel`, or the Slice 1 Save/Load logic. It is recorded as a
transient/session-specific Cocoa startup failure; its root cause is not fully
established and it is not attributed to Slice 1 source.

### Session 2: same-context startup reproducibility

| Item | Value |
| --- | --- |
| Evidence roots | `.tmp/m7-cocoa-startup-repro-20260813-185018/`, `.logs/m7-cocoa-startup-repro-20260813-185018/` |
| Wrapper / GUI PID | `56326` / `56336` |
| Exit | `0` |

The same IDE Codex launch route, binary UUID and repository-local isolation
topology produced a normal, movable and interactive `EB plus` main window with
no fatal dialog. `Cmd+Q` exited cleanly. The previous no-screen/QScreen
failure did not recur. This supports non-reproducibility in the same launch
context; it does not prove that a future Cocoa launch can never fail or that a
single root cause has been established.

### Session 3: representative config Save/Load qualification

| Item | Value |
| --- | --- |
| Evidence roots | `.tmp/m7-config-gui-qualification-20260814-001030/`, `.logs/m7-config-gui-qualification-20260814-001030/`, `.artifacts/m7-config-gui-qualification-20260814-001030/` |
| Wrapper / GUI PID | `58699` / `58708` |
| Exit | `0` |

The maintainer used the real `File -> Save Algo Params...` and `File -> Load
Algo Params...` workflows with the self-developed `Hot Pixel Filter`:

```text
State A: enabled ON, FPN target rate = 654
Save:    state-a.json
State B: enabled OFF, FPN target rate = 321 (not saved)
Load:    State A restored
Save:    state-after-reload.json
Quit:    Cmd+Q, normal exit
```

The maintainer observed normal startup, no fatal/error dialog, usable UI,
successful status-bar save/load messages, no residual `321`, no flicker,
repeated-toggle or recursion storm, and no camera/device activity. The
`ON -> OFF -> ON` enabled-state transition is a maintainer's supplemental
human GUI observation after the session, not a machine-logged UI trace. It is
accepted for this bounded representative qualification; a future independent
regression may record that transition more explicitly.

The session stderr contains only the existing Inter font-alias notice, one
window-move warning and one IMK mach-port message. It contains no no-screen,
QScreen, SIGSEGV, PasteBoard/XPC/LaunchServices, dyld or fatal marker, and no
new GUI crash report was created.

## Saved-artifact semantics

The GUI itself created:

- `.artifacts/m7-config-gui-qualification-20260814-001030/state-a.json`
- `.artifacts/m7-config-gui-qualification-20260814-001030/state-after-reload.json`

Both are 53,497-byte, byte-identical JSON files with SHA-256:

```text
73278ac5f6503b7b878d8db065bf61d46b84280dfab44291f0a1287f588dee6a
```

Each has `format = GUI-for-openEB-algo-params`, `version = 1`, 33 catalog
entries, and this representative entry:

```text
algorithms.hot_pixel_filter.category = cv
algorithms.hot_pixel_filter.enabled = true
algorithms.hot_pixel_filter.params.fpn_target_rate_hz = "654.000000"
```

Thus the GUI-visible parameter sequence `654 -> 321 -> load 654` and the
final desired-enabled semantic state are preserved by save-after-reload. State
B's `321` / OFF state is absent from the final artifact. This is representative
configuration evidence, not all-algorithm runtime or numerical evidence.

## Workspace-policy incident and remediation

During a post-session artifact semantic comparison, the agent incorrectly
created two repository-external normalized comparison files:

```text
/tmp/m7_state_a.normalized.json
/tmp/m7_state_after.normalized.json
```

Each was a 1,681-byte regular JSON file derived from the retained
repository-local artifacts. This was a repository-local workspace-policy
violation. It was detected, then the two exact paths were verified as ordinary
non-symlink derived files outside the repository, unreferenced by Git and not
unique validation evidence. Under explicit maintainer authorization, exactly
those two paths were removed. No wildcard, parent-directory or broad cleanup
was performed. The incident is process/governance evidence; it does not erase
or invalidate the functional qualification, and it must remain part of the
validation history.

## Evidence boundary and remaining work

This Slice 1 closure supports only:

- the current 33-entry catalog/config contract;
- format/version handling, migration, unknown-content compatibility and
  numeric-range behavior;
- lazy parameter and self desired-enabled persistence;
- automated configuration/UI synchronization regression coverage;
- one representative real Cocoa GUI Save/Load round trip; and
- the recorded macOS arm64 current-source build and CTest regression evidence.

It does **not** establish all 33 algorithm runtime behavior, algorithm
numerical correctness, all preprocessing modes, OpenEB FilterChain runtime
parity, E2VID/ONNX inference, model workflows, CSV/RAW-clip/AVI/general
algorithm-result export, calibration, processed recording, physical-camera or
facility behavior, standalone packaging, `.app` bundle behavior, signing,
notarization, Linux runtime, or Linux CI.

**Linux remains Not run / unverified.** No new Linux residual-risk acceptance
is made here.

M6 remains **Planned / Paused — physical CenturyArks camera currently
unavailable**. It is a hardware-temporary dependency, not failed, cancelled or
complete, and this Slice 1 evidence does not substitute for it.

## Closure result

M7 Slice 1 documentation closure preparation is complete. The next action is
maintainer review followed, only if separately authorized, by an explicit
commit decision. This report does not authorize a commit, push, pull request,
or work on another M7 slice.
