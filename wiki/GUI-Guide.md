# GUI Guide

EB plus uses a VSCode-style layout: a custom title bar with dropdown menus, a left sidebar (ActivityBar + stacked panels), a central OpenGL display, and a right-side algorithm display area. There is no classic menu bar or toolbar — all configuration lives in the sidebar.

## Title Bar

The `CustomTitleBar` (36 px tall) shows the "EB plus" chip on the left, followed by dropdown menus:

| Menu | Contents |
|------|----------|
| **File** | Open File, Open Recent, Save/Load Camera Config, Save/Load Biases, Save/Load Algo Params, Exit |
| **View** | Toggle Playback Panel, Reset/Save/Load Layout, Fullscreen |
| **Theme** | Color submenu (5 colors) + Mode submenu (Follow System / Light / Dark) |
| **Tools** | Intrinsic Wizard (calibration), Focus Assistant |
| **Help** | About, About Qt |

Window control buttons (minimize / maximize / close) are on the right. The title bar follows the active theme.

## Sidebar (ActivityBar)

The left sidebar is a 48 px icon column (`ActivityBar`) that switches between 5 panel groups via `QStackedWidget`. Each group hosts one or more panels in a scrollable page.

| Group | Icon | Tooltip | Panels |
|-------|------|---------|--------|
| **Camera** | `camera` | Camera devices and connection info | Devices, Information |
| **Display & Stats** | `chart` | Display settings and statistics | Display, Statistics |
| **Hardware** | `cpu` | Biases, ROI, ESP and trigger configuration | Biases, ROI, ESP, Trigger |
| **Algorithms** | `blocks` | Algorithm selection and preprocessing | Preprocessing, Algorithms |
| **Tools** | `tools` | File conversion and tools | File Tools |

- The sidebar can be collapsed to 48 px (icon-only) or expanded to the default 380 px.
- Drag the blank area of the ActivityBar to resize (cursor feedback: open hand → closed hand).
- "Reset Layout" expands the sidebar if it was collapsed.

## Panels (11 total)

### Camera group
- **Devices** — camera discovery, connect/disconnect, refresh. Shows available cameras and connection status.
- **Information** — sensor metadata: model, resolution, serial number, firmware version, generation.

### Display & Stats group
- **Display** — accumulation time (1–1 000 000 us, exponential slider + spinbox), frame rate (1–60 fps), FPS limit (1–1000), color palette (Dark / Light / CoolWarm / Gray).
- **Statistics** — live event rate, peak rate, ON/OFF ratio, FPS, timestamp, and (in online camera mode) per-algorithm event drop rate.

### Hardware group
- **Biases** — dynamically enumerates all HAL LL-biases; slider + spinbox + reset per bias; save/load `.bias` files.
- **ROI** — unified ROI/RONI. A live camera uses hardware `I_ROI`/RONI; file
  playback uses software crop/RONI. The current UI provides Enable ROI and
  ROI Settings rather than a legacy multi-rectangle filter stage.
- **ESP** — Anti-Flicker (mode / band / presets / duty cycle / threshold), Trail Filter (type / threshold), ERC (target event rate).
- **Trigger** — Trigger In (per-channel enable) + Trigger Out (enable / period / duty cycle).

Facility-backed Biases, ESP and Trigger controls degrade when their facility is
unavailable. Unified ROI remains software-backed during file playback.

### Algorithms and Tools groups
- **Preprocessing** — polarity/invert/geometry transforms for the display path.
- **Algorithms** — current registry selection, shared preprocessing controls and
  per-algorithm parameters. See [Algorithms](Algorithms.md).
- **File Tools** — live recording, file cutter, event-file conversion and
  source-event export workflows.

## Display

The central `EventDisplayWidget` is an OpenGL 3.3 core-profile widget with a
letterboxed viewport (preserves aspect ratio). Current display controls expose
accumulation time, frame rate/FPS limit and the Dark, Light, CoolWarm and Gray
palettes. Historical OpenEB frame-wrapper registrations are not current
algorithm-registry entries.

