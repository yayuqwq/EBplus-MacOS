# macOS Time Surface Algorithm Validation

## Status

**Passed -- bounded macOS arm64 build-tree Time Surface algorithm lifecycle
smoke for one RAW fixture with default parameters.**

Milestone 5 remains `Planned`.

## Baseline and scope

- Baseline: `main` at `9f828ada53da7dcfe4653176e0ab17b596ed244a`.
- Input: tracked `algo/tests/sparklers.raw`.
- Platform: macOS Apple Silicon arm64, build-tree GUI, repository-local OpenEB
  5.2 CenturyArks runtime.
- The actual GUI entry was `Settings -> Algorithms -> Computer Vision -> Time
  Surface`.
- `time_surface` has no model, physical-camera, or output-file dependency. It
  consumes file-source CD events and displays a standalone frame result.
- The incremental `gui_for_openeb` build passed. The binary was arm64 with UUID
  `0B5B4907-E3F8-305E-9941-13D35E4A26BD`.
- The Terminal wrapper PID was `48970`; the direct GUI child PID was `48988`.
  The GUI exit code was `0`.

## Passed facets

- RAW baseline playback opened without an error dialog, showed a non-empty
  changing display, and autoplayed with usable duration and position.
- Time Surface enabled with default parameters and showed visibly distinct,
  non-empty dynamic output from the base event display.
- Pause stopped progression; resume restored dynamic Time Surface output.
- One forward seek and one backward seek both recovered to responsive, dynamic
  output.
- Reopening the same source retained a usable enabled algorithm without an
  error dialog, hang, or incorrect retained state.
- The GUI closed normally with exit code `0`.
- The runner log scan found no `SIGSEGV`, `SIGABRT`, `EXC_BAD_ACCESS`, uncaught
  exception, `Device unavailable`, `102113`, Qt fatal, dyld fatal, or Time
  Surface/algorithm error marker.

## Observed limitations

- Several transient white frames could appear in the Time Surface window after
  either seek before dynamic output recovered.
- One non-fatal Cocoa/IMK mach-port warning was recorded.
- This was one RAW fixture with default parameters and qualitative behavior
  only; it provides no numerical-correctness evidence.

The transient white frames did not hang or crash the application and dynamic
algorithm output recovered, but exact reset latency and frame correctness were
not measured.

## Not run

- Other algorithms and algorithm parameter coverage.
- Model-backed algorithms.
- Algorithm-result export.
- Physical-camera workflows.
- AVI workflows.
- Long-duration stability testing.
- Linux compilation and runtime comparison.

This result does not establish algorithm numerical correctness, all-algorithm
coverage, model support, exported algorithm-output behavior, long-term
stability, or Linux parity.
