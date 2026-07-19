#!/usr/bin/env bash

set -euo pipefail
IFS=$'\n\t'
umask 022

readonly EXPECTED_OPENEB_VERSION="5.2.0"
readonly EXPECTED_OPENEB_UPSTREAM_COMMIT="9003b5416676e78ba994d912087486cfa94fae73"
readonly EXPECTED_OPENEB_TREE="b407c407aa46d3b97edc9b2096fb120a96c8b465"
readonly EXPECTED_HDF5_ECF_COMMIT="b982d908a0bc0afd9104d226607bedb1a11b2a95"

readonly EXPECTED_OPENEB_CMAKELISTS_SHA256="47ca5abe75b65130f5a823f2b404cd9bd40fd26159fb4aa3fcb2c8352cf6c4af"
readonly EXPECTED_PLUGIN_LIB_CMAKE_SHA256="4e9339b8478d17a4e43e2195faec624cee1451c829e2f2c916ada8a03e66272f"
readonly EXPECTED_PLUGIN_SOURCE_CMAKE_SHA256="e5aa1d596e52571410dae6a89caa0274b40c23b328ff701f408302cfd3876a88"
readonly EXPECTED_PSEE_UNIVERSAL_SHA256="56e2b337d90fadc63ca1553e1d7b09423530b6fc18a1a9f668ef322a103afc19"
readonly EXPECTED_VENDOR_MANIFEST_SHA256="267662a92a883c4b07dacda33be0dd6060212569f4509fdbbd9f4f40cd3e4c3c"
readonly EXPECTED_VENDOR_LICENSE_SHA256="ab1119dedc6ca90aef94f95ad78b10580cca0a1de76e3ca3052b166be8399f03"
readonly EXPECTED_ADAPTED_SOURCE_SHA256="a818d8cb77e592da577a35bad0b5de93cea7f27451929bf134b471b3fa62d93b"

readonly HDF5_ECF_ROOT_PATH="openeb/sdk/modules/stream/cpp/3rdparty/hdf5_ecf"
readonly HDF5_ECF_PREPARED_PATH="sdk/modules/stream/cpp/3rdparty/hdf5_ecf"
readonly PREPARED_SOURCE_RELATIVE_PATH=".tmp/openeb-5.2.0-centuryarks-source"
readonly PREPARATION_LOG_DIR_RELATIVE_PATH=".logs/openeb-5.2.0-centuryarks-macos"

