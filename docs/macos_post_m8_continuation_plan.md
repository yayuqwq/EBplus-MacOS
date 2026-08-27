# macOS Post-M8 Continuation Plan

**Date:** 2026-08-26

**Status:** active maintenance roadmap after M8 closure; no new numbered
milestone is defined here.

## Scope and evidence boundary

M8 remains **Complete / Qualified** for the documented macOS Apple Silicon
packaging and reproducibility scope recorded in [macOS Milestone 8
Validation](macos_milestone_8_validation.md). That closure is not reopened by
later source-level findings in an optional coordinate-transform pipeline:
packaging/reproducibility qualification and coordinate-transform runtime
correctness are separate evidence scopes.

This continuation is limited to:

1. selective reconciliation of later upstream changes;
2. correction of source-verified correctness risks; and
3. targeted macOS GUI/native-experience work after the geometry work.

It is not a formal-distribution effort, a new `M9`, Linux qualification, a
broad algorithm rewrite, GUI redesign, bulk upstream merge, or completion of
M6/M7 hardware-dependent work.

Status terms in this document are evidence-bound. `Complete -- read-only` and
`Complete -- static/source audit` do not imply a build, CTest, GUI runtime, or
physical-camera result. `Qualified for implementation design` means only that
the design boundary is frozen for a future implementation slice.

## Completed reconnaissance and design work

| Item | Status | Durable result |
| --- | --- | --- |
| Post-M8 Upstream Reconnaissance | Complete -- read-only audit | Later upstream changes were reviewed without integrating code. |
| U1 -- Coordinate Transform Geometry-Safety Audit | Complete -- static/source audit | The current coordinate-changing FilterChain stages do not have a complete, consistent geometry contract. |
| U1B -- Geometry Contract Design Freeze | Complete / Qualified for implementation design | The smallest sufficient conditioned-geometry contract and staged implementation boundary are recorded in [macOS U1 Geometry Contract Design](macos_u1_geometry_contract_design.md). |

### Upstream reconciliation status

The historical integrated upstream baseline is
`f72fdf750ab82c09eb1d11ba828a4ac0601a2ea9`; its integration evidence remains
in [macOS frozen upstream baseline integration
validation](macos_upstream_baseline_integration_validation.md). During the
2026-08-26 audit, the observed upstream head was
`45cc1f660e84d36c6bd27ffbb9e5a9f1f3e1264b`.

The audit measured 104 upstream-only commits after `f72fdf7`. That is a dated
audit snapshot, not a statement about the permanent or current commit count.
No later upstream code has been integrated by this record. The fork and
upstream have meaningful semantic divergence, so future adoption should be
evaluated by semantic slice rather than a broad merge by default. A broader
integration is not permanently forbidden; it requires fresh evidence and its
own bounded decision.

The reconnaissance backlog includes coordinate-transform correctness,
conditioning/ROI/config architecture, frame modes, dense-flow and frequency
features, calibration redesign, Auto Bias, and small targeted fixes. Listing
these items does not mean the fork lacks all equivalent behavior merely because
an upstream commit is outside its ancestry.

## Current main lane

```text
M8 closure
  Complete / Qualified for documented macOS Apple Silicon
  packaging and reproducibility scope
       |
       v
Post-M8 upstream reconnaissance
  Complete -- read-only audit
       |
       v
U1 geometry-safety audit
  Complete -- static/source audit
       |
       v
U1B geometry contract design freeze
  Complete / Qualified for implementation design
       |
       v
U1C1 Geometry Core + Fail-Closed Safety Containment
  Planned / Next
       |
       v
U1C2 File Consumer Migration
  Planned
       |
       v
U1C3 Shared Live Conditioned Batch
  Planned
       |
       v
G1 GUI Defect Inventory
  Planned
       |
       v
G2 Native macOS Window Chrome
  Planned
       |
       v
G3 Liquid Glass Feasibility
  Planned / research only
```

### U1C1 -- Geometry Core + Fail-Closed Safety Containment

**Status:** Planned / Next.

U1C1 is intentionally limited to a geometry core, revision model, typed
transform plan, output-extent derivation, forward mapping, conservative
reverse-rectangle mapping, plan validation, focused unit tests, and interim
safety admission/gating. It does not claim that all downstream consumers are
already ready for geometry-changing transforms.

