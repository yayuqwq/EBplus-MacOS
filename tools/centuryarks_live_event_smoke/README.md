# CenturyArks bounded live-event smoke

`centuryarks_live_event_smoke` is a repository-owned, headless OpenEB 5.2 probe. It opens one explicitly selected runtime identity, counts decoded CD events for a bounded duration, stops the camera, removes its callbacks, and emits one JSON result. It does not record event data or access camera configuration facilities.

## Command line

```text
centuryarks_live_event_smoke \
  --serial <full-runtime-selector> \
  [--duration-ms <100..5000>] \
  [--min-events <non-negative-integer>]
```

The tool default is 3000 ms and one event. A full runtime selector with exactly three non-empty `integrator:plugin:serial` fields is mandatory; there is no first-device fallback. The selector is passed directly to `Metavision::Camera::from_serial` and is never printed by the tool, including in its exception diagnostics or JSON result.

`--help` prints usage. Unknown or duplicate arguments fail.

## Build

The build requires exactly the OpenEB/Metavision SDK 5.2.0 `stream` component. The validation build is repository-local and is not installed. Its only explicitly configured `BUILD_RPATH` is the selected SDK library directory; CMake may also add local dependency-manager paths required by the imported SDK target.

```bash
REPO_ROOT="$(git rev-parse --show-toplevel)"
OPENEB_PREFIX="$REPO_ROOT/.deps/openeb-5.2.0-centuryarks-macos"
SMOKE_BUILD="$REPO_ROOT/.build/centuryarks-live-event-smoke-macos"
: "${STABLE_OPENEB_PREFIX:?Set the reviewed stable OpenEB prefix to ignore}"
MAKE_PROGRAM="${MAKE_PROGRAM:-$(command -v gmake)}"
test -n "$MAKE_PROGRAM"

cmake \
  -S "$REPO_ROOT/tools/centuryarks_live_event_smoke" \
  -B "$SMOKE_BUILD" \
  -G "Unix Makefiles" \
  -DCMAKE_MAKE_PROGRAM="$MAKE_PROGRAM" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_PREFIX_PATH="$OPENEB_PREFIX" \
  -DCMAKE_IGNORE_PREFIX_PATH="$STABLE_OPENEB_PREFIX"

cmake --build "$SMOKE_BUILD" --parallel 8 --verbose
```

The tool does not directly request or call UI, Qt, OpenCV, recording, firmware, EEPROM, bias, ROI, trigger, anti-flicker, ERC, or pixel-mask APIs. The audited B1 `MetavisionSDK::stream` target and binary have existing OpenCV dependencies; that SDK-transitive runtime closure is not introduced by this tool and cannot be removed without changing OpenEB.

## Run

Use the repository runner so the B1 prefix, plugin path, local HOME/TMPDIR, and no-DYLD boundary are applied consistently:

```bash
REPO_ROOT="$(git rev-parse --show-toplevel)"

"$REPO_ROOT/scripts/run_centuryarks_live_event_smoke.sh" \
  --serial '<full-runtime-selector>' \
  --duration-ms 5000 \
  --min-events 1 \
  >"$REPO_ROOT/.logs/centuryarks-live-event-smoke-macos/run.stdout.json" \
  2>"$REPO_ROOT/.logs/centuryarks-live-event-smoke-macos/run.stderr.log"
```

The runner waits three seconds before one bounded execution and does not retry. Keep complete selectors and serials only in ignored logs.

## Exit codes

| Code | Meaning |
| --- | --- |
| `0` | Open, start, stop, callback cleanup, and minimum event count passed. |
| `2` | The bounded run completed but received fewer than `--min-events`. |
| `3` | The asynchronous runtime error callback fired. |
| `4` | Invalid command line. |
| `5` | Camera open or start failed. |
| `6` | Stop or callback cleanup failed. |

For actual run modes stdout contains one JSON object and diagnostics use stderr. `--help` is the sole stdout exception.