fail() {
    printf 'prepare_centuryarks_openeb_source: %s\n' "$*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

sha256_of() {
    local file_path="$1"
    local checksum_line

    checksum_line="$(shasum -a 256 "$file_path")"
    printf '%s\n' "${checksum_line%% *}"
}

verify_sha256() {
    local expected_checksum="$1"
    local file_path="$2"
    local actual_checksum

    test -f "$file_path" || fail "required file is missing: $file_path"
    actual_checksum="$(sha256_of "$file_path")"
    test "$actual_checksum" = "$expected_checksum" ||
        fail "SHA-256 mismatch for $file_path: expected $expected_checksum, got $actual_checksum"
}

require_literal() {
    local file_path="$1"
    local literal_text="$2"
    local description="$3"

    test -f "$file_path" || fail "required file is missing: $file_path"
    grep -Fq "$literal_text" "$file_path" || fail "$description is missing from $file_path"
}

is_allowed_dirty_path() {
    local dirty_path="$1"

    case "$dirty_path" in
        README.md | \
        README_CN.md | \
        docs/centuryarks_openeb_5_2_integration_plan.md | \
        docs/centuryarks_openeb_5_2_overlay_build.md | \
        docs/centuryarks_openeb_5x_source_audit.md | \
        docs/macos_porting_plan.md | \
        scripts/prepare_centuryarks_openeb_source.sh | \
        third_party/centuryarks/silkyevcam-openeb-5x/*)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

verify_allowed_worktree_changes() {
    local status_entry
    local status_code
    local dirty_path

    while IFS= read -r -d '' status_entry; do
        status_code="${status_entry:0:2}"
        dirty_path="${status_entry:3}"

        case "$status_code" in
            *R* | *C*)
                fail "renamed or copied paths are not allowed during preparation: $dirty_path"
                ;;
        esac

        is_allowed_dirty_path "$dirty_path" ||
            fail "working tree contains an out-of-scope change: $dirty_path"
    done < <(git status --porcelain=v1 -z --untracked-files=all)
}

verify_patch_contract() {
    local patch_file="$1"
    local diff_count
    local hunk_count
    local patched_files
    local added_lines
    local deleted_lines
    local expected_files
    local expected_added_lines

    diff_count="$(grep -c '^diff --git ' "$patch_file" || true)"
    hunk_count="$(grep -c '^@@ ' "$patch_file" || true)"
    test "$diff_count" -eq 2 || fail "Phase 1 patch must contain exactly two file diffs"
    test "$hunk_count" -eq 2 || fail "Phase 1 patch must contain exactly two semantic hunks"

    patched_files="$(sed -n 's#^+++ b/##p' "$patch_file" | LC_ALL=C sort)"
    expected_files=$'hal_psee_plugins/lib/CMakeLists.txt\nhal_psee_plugins/src/plugin/CMakeLists.txt'
    test "$patched_files" = "$expected_files" ||
        fail "Phase 1 patch modifies an unexpected file set"

    added_lines="$(sed -n '/^+++/d; s/^+//p' "$patch_file")"
    expected_added_lines=$'    hal_plugin_centuryarks\ntarget_sources(hal_plugin_centuryarks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/centuryarks_plugin.cpp)'
    test "$added_lines" = "$expected_added_lines" ||
        fail "Phase 1 patch additions differ from the reviewed target/source wiring"

    deleted_lines="$(grep '^-' "$patch_file" | grep -v '^---' || true)"
    test -z "$deleted_lines" || fail "Phase 1 patch must not delete existing CMake content"
}

require_command git
require_command grep
require_command find
require_command install
require_command sed
require_command shasum
require_command sort
require_command tar

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"
cd "$REPO_ROOT"

readonly SCRIPT_DIR
readonly REPO_ROOT
readonly PREPARED_SOURCE="$REPO_ROOT/$PREPARED_SOURCE_RELATIVE_PATH"
readonly VENDOR_ROOT="$REPO_ROOT/third_party/centuryarks/silkyevcam-openeb-5x"
readonly VENDOR_MANIFEST="$VENDOR_ROOT/manifests/vendor-source.sha256"
readonly VENDOR_LICENSE="$VENDOR_ROOT/LICENSE_OPEN"
readonly SOURCE_IDENTITY="$VENDOR_ROOT/SOURCE_IDENTITY.md"
readonly AUTHORITATIVE_SOURCE="$VENDOR_ROOT/src/centuryarks_plugin.cpp"
readonly PATCH_FILE="$VENDOR_ROOT/patches/openeb-5.2.0/0001-add-centuryarks-side-plugin.patch"
readonly HDF5_ECF_WORKTREE="$REPO_ROOT/$HDF5_ECF_ROOT_PATH"
readonly PREPARED_ENTRYPOINT="$PREPARED_SOURCE/hal_psee_plugins/src/plugin/centuryarks_plugin.cpp"
readonly PREPARATION_LOG_DIR="$REPO_ROOT/$PREPARATION_LOG_DIR_RELATIVE_PATH"
readonly PREPARATION_MANIFEST="$PREPARATION_LOG_DIR/preparation-manifest.txt"

current_branch="$(git branch --show-current)"
test -n "$current_branch" || fail "detached HEAD is not allowed during preparation"

verify_allowed_worktree_changes

test -z "$(git status --porcelain=v1 --untracked-files=all -- openeb)" ||
    fail "canonical tracked openeb/ must be clean"

actual_openeb_tree="$(git rev-parse HEAD:openeb)"
test "$actual_openeb_tree" = "$EXPECTED_OPENEB_TREE" ||
    fail "unexpected canonical OpenEB tree: $actual_openeb_tree"

git cat-file -e "$EXPECTED_OPENEB_UPSTREAM_COMMIT^{commit}" ||
    fail "audited upstream OpenEB commit is unavailable"

grep -Fqx "project(metavision VERSION $EXPECTED_OPENEB_VERSION)" openeb/CMakeLists.txt ||
    fail "canonical OpenEB does not declare version $EXPECTED_OPENEB_VERSION"

verify_sha256 "$EXPECTED_OPENEB_CMAKELISTS_SHA256" openeb/CMakeLists.txt
verify_sha256 "$EXPECTED_PLUGIN_LIB_CMAKE_SHA256" openeb/hal_psee_plugins/lib/CMakeLists.txt
verify_sha256 "$EXPECTED_PLUGIN_SOURCE_CMAKE_SHA256" openeb/hal_psee_plugins/src/plugin/CMakeLists.txt
verify_sha256 "$EXPECTED_PSEE_UNIVERSAL_SHA256" openeb/hal_psee_plugins/src/plugin/psee_universal.cpp

require_literal openeb/hal/cpp/lib/CMakeLists.txt \
    'foreach(targ metavision_hal metavision_hal_discovery)' \
    "HAL dylib target ownership"
require_literal openeb/hal/cpp/lib/CMakeLists.txt \
    'INSTALL_RPATH "@loader_path"' \
    "HAL dylib Apple install RPATH"

require_literal openeb/hal/cpp/samples/metavision_platform_info/CMakeLists.txt \
    'TARGET metavision_platform_info APPEND PROPERTY' \
    "metavision_platform_info RPATH target"
require_literal openeb/hal/cpp/samples/metavision_platform_info/CMakeLists.txt \
    'INSTALL_RPATH "@executable_path/../lib"' \
    "metavision_platform_info Apple install RPATH"

require_literal openeb/hal_psee_plugins/lib/CMakeLists.txt \
    'TARGET metavision_psee_hw_layer APPEND PROPERTY' \
    "shared hardware-layer RPATH target"
require_literal openeb/hal_psee_plugins/lib/CMakeLists.txt \
    'INSTALL_RPATH "@loader_path/../../.."' \
    "shared hardware-layer Apple install RPATH"
require_literal openeb/hal_psee_plugins/lib/CMakeLists.txt \
    'set_property(TARGET ${plugin} APPEND PROPERTY INSTALL_RPATH' \
    "HAL plugin RPATH target loop"
require_literal openeb/hal_psee_plugins/lib/CMakeLists.txt \
    '"@loader_path"' \
    "HAL plugin loader-directory RPATH"
require_literal openeb/hal_psee_plugins/lib/CMakeLists.txt \
    '"@loader_path/../../.."' \
    "HAL plugin prefix-relative RPATH"

require_literal openeb/sdk/cmake/MetavisionSDKModulesHelper.cmake \
    'TARGET metavision_sdk_${module_name} APPEND PROPERTY' \
    "SDK dylib RPATH target"
require_literal openeb/sdk/cmake/MetavisionSDKModulesHelper.cmake \
    'INSTALL_RPATH "@loader_path"' \
    "SDK dylib Apple install RPATH"

require_literal openeb/sdk/modules/stream/cpp/3rdparty/CMakeLists.txt \
    'TARGET hdf5_ecf_plugin APPEND PROPERTY' \
    "HDF5 ECF plugin RPATH target"
require_literal openeb/sdk/modules/stream/cpp/3rdparty/CMakeLists.txt \
    'INSTALL_RPATH "@loader_path/../.."' \
    "HDF5 ECF plugin Apple install RPATH"

require_literal openeb/sdk/modules/stream/cpp/samples/metavision_file_info/CMakeLists.txt \
    'TARGET metavision_file_info APPEND PROPERTY' \
    "metavision_file_info RPATH target"
require_literal openeb/sdk/modules/stream/cpp/samples/metavision_file_info/CMakeLists.txt \
    'INSTALL_RPATH "@executable_path/../lib"' \
    "metavision_file_info Apple install RPATH"

require_literal openeb/sdk/modules/stream/cpp/samples/metavision_file_to_hdf5/CMakeLists.txt \
    'TARGET metavision_file_to_hdf5 APPEND PROPERTY' \
    "metavision_file_to_hdf5 RPATH target"
require_literal openeb/sdk/modules/stream/cpp/samples/metavision_file_to_hdf5/CMakeLists.txt \
    'INSTALL_RPATH "@executable_path/../lib"' \
    "metavision_file_to_hdf5 Apple install RPATH"

IFS=$' \t' read -r gitlink_mode gitlink_commit gitlink_stage gitlink_path < <(
    git ls-files --stage "$HDF5_ECF_ROOT_PATH"
)
test "$gitlink_mode" = "160000" || fail "HDF5 ECF root entry is not a gitlink"
test "$gitlink_commit" = "$EXPECTED_HDF5_ECF_COMMIT" || fail "HDF5 ECF index commit changed"
test "$gitlink_stage" = "0" || fail "HDF5 ECF gitlink is not at index stage 0"
test "$gitlink_path" = "$HDF5_ECF_ROOT_PATH" || fail "unexpected HDF5 ECF gitlink path"

IFS=$' \t' read -r tree_mode tree_type tree_commit tree_path < <(git ls-tree HEAD "$HDF5_ECF_ROOT_PATH")
test "$tree_mode" = "160000" || fail "HEAD HDF5 ECF entry is not a gitlink"
test "$tree_type" = "commit" || fail "HEAD HDF5 ECF entry has unexpected type"
test "$tree_commit" = "$EXPECTED_HDF5_ECF_COMMIT" || fail "HEAD HDF5 ECF commit changed"
test "$tree_path" = "$HDF5_ECF_ROOT_PATH" || fail "HEAD HDF5 ECF path changed"

test "$(git config -f .gitmodules --get "submodule.$HDF5_ECF_ROOT_PATH.path")" = "$HDF5_ECF_ROOT_PATH" ||
    fail "HDF5 ECF root submodule path mapping changed"
test "$(git config -f .gitmodules --get "submodule.$HDF5_ECF_ROOT_PATH.url")" = \
    "https://github.com/prophesee-ai/hdf5_ecf.git" || fail "HDF5 ECF URL mapping changed"

test -d "$HDF5_ECF_WORKTREE" || fail "HDF5 ECF worktree is not initialized"
test "$(git -C "$HDF5_ECF_WORKTREE" rev-parse HEAD)" = "$EXPECTED_HDF5_ECF_COMMIT" ||
    fail "HDF5 ECF worktree is not at the pinned commit"
test -z "$(git -C "$HDF5_ECF_WORKTREE" status --short)" || fail "HDF5 ECF worktree is dirty"

submodule_status="$(git submodule status -- "$HDF5_ECF_ROOT_PATH")"
test "${submodule_status:0:1}" = " " || fail "HDF5 ECF submodule status is not clean and pinned"

test -f "$VENDOR_MANIFEST" || fail "tracked vendor SHA-256 manifest is missing"
test -f "$VENDOR_LICENSE" || fail "tracked vendor license is missing"
test -f "$SOURCE_IDENTITY" || fail "tracked vendor source identity record is missing"
test -f "$AUTHORITATIVE_SOURCE" || fail "authoritative CenturyArks entrypoint is missing"
test -f "$PATCH_FILE" || fail "reviewed Phase 1 patch is missing"

verify_sha256 "$EXPECTED_VENDOR_MANIFEST_SHA256" "$VENDOR_MANIFEST"
verify_sha256 "$EXPECTED_VENDOR_LICENSE_SHA256" "$VENDOR_LICENSE"
verify_sha256 "$EXPECTED_ADAPTED_SOURCE_SHA256" "$AUTHORITATIVE_SOURCE"

verify_patch_contract "$PATCH_FILE"

test ! -e "$PREPARED_SOURCE" || fail "prepared source output already exists: $PREPARED_SOURCE"

if test -e "$PREPARATION_MANIFEST"; then
    test -f "$PREPARATION_MANIFEST" ||
        fail "existing preparation manifest is not a regular file: $PREPARATION_MANIFEST"
    test ! -L "$PREPARATION_MANIFEST" ||
        fail "existing preparation manifest must not be a symbolic link: $PREPARATION_MANIFEST"
fi

mkdir -p "$PREPARED_SOURCE"
git archive --format=tar HEAD:openeb | tar -xf - -C "$PREPARED_SOURCE"

mkdir -p "$PREPARED_SOURCE/$HDF5_ECF_PREPARED_PATH"
git -C "$HDF5_ECF_WORKTREE" cat-file -e "$EXPECTED_HDF5_ECF_COMMIT^{commit}"
git -C "$HDF5_ECF_WORKTREE" archive --format=tar "$EXPECTED_HDF5_ECF_COMMIT" |
    tar -xf - -C "$PREPARED_SOURCE/$HDF5_ECF_PREPARED_PATH"

install -m 0644 "$AUTHORITATIVE_SOURCE" "$PREPARED_ENTRYPOINT"

grep -Fqx "project(metavision VERSION $EXPECTED_OPENEB_VERSION)" "$PREPARED_SOURCE/CMakeLists.txt" ||
    fail "prepared OpenEB source does not declare version $EXPECTED_OPENEB_VERSION"
test -z "$(find "$PREPARED_SOURCE" -name .git -print -quit)" ||
    fail "prepared OpenEB source contains forbidden Git metadata"

apply_check_output="$(
    git apply \
        --check \
        --verbose \
        --unidiff-zero \
        --directory="$PREPARED_SOURCE_RELATIVE_PATH" \
        "$PATCH_FILE" 2>&1
)" || fail "Phase 1 patch check failed: $apply_check_output"

if printf '%s\n' "$apply_check_output" | grep -Eiq 'offset|fuzz'; then
    fail "Phase 1 patch check reported offset or fuzz: $apply_check_output"
fi

apply_output="$(
    git apply \
        --verbose \
        --unidiff-zero \
        --directory="$PREPARED_SOURCE_RELATIVE_PATH" \
        "$PATCH_FILE" 2>&1
)" || fail "Phase 1 patch application failed: $apply_output"

if printf '%s\n' "$apply_output" | grep -Eiq 'offset|fuzz'; then
    fail "Phase 1 patch application reported offset or fuzz: $apply_output"
fi

test "$(grep -c '^[[:space:]]*hal_plugin_centuryarks[[:space:]]*$' \
    "$PREPARED_SOURCE/hal_psee_plugins/lib/CMakeLists.txt")" -eq 1 ||
    fail "prepared plugin list does not contain exactly one hal_plugin_centuryarks entry"
test "$(grep -Fxc 'target_sources(hal_plugin_centuryarks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/centuryarks_plugin.cpp)' \
    "$PREPARED_SOURCE/hal_psee_plugins/src/plugin/CMakeLists.txt")" -eq 1 ||
    fail "prepared source wiring for hal_plugin_centuryarks is not exact"

test -z "$(git status --porcelain=v1 --untracked-files=all -- openeb)" ||
    fail "canonical tracked openeb/ changed during preparation"

root_head="$(git rev-parse HEAD)"
authoritative_source_sha256="$(sha256_of "$AUTHORITATIVE_SOURCE")"
prepared_source_sha256="$(sha256_of "$PREPARED_ENTRYPOINT")"
patch_sha256="$(sha256_of "$PATCH_FILE")"
vendor_manifest_sha256="$(sha256_of "$VENDOR_MANIFEST")"
vendor_license_sha256="$(sha256_of "$VENDOR_LICENSE")"
source_identity_sha256="$(sha256_of "$SOURCE_IDENTITY")"
prepared_lib_cmake_sha256="$(sha256_of "$PREPARED_SOURCE/hal_psee_plugins/lib/CMakeLists.txt")"
prepared_source_cmake_sha256="$(sha256_of "$PREPARED_SOURCE/hal_psee_plugins/src/plugin/CMakeLists.txt")"

mkdir -p "$PREPARATION_LOG_DIR"
{
    printf 'format_version=1\n'
    printf 'root_head=%s\n' "$root_head"
    printf 'branch=%s\n' "$current_branch"
    printf 'canonical_openeb_tree=%s\n' "$actual_openeb_tree"
    printf 'audited_upstream_openeb_commit=%s\n' "$EXPECTED_OPENEB_UPSTREAM_COMMIT"
    printf 'openeb_version=%s\n' "$EXPECTED_OPENEB_VERSION"
    printf 'hdf5_ecf_commit=%s\n' "$EXPECTED_HDF5_ECF_COMMIT"
    printf 'vendor_manifest_sha256=%s\n' "$vendor_manifest_sha256"
    printf 'vendor_license_sha256=%s\n' "$vendor_license_sha256"
    printf 'source_identity_sha256=%s\n' "$source_identity_sha256"
    printf 'authoritative_source=third_party/centuryarks/silkyevcam-openeb-5x/src/centuryarks_plugin.cpp\n'
    printf 'authoritative_source_sha256=%s\n' "$authoritative_source_sha256"
    printf 'prepared_entrypoint_sha256=%s\n' "$prepared_source_sha256"
    printf 'patch=third_party/centuryarks/silkyevcam-openeb-5x/patches/openeb-5.2.0/0001-add-centuryarks-side-plugin.patch\n'
    printf 'patch_sha256=%s\n' "$patch_sha256"
    printf 'prepared_plugin_lib_cmake_sha256=%s\n' "$prepared_lib_cmake_sha256"
    printf 'prepared_plugin_source_cmake_sha256=%s\n' "$prepared_source_cmake_sha256"
    printf 'patch_check=%s\n' "$apply_check_output"
    printf 'patch_apply=%s\n' "$apply_output"
} >"$PREPARATION_MANIFEST"

printf 'Prepared CenturyArks OpenEB 5.2 source: %s\n' "$PREPARED_SOURCE"
printf 'Preparation manifest: %s\n' "$PREPARATION_MANIFEST"
