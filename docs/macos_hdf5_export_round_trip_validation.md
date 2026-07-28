# macOS HDF5 Export Metadata Round-trip Validation

## 1. Status

**Status:** Passed — narrow macOS Apple Silicon HDF5 export metadata and
reopen-crash regression validation.

This result does not close Milestone 5, which remains `Planned`.

## 2. Scope and boundaries

This record covers the shared, platform-neutral `ExporterController::run_hdf5`
path using the tracked `algo/tests/sparklers.raw` fixture, the build-tree GUI,
and the repository-local OpenEB / Metavision SDK 5.2 CenturyArks profile on
macOS arm64. It covers one fresh HDF5 export, one reopen, minimal playback
smoke, and CLI comparison of the source and output.

It does not validate AVI, ExtTrigger export, cancellation, overwrite handling,
disk-full handling, large files, algorithms, models, physical cameras, or any
Linux runtime path.

## 3. Pre-fix failure

The pre-fix export completed and wrote a 1,819,474-byte HDF5 file, but reopening
that output crashed the GUI:

| Field | Value |
| --- | --- |
| Wrapper PID | `26014` |
| GUI PID | `26019` |
| GUI exit | `139` |
| Exception | `EXC_BAD_ACCESS` / `SIGSEGV` at null address |

The recorded call chain was:

```text
CameraController::fetch_sensor_info()
→ setup_camera()
→ connect_file()
→ PlaybackController::open_file()
```

The EBplus exporter wrote events but did not preserve source-camera metadata.
The resulting HDF5 lacked geometry; the OpenEB 5.2 generic offline reader then
reached a null geometry dereference during geometry-dependent setup. The old
crash binary is not treated as the post-fix binary and was not symbolicated
again.

## 4. Fix

`ExporterController::run_hdf5()` now follows the canonical OpenEB 5.2
`metavision_file_to_hdf5` sequence:

```cpp
Metavision::HDF5EventFileWriter writer(...);
writer.add_metadata_map_from_camera(cam);
```

The metadata call occurs before any `writer.add_events()` call. It uses the
OpenEB-provided metadata writer rather than manually writing geometry or
hard-coding dimensions. The OpenEB helper copies the source camera metadata
map while intentionally excluding only `evt` and `plugin_name`; therefore the
source geometry, format-related metadata, generation, and other supported
source metadata are preserved by the canonical contract.

The change is shared C++: it has no Apple/Linux conditional and does not alter
ExtTrigger export scope. No OpenEB source was modified.

## 5. Automated validation

The existing Release/arm64 build tree used the repository-local OpenEB 5.2
CenturyArks profile. The following checks passed:

| Check | Result |
| --- | --- |
| Focused `ExporterController.Hdf5ExportPreservesGeometry` | `1/1` passed |
| Full CTest | `310/310` passed |

The focused regression uses the tracked RAW fixture, exports a fresh HDF5 to a
build/test-controlled artifact directory, reopens it with
`Metavision::Camera::from_file()`, calls `camera.geometry()`, requires positive
width and height, and requires positive offline duration.

## 6. Post-fix GUI round-trip

The authoritative post-fix build-tree executable was arm64 with UUID
`0B5B4907-E3F8-305E-9941-13D35E4A26BD`.

| Field | Value |
| --- | --- |
| Wrapper PID | `42392` |
| GUI child PID | `42411` |
| Exported HDF5 size | `1,820,474` bytes |
| GUI exit | `0` |

The manual GUI run passed:

- tracked RAW open and normal playback;
- Export Dialog source prefill;
- HDF5 export reaching explicit `Done`;
- exported HDF5 reopen without `SIGSEGV`, `EXC_BAD_ACCESS`, or error `102113`;
- autoplay, pause/resume, and one seek; and
- normal GUI close.

The post-fix stderr and CLI records contain no `SIGSEGV`, `SIGABRT`, `102113`,
`DeviceUnavailable`, uncaught exception, HDF5 writer/parser, Qt fatal, or dyld
fatal marker.

## 7. CLI source/output comparison

The repository-local `metavision_file_info -i` command completed successfully
for both source and output.

| Field | RAW | Exported HDF5 |
| --- | ---: | ---: |
| Encoding | EVT2 | ECF |
| Generation | 3.0 | 3.0 |
| CD events | 521252 | 521252 |
| First timestamp | 0 | 0 |
| Last timestamp | 95871 | 95871 |
| Duration | 95 ms 871 us | 95 ms 871 us |

The CLI does not independently report geometry for this record. Geometry is
instead covered by the focused `camera.geometry()` regression and by the
successful GUI reopen reaching geometry-dependent startup.

## 8. Remaining limitations

An arbitrary externally-created HDF5 with missing geometry metadata may still
expose an OpenEB 5.2 generic-reader robustness issue. This change repairs
EBplus-generated HDF5 by preserving source metadata; it does not patch the
OpenEB malformed/missing-metadata reader behavior.

Linux native compilation, Linux GUI runtime, and Linux HDF5 export were not
run and remain unverified. Milestone 5 remains `Planned`.