The planned U1C1 admission policy is fail closed: it will reject activation
where the application cannot yet represent the transform coherently through
downstream consumers, rather than silently dropping otherwise-valid events.
The U1C1 target is to gate transpose, rotate 90/270, and rescale until geometry
consumer migration; to fail closed for coordinate-changing transforms combined
with ROI/RONI until coordinate-space propagation is implemented; and to fail
closed for incompatible non-identity coordinate plans with processed recording
until recording semantics are qualified. Extent-preserving legacy behavior for
`flip_x`, `flip_y`, and rotate 180 may remain available under restricted
conditions, but is not fully qualified until coordinate-space consumers have
migrated.

### U1C2 -- File Consumer Migration

**Status:** Planned.

U1C2 will establish a deterministic file-path contract:

```text
file source -> raw ROI/RONI -> one conditioned batch
            -> matching output geometry -> display, algorithms, and XYT
```

File work is the first consumer migration because it has no physical-camera
dependency and supports deterministic non-square integration evidence.

### U1C3 -- Shared Live Conditioned Batch

**Status:** Planned; physical-camera runtime qualification is partially blocked
by M6.

U1C3 will converge the current live display and algorithm paths on one shared
conditioned batch fan-out. Source, unit, and potentially file-backed work may
be possible without a camera; physical-camera qualification remains a separate
M6-linked evidence need.

`U1C4`, if ever needed, is only a possible later closure slice. Its scope must
be determined from U1C1-U1C3 evidence rather than pre-expanded now.

`U1R -- Conditioned Processed Recording Qualification` is deferred as a
separate qualification if the feature is retained. It must cover conditioned
EVT2 writing, readback, metadata, event bounds, and interoperability; it is
not a closure dependency for U1C1-U1C3.

## GUI continuation after geometry correctness

| Item | Status | Boundary |
| --- | --- | --- |
| G1 -- GUI Defect Inventory | Planned | Establish actual versus expected, functional versus cosmetic, and macOS-specific versus upstream-overlap findings before changing UI behavior. |
| G2 -- Native macOS Window Chrome | Planned after geometry correctness work | Investigate the current `Qt::FramelessWindowHint`, `CustomTitleBar`, and custom resize grips. Any Apple-scoped integration should use real macOS traffic lights rather than painted decorative circles, while retaining the Linux source/default-window path. |
| G3 -- Liquid Glass Feasibility | Planned / research only | Evaluate Qt Widgets/AppKit boundaries, native-titlebar prerequisites, QOpenGLWidget composition, availability/fallback, and a possible Objective-C++ bridge. This is not a commitment to rewrite the GUI in SwiftUI, AppKit, or QML. |

## Independent deferred, paused, and blocked branches

| Branch | Status | Current boundary |
| --- | --- | --- |
| M6 live camera parity | Paused | A physical CenturyArks camera is unavailable. |
| M7 Slice 5 and Slice 6 | Blocked by M6 | Calibration and processed-recording work remain dependent on the applicable live-camera evidence. |
| Linux Requalification | Deferred / not started | A future native Linux effort requires a clean/current clone, dependency baseline audit, configure, build, CTest, XCB/Wayland/OpenGL GUI runtime, and feature/runtime qualification. Linux source preservation is not Linux qualification. |
| Formal macOS distribution | Deferred by maintainer | Developer ID, Gatekeeper cross-machine qualification, notarization, DMG, and formal end-user distribution are not run. Local ad-hoc signing is not distribution qualification. |
| OpenCV 5 and HDF5 2 | Unverified | They remain outside the qualified dependency profile. |

These branches do not belong in the main continuation lane. M6/M7 resume only
when the hardware prerequisite is available; Linux requalification is a
separate environment-specific effort; and formal distribution resumes only by
maintainer decision.

## References

- [macOS porting plan](macos_porting_plan.md)
- [macOS Milestone 8 Validation](macos_milestone_8_validation.md)
- [macOS frozen upstream baseline integration validation](macos_upstream_baseline_integration_validation.md)
- [macOS U1 Geometry Contract Design](macos_u1_geometry_contract_design.md)
- [platform parity matrix](platform_parity_matrix.md)
