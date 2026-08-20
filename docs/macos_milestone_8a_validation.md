# macOS Milestone 8-A Validation

## Status and scope

**Status:** `Complete / Qualified` for a local ad-hoc-signed macOS arm64
packaged offline Cocoa runtime within the scope recorded here.

Milestone 8 overall remains **In progress**. Apple Silicon CI, Finder launch,
Developer ID signing, Gatekeeper, notarization and DMG work remain outside this
closure. This document records the complete M8-A evidence chain, including the
initial launch failure and its M8-A1 repair. It does not qualify physical-camera
operation, Linux runtime behavior, model runtime in the final regression, or
distribution readiness.

No GUI redesign was part of M8-A. The application currently uses a deliberate
frameless window with a custom title bar; the absence of native macOS traffic
lights is a UI design limitation, not a bundle or signing failure.

## Identity and provenance

| Item | Value |
| --- | --- |
| Branch | `build/macos-packaging-ci` |
| Starting/implementation base | `78bb53b38e0e5e088dfd6564203cdbbe769179a6` |
| App | `$REPO_ROOT/.deps/ebplus-macos/gui_for_openeb.app` |
| Executable | `$REPO_ROOT/.deps/ebplus-macos/gui_for_openeb.app/Contents/MacOS/gui_for_openeb` |
| Architecture | arm64 |
| Final executable UUID | `559F91F3-C95C-3FAE-B9C4-EAE3CF10C278` |
| Signature | local ad-hoc (`Signature=adhoc`, no TeamIdentifier) |
| OpenEB profile | repository-local OpenEB / Metavision SDK 5.2 CenturyArks profile |
| Tracked OpenEB tree | `b407c407aa46d3b97edc9b2096fb120a96c8b465` |
| Imported upstream release | OpenEB 5.2 commit `9003b5416676e78ba994d912087486cfa94fae73` |
| HDF5 ECF | `b982d908a0bc0afd9104d226607bedb1a11b2a95` |
| ORT | repository-local arm64 ONNX Runtime 1.29.0 runtime closure; no model was packaged or selected |

The tracked OpenEB tree and its upstream release commit are distinct objects;
the provenance correction is recorded in
[`openeb_version_isolation.md`](openeb_version_isolation.md). The prepared
CenturyArks source, OpenEB build/install prefix and EBplus build tree remain
under the repository-local `.tmp/`, `.build/` and `.deps/` roots.

## Evidence chain

### 1. Bundle foundation and loader closure

The macOS path creates a real `MACOSX_BUNDLE`, stages Qt frameworks/plugins,
OpenCV, OpenEB/HAL, HDF5 ECF and the linked ORT runtime under
`Contents/Frameworks`/`Contents/PlugIns`, and uses bundle-local RPATH and
startup plugin paths. The install-time fixup normalizes IDs, dependencies and
RPATHs in the final staged bundle, then verifies the result.

The final bundle verifier recorded:

- `289` logical arm64 Mach-O entries;
- zero prohibited loader paths;
- no `/Users/...` runtime dependency, `.build` dependency, `.deps` absolute
  dependency, `/usr/local` OpenEB, `/opt/homebrew` runtime dependency,
  x86_64 object or producer build-tree dependency; and
- successful standalone loader closure inspection.

The verifier record is retained in
`.logs/m8-a-codesign/20260820-161205/loader_verifier_after.txt` and
`loader_path_audit_after_verified.txt`.

### 2. First packaged Cocoa launch: failed

The first authorized Terminal-direct launch used the final staged executable
and reached dyld before the application window. It failed with:

```text
EXC_BAD_ACCESS
SIGKILL (Code Signature Invalid)
Namespace CODESIGNING, Code 2, Invalid Page
```

No main window appeared; stdout and stderr were both zero bytes. The retained
session metadata is under
`.logs/m8-a-gui-qualification/20260820-155551/`; the system crash report was
provided as runtime evidence and was not copied into the repository.

This failure is distinct from loader-closure success: static dependency
resolution did not establish code-signature integrity or Cocoa runtime
success.

### 3. M8-A1 diagnosis

The read-only signature audit confirmed:

- top-level strict recursive verification failed with a resource/signature
  mismatch;
- the main arm64 executable carried a linker-generated ad-hoc signature but
  strict verification failed;
- invalid signatures were broad, including all seven Qt framework code units,
  representative OpenCV/ORT/other flat dylibs and Qt plugins; and
- the install pipeline performed `fixup_bundle` and `install_name_tool` ID,
  dependency and RPATH mutations after those pre-existing signatures, with no
  final signing stage.

Therefore H1 (post-fixup Mach-O mutation invalidated signatures) and H3
(multiple nested objects were invalid) were confirmed. H2 (only the main
executable was invalid) was rejected. H4 (top-level resource/nested-code
sealing was also invalid) was confirmed. An empty TeamIdentifier was not a
defect: these were local ad-hoc signatures.

