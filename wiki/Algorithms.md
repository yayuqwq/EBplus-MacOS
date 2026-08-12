# Algorithms

The current `AlgoBridge` registry (`gui/algo_bridge/algo_bridge.cpp`) has **33
entries**:

- **26 self-developed** algorithms under `algo/` (19 Computer Vision + 7
  Analytics).
- **7 OpenEB FilterChain event transforms**: polarity filter/invert, flip X/Y,
  rotate, transpose, and rescale.

Registry inventory is source availability, not an all-algorithm runtime-pass
claim. Algorithms are **mutually exclusive** — enabling one disables the
previous. `sensor_self_test` is a Devices-panel hardware diagnostic and
`intrinsic_calibration` is a **Tools → Intrinsic Wizard** workflow; neither is
a registry entry.

## Display Modes

Each algorithm declares a display mode that controls how its output reaches the screen:

| Mode | Behavior |
|------|----------|
| **Passive** | No direct display output (e.g. filters, analytics that log only) |
| **Overlay** | Draws annotations on top of the live event display (trajectories, vectors, boxes) |
| **Replace** | Replaces the event display with the algorithm's own frame |
| **Standalone** | Opens a separate `AlgoWindow` with its own output canvas |

## Shared Preprocessing

Self-developed algorithms use shared preprocessing controls. Unified ROI/RONI
is a separate camera state: a live source uses hardware `I_ROI`/RONI, while a
file source uses software crop/RONI. It is not an eighth FilterChain stage and
file-source ROI evidence is not hardware-facility evidence.

The shared noise controls have nine modes; the processing contract and each
mode's numerical/lifecycle behavior require separate validation.

### Noise Filter Modes

Implemented in `algo/cv/noise_filter.h`. The GUI exposes parameters based on the selected mode:

| Mode | Description |
|------|-------------|
| BAF | Background Activity Filter |
| STCF | Spatio-Temporal Correlation Filter |
| Refractory | Refractory period filter |
| DWF | Directional Weighted Filter |
| AgePolarity | Age + polarity filter |
| Harmonic | Harmonic mean filter |
| Repetitious | Repetitious event filter |
| SpatialBP | Spatial Band-Pass filter |
| KNoise | KNoise filter |

## Self-Developed Algorithms

### Computer Vision (19)

| Algorithm | Display | Notes |
|-----------|---------|-------|
| Hot Pixel Filter | Passive | FPN correction option |
| Orientation Filter | Overlay | jAER SimpleOrientationFilter (min-dt WTA) |
| Direction Selective Filter | Overlay | jAER DirectionSelectiveFilter |
| Sparse Optical Flow | Overlay | 4 modes: LocalPlanes / LucasKanade / BlockMatch / ClusterOF |
| Blob Detector | Overlay | |
| Object Tracker | Overlay | 4 modes: RCT / Median / Kalman / MultiHypothesis |
| Corner Detector | Overlay | 4 modes: EndStopped / TypeCoincidence / Harris / Arc |
| Line Segment (ELiSeD) | Overlay | |
| Hough Line Tracker | Overlay | jAER HoughLineTracker |
| Hough Circle Tracker | Overlay | jAER HoughCircleTracker |
| Orientation Cluster | Overlay | |
| Cluster LIF | Overlay | LIF neuron clustering |
| Background Mask Filter | Replace | 2D histogram background modeling |
| Trigger Synced Filter | Passive | |
| Bandpass Filter | Overlay | |
| EIS (Optical Gyro) | Overlay | Electronic image stabilization |
| XYT 3D Visualizer | Standalone | GPU 3D point cloud |
| Overlay | Overlay | Generic overlay |
| Time Surface | Standalone | Gray / Hot / Plasma / Turbo palettes |

### Analytics (7)

