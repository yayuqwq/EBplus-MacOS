# macOS DAT Playback Validation

## 1. Status

**Status:** Passed — bounded macOS arm64 build-tree DAT playback validation for
one CD DAT fixture generated from the known `sparklers.raw` sample.

Milestone 5 remains `Planned`.

## 2. Scope and evidence boundary

The retained evidence covers macOS arm64 on the `main` lineage based on
`2d0f4fc54e57bc8820c208ac06c84d5a873ff9fd`, using the build-tree
`gui_for_openeb` and repository-local OpenEB 5.2 CenturyArks. The source was
the tracked `algo/tests/sparklers.raw`, copied once to an isolated working RAW
under a repository-local `SAMPLE_ROOT`. The playback fixture was the resulting
CD DAT only. The zero-byte trigger DAT was retained but was not used as
playback media.

This evidence supports one bounded CD DAT workflow only. It does not establish
coverage for other DAT files, geometries, sizes, corrupt inputs, installed or
packaged executables, Linux, algorithms, models, or export.

## 3. Converter provenance and contained generation

The converter was the arm64 repository-local OpenEB 5.2 build-tree binary:

```text
.build/openeb-5.2.0-centuryarks-macos/bin/metavision_file_to_dat
```

Its provenance and build-tree RPATH were audited as repository-local OpenEB
5.2 CenturyArks, with no `/usr/local` OpenEB 5.1.1 provenance and no `DYLD_*`
workaround. The tracked OpenEB source audit established the default naming
rule:

```text
foo.raw -> foo_cd.dat
foo.raw -> foo_trigger.dat
```

The converter exited `0`. Its retained log recorded both outputs under the
isolated repository-local `SAMPLE_ROOT`. The resulting CD DAT was 4,170,088
bytes; the retained trigger DAT was 0 bytes.

## 4. RAW index sidecar and containment

The tracked OpenEB RAW reader/index path also creates `foo.raw.tmp_index` when
it opens and indexes a RAW stream for seeking. The observed working-RAW
sidecar, `sparklers-dat-20260727T085125Z.raw.tmp_index` (1,322 bytes), is an
expected RAW indexing/seek sidecar. It is not a third DAT logger output and is
not a DAT conversion failure.

All conversion-associated files remained inside the one isolated,
repository-local `SAMPLE_ROOT`: the working RAW (2,109,142 bytes), the CD DAT,
the zero-byte trigger DAT, and the RAW index sidecar. The zero-byte trigger
stream output was retained and not opened or validated as playback media.

## 5. CLI harness correction and DAT evidence

The first CLI attempt used:

```text
metavision_file_info <CD_DAT>
```

It exited `1` with `Parsing error: the option '--input-event-file' is required
but missing`. This was a **validation harness invocation error**: processing
stopped during CLI argument parsing, so that invocation did not open or decode
the DAT and is not DAT decoder evidence.

The corrected invocation used:

```text
metavision_file_info -i <CD_DAT>
```

It exited `0` and reported:

| Field | Result |
| --- | --- |
| Encoding | ECF |
| Duration | 95,871 us |
| CD events | 521,252 |
| First timestamp | 0 |
| Last timestamp | 95,871 |
| Geometry | Not reported by this CLI output |

## 6. GUI runtime and observed workflow

The retained GUI run used the build-tree executable and recorded:

| Field | Result |
| --- | --- |
| PID | 16352 |
| Start | 2026-07-27T09:41:54Z |
| End | 2026-07-27T09:44:42Z |
| Exit | 0 |

The picker included `*.dat`. The exact CD DAT opened with no error dialog and
showed a non-empty changing event visualization. Autoplay, nonzero duration,
advancing position, and playback controls were observed. One pause/resume
sequence and one forward plus one backward seek smoke sequence passed. Recent
reopen passed with autoplay and a non-empty display. With loop disabled,
natural EOF was observed without a crash or hang; a subsequent Recent reopen
recovered playback. The window then closed cleanly with exit `0`.

## 7. Generic-offline regression and automated logs

`Error 102113: Device unavailable` did not recur during this DAT validation.
This is additional DAT runtime evidence for the shared generic-offline fix from
PR #15; it does not constitute Linux validation.

The retained GUI logs contained no `102113`, `Device unavailable`, `SIGABRT`,
`SIGSEGV`, uncaught exception, dyld fatal, Qt fatal, or DAT parser/decoder
error. Two Cocoa `Window move completed without beginning` messages and one
IMK mach-port message were observed as nonfatal logs.

## 8. Recent-files evidence

Native QSettings remained user-domain persistent, so the Recent result used an
exact full-path differential. The exact CD DAT path appeared zero times before
the run and once afterward.

## 9. Passed

- DAT generation and containment inside the isolated `SAMPLE_ROOT`
- DAT CLI read/decode through the corrected `-i` invocation
- DAT picker extension, GUI open, changing display, and autoplay
- Pause/resume and forward/backward seek smoke coverage
- Recent reopen, natural EOF, and post-EOF recovery
- Clean GUI exit and absence of the generic-offline DeviceUnavailable
  regression

## 10. Failed

None among the correctly executed DAT validation contracts.

## 11. Observed limitations

- The converter-generated trigger DAT was zero bytes and was not tested as
  playback media.
- Coverage is one CD DAT fixture only.
- The CLI output did not report geometry.
- Exact numeric seek accuracy was not measured.
- Large-file, performance, and stress coverage were not performed.

## 12. Not run

- Other DAT fixtures, trigger DAT playback, corrupt inputs, different
  geometries, large files, and permission-failure cases
- Algorithms, models, export, installed executable, and packaged executable
- Linux compilation, Linux GUI runtime, Linux DAT runtime, and long-stability
  validation

## 13. Milestone impact

This adds bounded DAT GUI playback coverage to the existing RAW and HDF5/H5
macOS build-tree evidence. It does not close Milestone 5, which remains
`Planned`.
