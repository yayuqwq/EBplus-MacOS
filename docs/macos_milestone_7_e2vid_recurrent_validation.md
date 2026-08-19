# macOS Milestone 7 recurrent E2VID validation

## Status and scope

**Status:** Complete / Qualified for the real recurrent-model and conversion
sub-phase of M7 Slice 3. M7 Slice 3 and Milestone 7 overall remain **In
progress** because plain ONNX E2VID inference has not been qualified.

This report records a bounded macOS Apple Silicon qualification on
<code>feat/macos-e2vid-model-conversion</code> above base
<code>83f26a6080c6f31a0366dc3b81cd9e0a19807b92</code>. It covers restricted
checkpoint conversion, recurrent ONNX structure, real arm64 C++ ONNX Runtime
inference/state/reset, tracked RAW seek/loop lifecycle evidence, one bounded
Cocoa observation, and fresh full CTest runs with and without a local model.

It does not qualify a plain ONNX model, neural image quality, long-duration
performance, live-camera inference, Linux behavior, application-bundle
closure, or all ONNX Runtime installations.

## Fixed local inputs and retention boundary

| Item | Actual identity |
| --- | --- |
| Official checkpoint | UZH/RPG E2VID_lightweight.pth.tar from https://download.ifi.uzh.ch/rpg/web/data/E2VID/models/E2VID_lightweight.pth.tar |
| Checkpoint local location | $REPO_ROOT/.downloads/e2vid/E2VID_lightweight.pth.tar |
| Checkpoint identity | regular, non-symlink, 42,878,232 bytes, SHA-256 <code>4cfeb2c850bf48fc9fa907e969cb8a04e3c51314da2d65bdb81145ac96574128</code> |
| RAW fixture | tracked algo/tests/sparklers.raw; EVT2, 640x480, 521,252 CD events, 0..95,871 us, SHA-256 <code>e84afbecdc07d2910ae846a4ae0ee246f5b9c97a53816c637d4f85c023d7c234</code> |
| Generated ONNX | $REPO_ROOT/.artifacts/m7-slice3d2e/E2VID_lightweight_recurrent.onnx |
| ONNX identity | regular, non-symlink, 42,871,228 bytes, SHA-256 <code>7f59e4c0bc2887b68ec1bd1f9bc87a42ea67f4836757c79bb9fb1ffe657114d0</code> |
| Pinned conversion source | git@github.com:uzh-rpg/rpg_e2vid.git at <code>d0a7c005f460f2422f2a4bf605f70820ea7a1e5f</code>, GPL-3.0 source licence |

The checkpoint's independent redistribution licence remains unresolved. The
GPL-3.0 source licence must not be inferred to license the checkpoint or the
derived ONNX. Both remain ignored, local qualification artifacts only: they
are not tracked, committed, packaged, released, or redistributed. Their SHA
values are integrity anchors; a same-named file with another hash is a
different input.

## Restricted conversion and generated graph

The repository-local conversion environment was CPython 3.13.8 arm64, PyTorch
2.10.0, NumPy 2.1.0, ONNX 1.18.0, and protobuf 5.29.5 in
$REPO_ROOT/.venv/e2vid-convert-patched/.

The only functional converter repair in
models/convert_to_onnx.py replaces unrestricted
<code>weights_only=False</code> loading with:

~~~
torch.load(path_to_model, map_location="cpu", weights_only=True)
~~~

No fallback to unrestricted loading, pickle loading, or automatic
safe-globals allowlisting was used. The restricted checkpoint data supplied
the architecture, model configuration, and state dictionary; the pinned
source class was constructed and its normal strict load_state_dict contract
was used.

The actual model is E2VIDRecurrent: 5 bins, 3 encoders, and ConvLSTM
recurrent state. The exporter rejects any encoder count other than 3 rather
than silently changing the state interface. The converter completed
onnx.load and onnx.checker.check_model.

| Generated graph property | Actual result |
| --- | --- |
| IR / opset | IR v8 / ai.onnx opset 17 |
| Nodes / initializers | 126 / 39 |
| External data | none |
| Inputs / outputs | 7 / 7 |

Inputs are <code>event_tensor [batch,5,H,W]</code>,
<code>h0/c0 [batch,64,H2,W2]</code>,
<code>h1/c1 [batch,128,H4,W4]</code>, and
<code>h2/c2 [batch,256,H8,W8]</code>. Outputs are
<code>image [batch,1,H,W]</code> and
<code>h0_new/c0_new</code>, <code>h1_new/c1_new</code>, and
<code>h2_new/c2_new</code>, with matching dynamic state spatial dimensions.
The exporter has symbolic output-channel metadata for the six state outputs;
the input state channel dimensions are explicit. Dynamic batch, image H/W,
and state spatial dimensions are deliberate model contract.

## Real C++ ONNX Runtime inference and reset