The diagnosis is retained in
`.logs/m8-a-codesign/20260820-161205/diagnosis_summary.txt` and the before
inventory files.

### 4. M8-A1 repair

The final install pipeline is now:

```text
stage/copy final bundle
-> Mach-O ID/load-path/RPATH fixups
-> no further Mach-O mutation
-> sign final flat leaves
-> sign nested framework/bundle code units
-> sign the .app last
-> recursive verification
```

The signer uses `codesign --force --sign -` only on code copied into the final
`.app`; it does not use `--deep` for signing and never re-signs producer
prefixes. The final bundle verifier records valid ad-hoc signatures for the
main executable, Qt plugins, flat dylibs and seven framework containers.

### 5. Static post-repair verification

`codesign --verify --deep --strict --verbose=4` passed for the app. Independent
main-executable and representative nested-object verification also passed.
The main signature reports `Format=app bundle with Mach-O thin (arm64)`,
`Signature=adhoc`, and a sealed resource envelope. The loader verifier again
reported `289` arm64 logical Mach-O entries and zero prohibited paths.

These are local execution-integrity results only. They are not Developer ID,
Gatekeeper, notarization or release-signing evidence.

### 6. Second bounded Terminal-direct Cocoa session

The one authorized retry used the exact packaged executable, with the current
`DYLD_*`, `MV_HAL_*` and `HDF5_PLUGIN_PATH` variables scrubbed from that child
process. No camera source or model selector was used.

The fixture was the tracked file `algo/tests/sparklers.raw`:

| Property | Value |
| --- | --- |
| Size | `2,109,142` bytes |
| SHA-256 | `e84afbecdc07d2910ae846a4ae0ee246f5b9c97a53816c637d4f85c023d7c234` |

The session passed the following human observations:

- main window `EB plus` appeared without an error dialog;
- `File` -> `Open File...` opened the tracked RAW and showed the event/playback
  view;
- playback updated normally;
- `Pause` stopped updates while the GUI remained responsive;
- `Play` resumed updates;
- the seek slider moved to approximately 50% and playback remained normal;
- EOF left the window responsive with no error; and
- `File` -> `Exit` closed normally.

The process exit code was `0`. Session metadata and stdout/stderr are retained
under `.logs/m8-a-gui-qualification/20260820-194707/`. The only reported
messages were non-fatal font/IMK diagnostics; no fatal marker was found.

### 7. Full regression CTest

The final configured suite ran once from the existing Release/arm64 build tree:

```text
399 discovered
391 passed
8 skipped
0 failed
exit 0
```

The eight skipped cases are conditional real-model tests because
`EBPLUS_E2VID_TEST_MODEL` was not set. No model runtime was invoked in this
final regression. The complete console record is
`.logs/m8-a-full-ctest/20260820-201933/ctest.log`.

The CTest shell inherited:

```text
HDF5_PLUGIN_PATH=:/usr/local/lib/plugin:/usr/local/lib/plugin
```

Consequently, this CTest run is regression evidence, not dependency-isolation
evidence. Dependency isolation is established by the bundle loader verifier,
the scrubbed packaged Cocoa session, repository-local OpenEB/ORT provenance,
and the earlier scrubbed focused tests.

## Evidence classification and boundaries

### Machine evidence

Bundle structure, Mach-O architecture and UUID, `LC_LOAD_DYLIB`/`LC_RPATH`,
code-signature verification, the 289-entry loader audit, fixture hash, process
exit code, CTest counts and fatal-marker scans are machine evidence.

### Human observation

Window appearance, RAW view, visible playback, pause/resume, approximate seek,
EOF responsiveness and normal `File -> Exit` are human observations. They are
not numerical event-count, pixel-correctness, algorithm-correctness or model
qualification claims.

## Not run and limitations

- Finder launch: **Not run**; Terminal-direct launch does not qualify Finder.
- Developer ID signing: **Not run**.
- Gatekeeper: **Not run**.
- Notarization: **Not run**.
- DMG: **Not run**.
- CI: **Not run**.
- Physical camera, M6 and M7 Slice 5/6: **Blocked / Not run**.
- Model runtime in the final regression: **Not run**; eight conditional skips.
- Linux native configure/build/CTest/runtime: **Not run / unverified**.

Local ad-hoc signing is local execution integrity; it is not Developer ID
signing, notarization or distribution readiness. No E2VID checkpoint or
recurrent ONNX artifact was copied into the app or added to Git.

## Final disposition

M8-A is **Complete / Qualified for local ad-hoc-signed macOS arm64 packaged
offline Cocoa runtime within this documented scope**. M8 overall remains
**In progress** pending CI and distribution work. M6 remains
**Planned / Paused — physical CenturyArks camera unavailable**; M7 remains
**In progress / paused pending M6 hardware prerequisites** for its live-camera
dependent slices.
