# CenturyArks OpenEB 5.2 bounded live-event validation

## 1. Scope

This document records Milestone 2C-B2: a repository-owned, headless, bounded CD-event smoke against the existing B1 OpenEB 5.2 CenturyArks installation. The probe is independent of canonical OpenEB source and does not record event data or change camera parameters.

| Area | Result |
| --- | --- |
| Camera API audit | Passed |
| Bounded tool implementation | Passed |
| arm64/Release configure | Passed |
| arm64/Release build | Passed |
| Repository-local OpenEB 5.2 selected | Passed |
| No OpenEB 5.1.1 contamination | Passed |
| Existing B1 SDK OpenCV runtime closure | Accepted; B2 adds no OpenCV component or API |
| Physical selector enumeration | Passed; exactly one CenturyArks device |
| Run 1 live CD events | Passed |
| Run 2 live reopen CD events | Passed after an explicit three-second wait |
| Pre/post parameter comparison | Passed; zero-line stable-field diff |
| Recording/output audit | Passed; no event-data files generated |

The implementation and runtime acceptance are intentionally limited to:

```text
explicit runtime selector
Metavision::Camera::from_serial
read-only CD callback statistics
maximum requested capture duration of 5 seconds
normal stop and callback cleanup
one bounded reopen after at least 3 seconds
```

Both bounded runs completed, received CD events, stopped normally, and exited zero.

## 2. Camera API audit

The implementation was derived from the prepared OpenEB 5.2 source, not from an earlier SDK version. The audited API contract is:

| Area | OpenEB 5.2 contract |
| --- | --- |
| Camera include | `<metavision/sdk/stream/camera.h>` |
| Explicit open | `Metavision::Camera::from_serial(const std::string &)` |
| CD callback | `void(const EventCD *begin, const EventCD *end)` |
| CD callback removal | `camera.cd().remove_callback(callback_id)` |
| Runtime error callback | `void(const CameraException &)` |
| Runtime callback removal | `camera.remove_runtime_error_callback(callback_id)` |
| Start/stop | `bool start()`, `bool stop()`; both may throw `CameraException` |
| Runtime-error rule | callback sets state only; `stop()` remains on the main thread |
| CMake component/target | exact SDK `5.2.0`, component `stream`, target `MetavisionSDK::stream` |

The detailed source references are preserved in the ignored `api-audit.log`.

## 3. Tool safety design

The tracked probe accepts only a mandatory full runtime selector containing exactly three non-empty `integrator:plugin:serial` fields, a duration from 100 through 5000 ms, and a non-negative minimum event count. Rejecting additional separators prevents the OpenEB Release parser from dropping the explicit integrator/plugin constraint. The tool has no first-device path, output path, recording mode, or configuration-file input. No selector, complete serial, product string, or firmware value is compiled into the tool, and tool-owned exception diagnostics report only numeric error codes.

The CD callback keeps only aggregate counters and first/last timestamp bounds under a mutex. It performs no file I/O and does not retain event buffers. The runtime-error callback only sets an atomic flag. The main thread uses `steady_clock`, stops the camera, removes both callbacks, and emits one selector-free JSON object.

The source and CMake do not directly request or call UI, Qt, OpenCV, recording, firmware, EEPROM, bias, ROI, trigger, anti-flicker, ERC, or pixel-mask APIs. The B1 `MetavisionSDK::stream` target and binary have existing SDK-transitive OpenCV dependencies; B2 does not add an OpenCV component or API dependency.

## 4. Build and runtime profile

The validation profile uses:

```text
Source: tools/centuryarks_live_event_smoke
Build: .build/centuryarks-live-event-smoke-macos
OpenEB prefix: .deps/openeb-5.2.0-centuryarks-macos
Logs: .logs/centuryarks-live-event-smoke-macos
Artifacts: .artifacts/centuryarks-live-event-smoke-macos
Architecture: arm64
Build type: Release
Generator: Unix Makefiles
SDK: OpenEB / Metavision 5.2.0 exactly
```

The executable is not installed or distributed. The only RPATH B2 sets explicitly is the ignored validation path to the repository-local B1 library directory. CMake also emitted `/opt/homebrew/lib` for the imported SDK's existing package-manager dependencies. Runtime clears `DYLD_LIBRARY_PATH` and `DYLD_FALLBACK_LIBRARY_PATH`, uses `PLUGIN_PATH_ONLY`, and redirects HOME/TMPDIR into the repository.

Configure selected the B1 Metavision SDK package at version `5.2.0`, `Release`, and `arm64`. The final executable is a Mach-O arm64 binary and has no build-tree OpenEB dependency or `/usr/local/lib/libmetavision*` dependency.

The original no-OpenCV-transitive-dependency gate was rejected as incompatible with the existing OpenEB 5.2 SDK link interface. `MetavisionSDK::stream` publicly links `MetavisionSDK::core`, and the audited target and binary already carry OpenCV dylibs.

The probe itself does not request or call OpenCV, Qt, UI, or highgui APIs. The accepted OpenCV libraries are pre-existing transitive dependencies of `MetavisionSDK::stream`. OpenEB was not modified or rebuilt.

## 5. Runtime result

Exactly one full runtime selector of the expected form was discovered:

```text
CenturyArks:hal_plugin_centuryarks:<runtime serial>
serial SHA-256 prefix: cb823604ea92
VID/PID: 31f7:0003
```