The display also supports overlays drawn by algorithms (bounding boxes,
trajectories, vectors and arrows) via `FrameAnnotator`.

## Preprocessing Filter Chain

Thread-safe pipeline of 7 stackable event transforms, toggled from the
Preprocessing panel. Applied in order:

1. Polarity Filter (OFF / ON)
2. Polarity Invert
3. Flip X
4. Flip Y
5. Rotate (0 / 90 / 180 / 270)
6. Transpose
7. Rescale (Scale X, Scale Y)
Unified ROI/RONI is not a FilterChain stage. It is a separate state: software
crop/RONI for files and hardware `I_ROI`/RONI for live cameras. The precise
algorithm-path and coordinate/numerical behavior requires dedicated validation.

## Tools Menu

The **Tools** dropdown menu (in the custom title bar) hosts calibration and
focus utilities.

### Intrinsic Wizard

A dialog for asymmetric Zhou circle-grid calibration from CD events. After a
connected, running camera provides events, **Space** or **Capture** takes a
recent 5,000 us event window. Accepted captures are processed against the
configured asymmetric grid; after the configured target (default 15), the
workflow can solve and export YAML. This is a physical-camera/calibration
workflow, not a registry algorithm and not yet a runtime-validation claim.

### Focus Assistant

The current focus tool presents a rotating Siemens-star target for visual focus
assistance. It is distinct from calibration and does not provide a numerical
sharpness validation claim.

## Recording & Playback

- **RAW recording** — record live camera streams to `.raw` with real-time buffer flushing.
- **Playback** — open `.raw`, `.hdf5`, `.h5`, or `.dat` file sources; speed
  control, pause/resume, immediate seek rendering, loop, EOF restart and
  position tracking are wired into the file playback path.
- **Loop playback** — cyclic playback; temporal algorithm state is reset on
  loop/seek/source transitions. This does not substitute for numerical or
  all-algorithm validation.
- **File cutter** — extract a time range from an event file.

The playback dock can be toggled with `Ctrl+Shift+P`.

## Export & Conversion

Available from the File Tools panel:

- **Format conversion**: event-file source to HDF5 or CSV, plus RAW clip.
- **Export**: source-event HDF5 or AVI. AVI uses
  `PeriodicFrameGenerationAlgorithm` plus direct synchronous `cv::VideoWriter`.

HDF5 source-event export must not be described as general algorithm-result
export.

## Theming

- **5 background colors**: Gray, Green, Yellow, Pink, Blue (default).
- **3 modes**: Follow System (default, requires Qt 6.5+), Always Light, Always Dark.
- Dark mode uses a **dark variant of the chosen color** — not pure black.
- Text color auto-adjusts (black on light, white on dark).
- The "EB plus" title chip uses inverse colors relative to the background for high contrast.
- Settings persist across restarts (`QSettings`).
- Theme changes apply immediately (style unpolish/polish).

## Multi-Window

- **XYT 3D point cloud** — GPU-accelerated 3D event visualization (`SpaceTimeDisplay`, VBO + GLSL).
- **Algorithm display windows** — `AlgoWindow` dockable windows showing algorithm title + output only (no parameters — those live in the sidebar).
- **Layout persistence** — save/restore dock geometry and window positions to JSON (View → Save/Load Layout).

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Ctrl+O` | Open file |
| `Ctrl+Shift+P` | Toggle playback panel |
| `F11` | Fullscreen |
| `Ctrl+Q` | Quit |

## Configuration Files

- **`.bias`** — camera bias presets (save/load from File menu or Biases panel).
- **Algo params JSON** — per-algorithm parameter snapshots (File → Save/Load Algo Params).
- **Layout JSON** — dock/window geometry (View → Save/Load Layout).
- **QSettings** — theme color/mode, sidebar state, recent files.
