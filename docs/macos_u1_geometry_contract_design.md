# macOS U1 Geometry Contract Design

**Date:** 2026-08-26

**U1 status:** Complete -- static/source audit.

**U1B status:** Complete / Qualified for implementation design.

## Scope and evidence boundary

This is a durable design record for the current fork's coordinate-changing
preprocessing contract. It preserves the U1 source findings and freezes the
U1B implementation baseline; it is not an implementation, build, CTest, GUI,
file-runtime, or physical-camera validation record.

Evidence labels used below:

- **SOURCE-VERIFIED**: current fork source was directly inspected.
- **DESIGN FROZEN**: the maintainer-approved implementation direction for a
  future bounded slice.
- **RUNTIME NOT RUN**: no runtime reproduction or qualification was performed
  by U1/U1B.
- **HARDWARE NOT RUN**: no physical-camera evidence was obtained.

In particular, a source-reachable out-of-bounds pattern is not a reproduced
crash, and a design qualification is not a claim that the design is implemented
or runtime-qualified.

## U1 source findings

### Current transform inventory

**SOURCE-VERIFIED.** The current `FilterChain` has seven stages:

```text
polarity_filter
polarity_invert
flip_x
flip_y
rotate
transpose
rescale
```

`polarity_filter` and `polarity_invert` are value/validity-only stages.
`flip_x` and `flip_y` change coordinates while preserving extent. `rotate`
supports the discrete 0/90/180/270 cases: 0 and 180 preserve extent; 90 and
270 swap extent. `transpose` swaps extent. `rescale` changes coordinate extent.
The existing M7 FilterChain tests provide useful stage-formula evidence on a
non-square synthetic input, but do not establish cross-consumer geometry
propagation; see [M7 Slice 2 File-Source Algorithm
Validation](macos_milestone_7_file_algorithm_validation.md).

### Contract gap

**SOURCE-VERIFIED.** `FilterChain::process(...)` returns events only. A
transformed batch has no immutable, batch-bound output extent, coordinate-space
identity, raw-to-output mapping, or output-to-raw rectangle mapping. Meanwhile,
raw sensor geometry is cached or inferred by display, algorithm, ROI, and
recording consumers.

The resulting source-level risks are:

1. transpose, rotate 90/270, and rescale can change the valid output extent
   without a corresponding consumer-wide geometry update;
2. the file path contains a safe-but-lossy raw-bound drop for events outside
   raw extent after transformation;
3. the live path has a source-reachable non-square transpose out-of-bounds
   indexing pattern;
4. raw-space ROI can be mixed with transformed-event coordinates;
5. processed recording can write transformed events with raw sensor geometry
   metadata; and
6. mapping-only transforms can still invalidate overlays, selections, and any
   coordinate-aware consumer even when width and height do not change.

**RUNTIME NOT RUN.** These are static/source findings. They do not claim a
reproduced crash, a measured event loss, or a qualified runtime behavior.

## Frozen coordinate-space model

**DESIGN FROZEN.** The minimal terminology is:

```text
Raw Source Space
  stable camera-sensor or RAW-file coordinates and source metadata extent
       |
       | FilterChain typed discrete transform plan
       v
Conditioned Output Space
  event coordinates and extent emitted by the active transform plan
       |
       +--> display, algorithm, and XYT consumers use the matching batch geometry
       |
       +--> output ROI is derived only from canonical raw ROI/RONI

Widget / Screen Space
  UI presentation coordinates only; never sensor/conditioned geometry
```

`Raw Source Space` is the stable canonical reference. It includes camera sensor
coordinates, RAW-file recorded coordinates, hardware ROI, source metadata, and
the raw source extent. It does not change because preprocessing is toggled.

`Conditioned Output Space` is the only space in which post-transform events are
interpreted. Algorithm-local rectangles must explicitly identify whether they
are raw or conditioned; they must not be confused with widget/screen geometry.
Widget/screen coordinates are UI-only and are always converted at the UI
boundary.

## Chosen conditioned-geometry contract