| Algorithm | Display | Notes |
|-----------|---------|-------|
| Active Marker Tracking | Overlay | Sliding-window clustering |
| Event -> Video (E2VID) | Standalone | 3 modes (see below) |
| Flow Statistics | Passive | Requires ground-truth |
| ISI Analyzer | Standalone | Inter-spike interval histograms |
| Particle Counter | Overlay | Line-crossing counter |
| Auto Bias Controller | Overlay | Closed-loop bias tuning |
| Frequency Detector | Standalone | Blinking frequency detection |

### Non-registry workflows

- **Sensor Self-Test** — Devices-panel hardware diagnostic; it requires a
  separately authorized physical-device scope.
- **Intrinsic Wizard** — Tools workflow using the current asymmetric circle
  grid and manual capture; it is not a registry algorithm.

## Event-to-Video (E2VID)

The Event-to-Video algorithm reconstructs grayscale intensity images from raw event streams. It has **3 modes**, selected via the `mode` parameter:

| Mode | Default | Description |
|------|---------|-------------|
| `0 = BardowVariational` | | Non-DL; joint optical-flow + log-intensity variational optimization |
| `1 = InteractingMaps` | | Non-DL; six interconnected maps (I/G/V/F/C/R) with rotation estimation |
| `2 = E2VID` | ✅ | DL; ONNX Runtime neural-network inference |

**Common parameters** (modes 0, 1): `output_fps` (1–120, default 30), `window_ms`, `decay_tau_ms` (0–5000, default 500).

### E2VID runtime/model boundary

The repository tracks model-conversion source but no ready-to-run model or
complete portable ONNX Runtime pair. `num_bins` is auto-determined by the
ONNX model's input channel count when a model is loaded. A compatible runtime/
model pair and a dedicated test are required before claiming real ONNX
inference; the configured heuristic fallback is not that claim.

E2VID parameters exposed in the GUI: model path, `num_bins`, auto-HDR, unsharp amount/sigma, bilateral sigma.

> **Without ONNX Runtime**: E2VID falls back to a heuristic mode (voxel-grid sum + sigmoid). BardowVariational and InteractingMaps work without any extra setup.

### BardowVariational (mode 0)

Joint estimation of optical flow `u` and log-brightness `L` via Chambolle-Pock primal-dual optimization. Uses all six regularization terms:
- `lambda1` (flow TV), `lambda2` (temporal smoothness), `lambda3` (intensity TV)
- `lambda4` (flow constraint), `lambda5` (no-event dead zone), `lambda6` (prior map)

`lambda3` and `num_iterations` are shared with InteractingMaps.

### InteractingMaps (mode 1)

Six interconnected maps updated by alternating relaxation:
- **I** intensity, **G** gradient (= ∇I), **V** time-varying (−V = F·G), **F** optical flow, **C** calibration, **R** rotation (estimated via linear least squares).

V values are clamped to [-1, 1] to prevent NaN divergence. `I_map_` is reinitialized from Vc every frame to prevent ghosting.

## OpenEB FilterChain transforms (7)

The current registry keeps seven event transforms under `openeb_filter`:
polarity filter, polarity invert, flip X, flip Y, rotate, transpose and
rescale. They are controlled by the preprocessing UI. Unified ROI/RONI is
separate; older frame/preprocessor/utility wrapper catalogs are not current
registry entries.

## Adding a New Algorithm

1. Implement the algorithm in `algo/cv/` or `algo/analytics/` (header-only, operate on event packets).
2. Register it in `AlgoBridge::register_self_cv()` or `register_self_analytics()` with `AlgoInfo` (name, display name, category, display mode, parameters).
3. Implement the backend in `gui/algo_bridge/backends/` (e.g. `cv_backends.cpp`) — wire `set_param` / `get_param` / `process`.
4. If the backend has a factory entry, register it in `backend_registry.h`.
5. The algorithm auto-appears in the Algorithms panel; parameters are read/written via `set_param` / `get_param`.

All algorithms must have both `set_param` and `get_param` implementations so the GUI can read and write values.
