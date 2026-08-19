# macOS Milestone 7 plain ONNX E2VID disposition

## Status and scope

**Status:** `Deferred / Optional — implemented in source but not qualified`.

The current source retains the generic plain single-input/single-output ONNX
E2VID compatibility path. The generic GUI model-path wiring can reach that
path, but no plain/recurrent architecture selector, plain-specific user
workflow, or plain runtime qualification is recorded.

A read-only audit found no authoritative plain checkpoint or ONNX fixture, no
frozen-upstream plain workflow/runtime evidence, and no historical product
fixture. The official recurrent checkpoint and its derived recurrent ONNX are
not plain-model evidence and must not be used to claim plain qualification.

By maintainer scope decision, plain ONNX is a generic/optional compatibility
path rather than a separately required M7 Slice 3 product capability. It
remains implemented but unqualified. If a trusted plain fixture or an actual
product requirement emerges, reopen a bounded plain-model qualification with
independently recorded model provenance and runtime evidence.

## Slice disposition

M7 Slice 3 is `Complete / Qualified within the maintainer-accepted scope`:
the Slice 3A heuristic fallback and the real recurrent-model
conversion/inference/state/reset/RAW seek-loop sub-phases are qualified. This
is a scope decision, not new plain runtime evidence.

M7 remains `In progress`: Slice 5 awaits the M6 live-capture prerequisite, and
Slice 6 awaits the M6 live-lifecycle prerequisite. M6 remains `Planned /
Paused — physical CenturyArks camera currently unavailable`; Linux remains
`Not run / unverified`.

This disposition supersedes only the current Slice 3 closure interpretation of
[M7 recurrent E2VID validation](macos_milestone_7_e2vid_recurrent_validation.md);
it does not alter or expand that report's recurrent runtime evidence.
