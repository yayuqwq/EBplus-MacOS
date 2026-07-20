#!/usr/bin/env bash

set -euo pipefail
IFS=$'\n\t'
umask 077

usage() {
    printf '%s\n' \
        'Usage: run_centuryarks_live_event_smoke.sh --serial <selector> [--duration-ms <100..5000>] [--min-events <N>]'
}

fail() {
    printf 'run_centuryarks_live_event_smoke: %s\n' "$*" >&2
    exit 4
}

serial=""
duration_ms="5000"
min_events="1"
serial_seen=false
duration_seen=false
minimum_seen=false

while (($# > 0)); do
    case "$1" in
        --serial)
            "$serial_seen" && fail "duplicate --serial argument"
            (($# >= 2)) || fail "missing value for --serial"
            serial="$2"
            serial_seen=true
            shift 2
            ;;
        --duration-ms)
            "$duration_seen" && fail "duplicate --duration-ms argument"
            (($# >= 2)) || fail "missing value for --duration-ms"
            duration_ms="$2"
            duration_seen=true
            shift 2
            ;;
        --min-events)
            "$minimum_seen" && fail "duplicate --min-events argument"
            (($# >= 2)) || fail "missing value for --min-events"
            min_events="$2"
            minimum_seen=true
            shift 2
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            fail "unknown argument"
            ;;
    esac
done

"$serial_seen" || fail "--serial is required"
test -n "$serial" || fail "--serial must not be empty"

[[ "$duration_ms" =~ ^[0-9]{1,4}$ ]] || fail "--duration-ms must be a decimal integer"
duration_value=$((10#$duration_ms))
((duration_value >= 100 && duration_value <= 5000)) || fail "--duration-ms must be between 100 and 5000"

[[ "$min_events" =~ ^[0-9]+$ ]] || fail "--min-events must be a non-negative decimal integer"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"
test "$SCRIPT_DIR" = "$REPO_ROOT/scripts" || fail "script is outside the repository scripts directory"

readonly OPENEB_PREFIX="$REPO_ROOT/.deps/openeb-5.2.0-centuryarks-macos"
readonly OPENEB_PLUGIN_PATH="$OPENEB_PREFIX/lib/metavision/hal/plugins"
readonly OPENEB_HDF5_PLUGIN_PATH="$OPENEB_PREFIX/lib/hdf5/plugin"
readonly SMOKE_EXECUTABLE="$REPO_ROOT/.build/centuryarks-live-event-smoke-macos/centuryarks_live_event_smoke"
readonly SMOKE_LOGS="$REPO_ROOT/.logs/centuryarks-live-event-smoke-macos"
readonly SMOKE_HOME="$REPO_ROOT/.tmp/centuryarks-live-event-smoke-home"
readonly SMOKE_TMP="$REPO_ROOT/.tmp/centuryarks-live-event-smoke-tmp"

test -d "$OPENEB_PREFIX" || fail "B1 OpenEB prefix is missing"
test -d "$OPENEB_PLUGIN_PATH" || fail "B1 HAL plugin directory is missing"
test -d "$OPENEB_HDF5_PLUGIN_PATH" || fail "B1 HDF5 plugin directory is missing"
test -x "$SMOKE_EXECUTABLE" || fail "smoke executable is missing or not executable"

mkdir -p "$SMOKE_LOGS" "$SMOKE_HOME" "$SMOKE_TMP"

unset DYLD_LIBRARY_PATH
unset DYLD_FALLBACK_LIBRARY_PATH

printf '%s\n' \
    'Live-event smoke starts in 3 seconds.' \
    'Point the camera at a high-contrast scene and gently move the' \
    'camera or wave an object during the capture.' >&2
sleep 3

exec env \
    HOME="$SMOKE_HOME" \
    TMPDIR="$SMOKE_TMP" \
    PATH="$OPENEB_PREFIX/bin:/usr/bin:/bin:/usr/sbin:/sbin" \
    MV_HAL_PLUGIN_PATH="$OPENEB_PLUGIN_PATH" \
    MV_HAL_PLUGIN_SEARCH_MODE=PLUGIN_PATH_ONLY \
    HDF5_PLUGIN_PATH="$OPENEB_HDF5_PLUGIN_PATH" \
    "$SMOKE_EXECUTABLE" \
    --serial "$serial" \
    --duration-ms "$duration_value" \
    --min-events "$min_events"