The complete selector and serial are retained only in permission-restricted ignored logs. The hash identifies one tested physical instance and is not an implementation condition.

Both `hal_plugin_centuryarks` and `hal_plugin_prophesee` loaded before and after the runs. The explicit selector constrained open to `hal_plugin_centuryarks`; the opened device reported integrator `CenturyArks`. The standard plugin's Treuzell discovery did not claim the device.

| Result | Run 1 | Run 2 |
| --- | ---: | ---: |
| Requested duration | 5000 ms | 3000 ms |
| Observed duration | 5005 ms | 3007 ms |
| Callback batches | 800,595 | 86,282 |
| CD events | 233,846,153 | 27,423,084 |
| First timestamp | 1,107 us | 1,125 us |
| Last timestamp | 4,999,999 us | 2,999,999 us |
| Timestamp span | 4,998,892 us | 2,998,874 us |
| Runtime error | false | false |
| Start/stop | passed | passed |
| Exit code | 0 | 0 |

The workflow waited at least three seconds after Run 1 before invoking the runner for Run 2. The runner itself then provided its documented three-second movement prompt. This validates bounded live start/stop and a subsequent bounded reopen, not a stress test or physical reconnect lifecycle.

Event and callback counts depend strongly on scene motion, contrast, and illumination. These runs are not throughput benchmarks. They do not evaluate event loss, timestamp continuity beyond the reported first/last bounds, event correctness, image quality, or long-duration stability.

The pre-run and post-run `metavision_platform_info --system` stable snapshots were byte-equivalent after normalization. The zero-line diff covered:

- sensor `IMX636`, USB connection, CenturyArks integrator, and EVT3 encoding;
- the visible product release/build/speed fields;
- device options `format` defaulting to EVT3 and `ll_biases_range_check_bypass` defaulting to `0`;
- the six visible default bias values and their current register values.

Geometry was not printed by this `platform_info --system` build, and the B2 probe deliberately did not add a geometry/facility query. No stable visible configuration field changed. Both installed HAL plugin hashes also remained unchanged.

Each stdout file contains exactly one selector-free JSON object. Stderr contains only the three-line movement prompt. `.artifacts/centuryarks-live-event-smoke-macos`, the runtime HOME, and runtime TMPDIR contain no files; no RAW, DAT, HDF5, CSV, image, or video output was created.

`docs/macos_porting_plan.md` was updated to:

```text
Milestone 2: Complete
Milestone 2C bounded live-event validation: Complete
Milestone 2C PID 0003 enumeration/open/reopen: Verified
Milestone 2C PID 0003 bounded live CD event delivery: Verified
```

## 6. Evidence and checks

Complete runtime identity and raw TRACE output remain under the ignored, permission-restricted directory:

```text
.logs/centuryarks-live-event-smoke-macos/
```

Key evidence includes the API audit, configure/build logs, CMake cache and link command, executable `file`/`otool` output, pre/post `--short` and `--system` logs, normalized stable-field snapshots and zero-line diff, two JSON run results, exit codes, the explicit reopen wait, plugin identity summary, and before/after plugin hashes. `runtime-selector.txt` and both run output/error files have mode `0600`.

Checks run:

- exact Metavision SDK 5.2.0 package selection, arm64/Release configure and build;
- C++/runner CLI bounds, Bash syntax, source privacy and prohibited-API scans;
- Mach-O architecture, dependency and RPATH audit;
- no `/usr/local` Metavision or OpenEB build-tree contamination;
- exactly one explicit CenturyArks runtime selector;
- pre-run `--short` and `--system`;
- bounded five-second live CD callback run;
- explicit wait of at least three seconds;
- bounded three-second reopen run;
- post-run `--short` and `--system`;
- stable system-field and installed B1 plugin hash comparisons;
- stable OpenEB 5.1.1 CLI/library/plugin manifests and external prefix stat comparison;
- empty artifact, runtime HOME, and runtime TMPDIR audit;
- canonical OpenEB and HDF5 ECF Git integrity checks.

Checks not run:

- RAW/event recording, GUI or EBplus application execution;
- facility access or any parameter mutation;
- EEPROM, pixel-mask, firmware, physical unplug/replug, multi-camera, PID `0002`, or PID `0004` checks;
- sustained/stress/performance, event-quality, or Linux regression tests.

## 7. Disk and workspace

```text
Repository before: 369,052 KiB
Repository after:  370,144 KiB
Project growth:      1,092 KiB

Smoke build:           860 KiB
Smoke logs:            184 KiB
Smoke artifacts:         0 KiB
Runtime HOME/TMPDIR:      0 KiB

Available before: 48,092,772 KiB
Available after:  46,972,084 KiB
```

The larger filesystem-wide availability change during the session is not attributable to the 1,092 KiB repository growth. Available space remained above both workspace protection lines. No project-controlled files were written outside the repository. No OpenEB install, Homebrew operation, or system-prefix write was performed.

## 8. Validation boundaries

This milestone verifies live CD event delivery for one PID `0003` camera through two bounded runs. It does not validate sustained streaming, event correctness or image quality, throughput, recording, GUI or EBplus application integration, facility access, parameter changes, EEPROM, pixel masks, firmware operations, physical unplug/replug, multiple cameras, PID `0002`, PID `0004`, or Linux runtime behavior.