The build configuration is ONNX Runtime mode ON with the repository-local
1.29.0 arm64 prefix at
$REPO_ROOT/.deps/onnxruntime-1.29.0-macos-arm64/. Real-model tests construct
E2VIDInference with a non-model bin count, load the ONNX, then require
model-loaded state and model-authoritative 5 bins. Direct neural outputs are
finite, sensor-sized CV_32FC1 frames, not the CV_8UC1 heuristic fallback type.

The fixed A/B state test recorded:

~~~
B_stateful versus B_zero NORM_INF = 0.121709
A replay NORM_INF = 0
B replay NORM_INF = 0
replay tolerance = 1e-5
~~~

This establishes zero-state first inference, a nonzero recurrent-state effect,
reset clearing that state, and deterministic replay for this model and
environment. It is not an image-quality claim. The dynamic-shape downsample
smoke also passed a 62x46 input, produced a finite padded 64x48 neural result,
and cropped it to the sensor dimensions. EventToVideoTest.RealModelSmoke
passed the GUI-facing EventToVideo integration.

## Tracked RAW, seek, loop, and automated mode switch

With an explicit model, full CTest executed and passed:

- three bounded 33,333-us real neural RAW windows;
- RealModelTrackedRawPlayback.ProducesSequentialNeuralFrames;
- RealModelSeekResetReplay.ActualSeekSignalRestoresZeroStateOutput;
- RealModelLoopResetReplay.ActualLoopSignalRestoresFirstWindows;
- RealModelModeSwitchReset.ReturningToE2VIDClearsRecurrentState; its
  mode-switch replay NORM_INF was 0 with tolerance 1; and
- the direct real-model state/reset and downsample tests.

The harness uses real FramePipeline and FileFrameGenerator signals, then
mirrors the MainWindow reset body. Source inspection separately confirms:

~~~
file_seeked -> MainWindow resets all live algorithm instances
file_looped -> MainWindow resets all live algorithm instances
~~~

Therefore this is production-equivalent signal/replay evidence plus
source-inspected MainWindow wiring; it does not instantiate MainWindow or
claim an automatically measured MainWindow numerical replay. The tests capture
stderr where needed and fail on ONNX inference failure or heuristic fallback.

## Bounded Cocoa observation

One bounded Cocoa session used the current arm64 build-tree binary, the
tracked RAW, and the ignored recurrent ONNX. The maintainer observed:

- a right-side Event -> Video (E2VID) picture;
- continuing output updates after an actual seek;
- Loop continuing without freezing;
- no error; and
- normal Cmd+Q shutdown with GUI exit code 0.

The fresh session log contained only a Qt missing-font advisory, one
window-move diagnostic, and macOS IMK/CapsLock diagnostics. The fatal scan
found no SIGSEGV, SIGABRT, dyld, fatal, ONNX inference failure, or heuristic
fallback marker. This is Cocoa UI wiring/lifecycle evidence only. It does not
claim image quality and does not record a human recurrent-model mode-switch
observation; the recurrent mode-switch reset evidence above is automated only.

## Final build, CTest, and Mach-O provenance

The existing $REPO_ROOT/.build/ebplus-macos tree was incrementally rebuilt;
no second build tree was created. Fresh full CTest ran twice:

| Run | Environment | Result |
| --- | --- | --- |
| Default checkout | EBPLUS_E2VID_TEST_MODEL unset | 395 discovered, 387 passed, 8 clean skipped, 0 failed; 21.34 s |
| Real-model qualification | EBPLUS_E2VID_TEST_MODEL explicitly names the ignored recurrent ONNX | 395 discovered, 395 passed, 0 skipped, 0 failed; 23.73 s |

The eight default-checkout skips are deliberately model-gated: four GUI RAW
lifecycle tests, EventToVideoTest.RealModelSmoke, and three direct
E2VIDInference real-model tests. They are not real-model evidence. With the
environment supplied, all eight executed and passed.

The GUI and checked test binaries are arm64 Mach-O. Their ONNX Runtime
dependency is version 1.29.0 and their Metavision libraries report 5.2.0.
Their RPATHs include the repository-local OpenEB 5.2 and ONNX Runtime
prefixes. The checked targets have no /usr/local OpenEB 5.1.1 or x86_64
reference. Homebrew Qt/OpenCV/Boost dependencies remain build-tree
dependencies; this is not standalone application-bundle closure evidence.

## Closure and remaining work

| Scope | Status |
| --- | --- |
| M7 Slice 3A heuristic fallback | Complete / Qualified |
| Real recurrent model plus restricted conversion sub-phase | Complete / Qualified |
| MOD-003 recurrent ONNX E2VID state | Verified |
| MOD-004 RPG E2VID model conversion | Verified |
| MOD-002 plain ONNX E2VID inference | Not started / not qualified |
| M7 Slice 3 overall / M7 | In progress / In progress |
| M6 | Planned / Paused — physical CenturyArks camera currently unavailable |
| Linux | Not run / unverified |

No authoritative plain-model fixture was acquired or qualified. A plain-model
claim must not be synthesized from this recurrent-model evidence.