**DESIGN FROZEN.** The smallest sufficient output contract is an immutable
geometry snapshot bound to every conditioned batch:

```text
conditioned events
+ conditioned output extent
+ typed discrete raw/output mapping
+ geometry revision
```

This is a conceptual contract, not an existing C++ API. The geometry snapshot
is derived from the immutable raw source extent and active transform plan. It
does not require a generic unlimited geometry framework, arbitrary affine
matrices, or a new upstream pipeline architecture.

The typed mapping scope is deliberately limited to the current transforms:

```text
FlipX, FlipY
Rotate 0/90/180/270
Transpose
Rescale sx/sy
```

Arbitrary-radian rotation is outside the U1 implementation contract. The
mapping is composed in the current fixed stage order, with disabled stages
omitted. Each stage derives the next discrete extent; 90/270 rotation and
transpose swap width and height, while rescale derives output extent using the
existing OpenEB rescale semantics. Edge-coordinate rounding and bounds must be
specified by focused tests rather than silently changed by a new generic
rounding rule.

The mapping provides forward raw-to-output point mapping and conservative
output-to-raw rectangle mapping. A rescale inverse need not be one-to-one:
reverse rectangle mapping must enclose every contributing raw pixel using an
explicit conservative policy. Invalid, zero-sized, overflowing, or otherwise
unrepresentable plans fail closed before any conditioned batch is published.

## ROI decision: R3

**DESIGN FROZEN.** Raw ROI/RONI is the sole mutable source of truth. It is
applied in Raw Source Space before coordinate-changing conditioning. Any output
ROI is derived from the current geometry revision; raw rectangles are never
directly reapplied to already-conditioned events.

Display selection and Auto ROI that originate in Conditioned Output Space must
use conservative output-to-raw rectangle mapping before updating raw ROI.
RONI follows the same raw-space ownership rule. This avoids stale rectangles
when transform mapping or extent changes, including mappings whose extent is
unchanged.

## Consumer contracts

### Display

**DESIGN FROZEN.** Display/frame allocation and overlays consume the batch's
conditioned output extent and geometry revision. A raw source width/height is
not a valid substitute after coordinate-changing conditioning. Geometry
revision changes cause the pipeline owner to recreate or reconfigure derived
frame/renderer state and refresh overlays; widget resizing remains a separate
presentation concern.

### Algorithms and XYT

**DESIGN FROZEN.** Every geometry-aware algorithm and XYT consumer receives
events, extent, and ROI in one matching coordinate space. Extent changes require
the appropriate geometry refresh or safe rebuild. Mapping-only changes also
publish a new geometry revision so coordinate-aware overlays and ROIs cannot
silently retain stale assumptions. Calibration remains a Raw Source Space
consumer unless a future, separately scoped design changes that contract.

### Recording

**CURRENT SOURCE-VERIFIED.** Raw recording remains pre-transform and
source-native. Processed recording may receive coordinate-transformed events
while its writer metadata still uses raw `SensorInfo` dimensions; this creates a
metadata/coordinate mismatch risk. No fail-closed admission enforcement is
currently implemented.

**DESIGN FROZEN / PLANNED.** U1C1 will target fail-closed admission for
processed recording whenever an incompatible non-identity coordinate-transform
plan is active. A conditioned virtual-stream recording contract remains a
separate future `U1R` qualification if retained.

## Mutation, source, and persistence boundary

**CURRENT SOURCE-VERIFIED.** There is no unified conditioned-geometry
propagation contract today. Active `FilterChain` transform enablement and
transform parameters are not part of the persisted M7 configuration contract.

**DESIGN FROZEN.** The frozen contract requires a U1C implementation to derive
a new conditioned geometry revision before it publishes a batch after a
transform or source-geometry change. The owner must propagate that revision
instead of asking each consumer to infer transformed geometry from raw
`SensorInfo`.

On file switch, reconnect, or a source with different geometry, the U1C
implementation shall deterministically re-derive conditioned geometry from the
new raw source geometry before publishing batches. It must validate a transform
plan before publishing its new geometry revision.

