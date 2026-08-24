# macOS Milestone 8 Packaging and Reproducibility Closure

## Status and scope

**Status:** `Complete / Qualified` for the documented macOS Apple Silicon
packaging and reproducibility scope.

This is the overall closure record for Milestone 8. It combines the independent
local packaged-runtime evidence from M8-A with the fresh GitHub-hosted Apple
Silicon CI evidence from M8-B1. It is not a cross-platform release
qualification and does not replace either child record's evidence boundary.

## Qualified evidence

| Component | Qualified evidence |
| --- | --- |
| M8-A: local packaged runtime | A final arm64 `.app`; standalone loader closure; local ad-hoc signing; bounded Terminal-direct Cocoa runtime; Finder launch; and bounded tracked-RAW playback interaction. See [M8-A validation](macos_milestone_8a_validation.md). |
| M8-B1: fresh CI foundation | GitHub-hosted `macos-15-arm64` clean checkout; reproducible OpenCV 4 and pinned HDF5 1.14.6 bootstrap; prepared CenturyArks OpenEB configure/build/install; EBplus build; GTest discovery; full configured CTest; app package; recursive HDF5 closure; Qt framework topology normalization; bundle verifier; and strict local-ad-hoc signature verification. See [M8-B1 validation](macos_milestone_8b1_validation.md). |

The final technical CI was [run 32626325967](https://github.com/yayuqwq/EBplus-MacOS/actions/runs/32626325967), which concluded `success`. Its configured CTest result was:

```text
399 discovered
391 passed
8 skipped
0 failed
```

The eight skips were the ORT/model-off CI-profile cases, not model-runtime
passes. The documentation-change validation was [run
32628955967](https://github.com/yayuqwq/EBplus-MacOS/actions/runs/32628955967),
which also concluded `success`. The two runs confirm the selected macOS
dependency, build/test/package and static closure contract; they are not
interactive GUI, camera, Linux, or distribution evidence.

## Scope refinement: Linux qualification

Earlier M8 wording included native Linux CI as part of a broad cross-platform
regression objective. The project goal still requires preservation of existing
Linux behavior and source paths. That requirement remains an engineering
constraint: macOS-specific packaging and signing logic must remain `APPLE`
scoped, and existing Linux/non-Apple paths must not be intentionally rewritten
without separate authorization.

Native Linux CI is not implemented. The current evidence contains no fresh
native Linux configure, build, CTest, XCB/Wayland/OpenGL GUI runtime, or Linux
CI qualification baseline. A first modern Linux CI result could not reliably
distinguish a macOS-port regression from a historical upstream issue, Linux
dependency/toolchain drift, a missing Linux dependency profile, or an EBplus
defect. Native Linux CI and runtime qualification are therefore **Deferred /
separate future qualification**, not a mandatory completion gate for this macOS
packaging milestone.

This refinement does not say Linux was never part of the broader objective. It
does not establish Linux support, Linux regression protection, Linux CI, or
Linux runtime behavior. Source-level preservation is not Linux validation.

## Evidence boundary and deferred work

The following remain outside this M8 closure and are **Not run**, deferred, or
otherwise unqualified as stated:

- Linux CI; native Linux configure, build, CTest, and installed/runtime
  qualification; and Linux XCB/Wayland/OpenGL GUI runtime.
- Physical CenturyArks camera workflows and M6, which remains `Planned /
  Paused — physical CenturyArks camera unavailable`.
- M7 Slices 5 and 6, which remain blocked by their M6 live-capture and
  live-lifecycle prerequisites.
- Real E2VID neural inference in this CI profile; OpenCV 5 compatibility; and
  HDF5 2 compatibility.
- Developer ID signing, Gatekeeper distribution qualification, notarization,
  DMG, and cross-machine end-user distribution.

Local ad-hoc signing is not Developer ID signing. Local Finder launch is not
Gatekeeper or cross-machine distribution qualification. The bundle verifier is
not notarization. Future distribution work remains optional/as-needed and must
be separately authorized and qualified.

The M8-A and M8-B1 validation documents retain their time-of-record evidence.
Their technical results are unchanged; this document records the later overall
scope decision and current M8 disposition.

## Local disk boundary

The local machine was below the project disk protection line during the final
M8-B1 bring-up and remains unsuitable for a new local large build/package
operation without separate disk authorization. This documentation closure used
the GitHub-hosted runner evidence and did not perform a local build, CTest,
package, GUI, camera, model, download, install, or cleanup. No conclusion is
drawn here about the historical source of the observed local disk-space delta.

## Final disposition

- **M8:** `Complete / Qualified` for documented macOS Apple Silicon packaging
  and reproducibility scope.
- **M8-A:** `Complete / Qualified` for its documented local ad-hoc-signed
  packaged runtime scope.
- **M8-B1:** `Complete / Qualified` for its documented fresh macOS arm64 CI
  foundation scope.
- **Apple Silicon CI:** `Verified`.
- **Linux CI/native runtime:** `Deferred / separate future qualification`;
  `Not run / unverified`.
- **Developer ID, Gatekeeper, notarization, DMG, and cross-machine
  distribution:** `Not run` / future distribution qualification.
