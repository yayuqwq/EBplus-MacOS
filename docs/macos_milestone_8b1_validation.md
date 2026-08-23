# macOS Milestone 8-B1 CI Validation

## Status and scope

**Status:** `Complete / Qualified` for the fresh GitHub-hosted macOS arm64 CI
foundation recorded here.

M8-B1 proves that the selected dependency profile can be reproduced on a fresh
Apple Silicon runner; the prepared OpenEB producer and EBplus can be built;
the configured headless suite can run; and the installed `.app` can complete
static dependency, topology, signature, and contamination closure. It does
not replace the separate local M8-A Terminal-direct or Finder runtime evidence.

The final successful run is [macOS arm64 CI run 32626325967](https://github.com/yayuqwq/EBplus-MacOS/actions/runs/32626325967),
job [macOS 15 arm64](https://github.com/yayuqwq/EBplus-MacOS/actions/runs/32626325967/job/97162133481).

## Final run identity

| Item | Evidence |
| --- | --- |
| Branch and revision | `feat/macos-ci-foundation` at `3f85a44320c31ecb29037862dc75561afbb78bef` |
| Event | `pull_request` |
| Runner | GitHub-hosted `macos-15-arm64` / Apple Silicon |
| Compiler | AppleClang 17.0.0.17000013 |
| Run result | `success`, 2026-08-23T07:43:30Z through 2026-08-23T07:51:39Z (`8m09s`) |
| OpenCV | Homebrew `opencv@4` 4.14.0 |
| HDF5 | runner-local source bootstrap 1.14.6; archive SHA-256 `e4defbac30f50d64e1556374aa49e574417c9e72c6b1de7a4ff88c4b1bea6e9b` verified |
| OpenEB | prepared CenturyArks OpenEB 5.2.0 profile, configured, built, and installed |
| Tracked OpenEB tree | `8c12992a3d025ebe86e28a8ce80fe3c3da26b4a8` |
| HDF5 ECF pin | `b982d908a0bc0afd9104d226607bedb1a11b2a95` |

## Successful CI path

```text
checkout with full history and submodules
-> attach CI-local CenturyArks provenance state
-> OpenCV 4 selection and checksum-pinned HDF5 1.14.6 bootstrap
-> prepare CenturyArks OpenEB source
-> configure, build, and install OpenEB
-> configure and build EBplus
-> build-tree GTest discovery and HDF5 loader closure
-> full configured CTest
-> install and package the app bundle
-> transitive runtime closure and Qt framework topology normalization
-> bundle verifier and local ad-hoc signature verification
```

All listed workflow steps succeeded. The configured CTest result was:

```text
399 discovered
391 passed
8 skipped
0 failed
```

The eight skips are the CI profile's model-dependent ORT/model-off contract;
they are not a model-runtime pass.

### Package and loader closure

The build-tree HDF5 loader gate passed before CTest. During package closure,
the Apple native dependency pre-pass reported the expected unresolved
`@rpath/libhdf5.310.dylib` and `@rpath/libhdf5_cpp.310.dylib` identities. The
Apple-only fail-closed fallback admitted one trusted producer for each,
continued recursive closure, and `fixup_bundle()` completed.

The final app contains the HDF5 runtime closure under `Contents/Frameworks`,
including `libhdf5.310.5.1.dylib` / `libhdf5.310.dylib` and
`libhdf5_cpp.310.0.6.dylib` / `libhdf5_cpp.310.dylib`. The packaged-bundle
verification step passed the existing loader and producer-path contamination
policy; runner-local producer paths were used only while staging and did not
survive as final loader metadata.

`fixup_bundle()` initially materialized the compatibility entries of seven Qt
frameworks: `QtCore`, `QtDBus`, `QtGui`, `QtOpenGL`, `QtOpenGLWidgets`,
`QtSvg`, and `QtWidgets`. Each had selected version `A`, materialized
`Versions/Current`, `Resources`, root executable, and `Headers` entries. The
post-fixup topology gate validated identity and topology, restored all seven
to relative compatibility links, and reported zero missing `Current` links.

The final packaging step and the following verifier/signature step succeeded.
The latter ran strict verification for the main executable and recursive strict
verification for the app. This is local ad-hoc signature evidence, not
Developer ID or distribution-signing evidence.

Final remote disk telemetry recorded `39Gi` available, with CI-local
`.downloads` `37M`, `.tmp` `261M`, `.build` `200M`, and `.deps` `207M`.

## Bring-up findings retained for reproducibility

These findings describe CI/reproducibility bring-up, not a new application
functionality claim:

1. Homebrew's unversioned OpenCV moved to OpenCV 5, so the workflow selects an
   explicit `opencv@4` compatibility line.
2. The producer profile requires HDF5 1.14.6 while Homebrew provides HDF5 2.x,
   so CI bootstraps the official HDF5 1.14.6 archive under checksum control.
3. AppleClang 17 surfaced two unused-warning failures under OpenEB `-Werror`;
   two behavior-neutral `(void)main_dev` annotations make the uses explicit.
4. Build-tree GTest discovery required the selected runner-local HDF5 build
   RPATH.
5. A clean exporter artifact path exposed a test-helper portability defect:
   `symlink_status()` returning `not_found` is an expected pre-creation state.
6. Apple `file(GET_RUNTIME_DEPENDENCIES ... DIRECTORIES ...)` does not search
   producer directories. The retained pre-pass now has an Apple-only,
   fail-closed, unique-match recursive fallback for trusted producer dylibs.
7. BundleUtilities full-framework copying materializes Qt compatibility
   symlinks, and post-fixup Mach-O bytes can legitimately differ. The staged
   topology repair therefore verifies safe identity/topology rather than raw
   executable byte equality.

## Evidence boundary

M8-B1 CI success proves that a fresh GitHub-hosted Apple Silicon runner can
reproduce the documented dependency profile, build and install the customized
OpenEB producer, build EBplus, run the configured headless suite, package the
app, and pass static loader/signature closure.

It does **not** prove interactive Cocoa GUI runtime or Finder runtime. Those
remain separate M8-A local evidence; M8-B1 neither replaces nor reruns them.
It also does not prove physical CenturyArks camera operation, M6, M7 Slices
5/6, real E2VID inference, OpenCV 5 compatibility, HDF5 2 compatibility,
Developer ID, Gatekeeper, notarization, DMG, cross-machine end-user
distribution, or Linux CI/runtime.

The local machine remained below the project disk protection line during final
bring-up, so no local M8-B1 build was performed. Fresh build qualification came
from the GitHub-hosted runner.

## Final disposition

M8-B1 is **Complete / Qualified** for its documented fresh macOS arm64 CI
foundation scope. M8 overall remains **In progress** because native Linux CI
has not started.