**U1 SCOPE FROZEN.** U1C1, U1C2, and U1C3 do not extend the M7 persistence
schema or introduce persistence for `FilterChain` runtime transform state.
Future transform persistence, if requested, requires a separate schema,
migration, and source-switch design scope.

## Frozen invariants

**DESIGN FROZEN.** Future implementation must preserve all of the following:

1. Every geometry-aware consumer receives events with a matching immutable
   geometry revision.
2. No conditioned-stream consumer infers output geometry from raw `SensorInfo`.
3. Raw ROI is never reapplied directly to already-conditioned events.
4. Mapping-only changes are geometry revisions even when extent does not
   change.
5. Source geometry changes deterministically rebuild all derived conditioned
   geometry.
6. Raw recording remains raw and source-native.
7. Processed-recording metadata matches the coordinate extent of the events
   actually written.
8. Invalid or unrepresentable geometry plans fail closed before a conditioned
   batch is published.
9. Calibration remains a raw-source consumer unless separately redesigned.

## U1C implementation decomposition

### U1C1 -- Geometry Core + Fail-Closed Safety Containment

**Status:** Planned / Next.

U1C1 is the smallest implementation slice: conditioned-geometry core,
revision model, typed transform plan, extent derivation, forward mapping,
conservative reverse-rectangle mapping, plan validation, focused unit tests,
and interim safety admission/gating. It must not claim full downstream support
for geometry-changing transforms.

The planned U1C1 safety policy will gate transpose, rotate 90/270, and rescale
until the relevant consumer contracts are migrated. It will fail closed for
coordinate-changing transforms combined with ROI/RONI or processed recording
until their contracts are qualified. Extent-preserving `flip_x`, `flip_y`, and
rotate 180 are not thereby fully qualified; they may remain available only
under restricted legacy conditions while coordinate-space migration is
incomplete.

### U1C2 -- File Consumer Migration

**Status:** Planned.

U1C2 migrates the deterministic file path from raw ROI/RONI through one
conditioned batch with matching output geometry to display, algorithms, and
XYT. Non-square geometry, chained transforms, ROI mapping, output allocation,
algorithm dimensions, and source switching need file-path integration tests.

### U1C3 -- Shared Live Conditioned Batch

**Status:** Planned; hardware runtime qualification partially blocked by M6.

U1C3 replaces independent live display/algorithm transform processing with one
shared conditioned batch fan-out. Unit/source work may proceed without a
camera, but physical-camera runtime evidence remains blocked until the M6
hardware prerequisite is available.

### Later work

`U1C4` is only a possible later closure slice; its scope is to be determined
from U1C1-U1C3 evidence. `U1R -- Conditioned Processed Recording
Qualification` is deferred and separate if the feature is retained. It requires
conditioned EVT2 write/readback, output metadata, bounds, and interoperability
evidence; it is not a U1C1-U1C3 closure dependency.

## Upstream relationship

**UPSTREAM-DIFF-VERIFIED.** Later upstream work demonstrates useful concepts:
single conditioning ownership, a shared conditioned batch/span, and explicit
output geometry. Those concepts may be borrowed semantically.

**DESIGN FROZEN.** Do not import or cherry-pick upstream `StreamConditioner`
architecture as the U1 solution. The current fork should implement the smaller
contract above, preserving the current platform-neutral source path and without
turning this correction into an upstream architecture rewrite.

## Validation ladder for future slices

The future evidence order is static review, focused geometry unit tests,
deterministic file-path integration tests, configured CTest, packaged/file
playback runtime, then physical-camera runtime. Camera-specific evidence remains
hardware-blocked rather than a prerequisite to closing source/file-bound
implementation work. Source-level containment is not runtime qualification.

## Current disposition

No transform is retired by this design record. `polarity_filter` and
`polarity_invert` are preserved. `flip_x`, `flip_y`, rotate, transpose, and
rescale are preserved pending architecture correction and qualification, with
the interim admission boundary above. Future retirement would require separate
evidence about product value, compatibility, migration, and user-facing
behavior; upstream removal alone is not sufficient evidence.
