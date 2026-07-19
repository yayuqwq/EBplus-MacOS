# CenturyArks OpenEB 5.2 side-by-side overlay Phase 1 plan and validation

## 1. Purpose and revision status

This document defines and records Milestone 2C-B1: the first OpenEB 5.2 CenturyArks implementation, build and hardware-enumeration stage.

It supersedes the earlier feature-gated, two-profile proposal. The revised design keeps Architecture B for source isolation, but the prepared CenturyArks source always builds and installs both live-device plugins:

```text
hal_plugin_prophesee
hal_plugin_centuryarks
metavision_psee_hw_layer
```

The canonical tracked `openeb/` tree remained unchanged. The tracked source-delivery inputs, exact two-hunk patch and deterministic preparation script were implemented, and the isolated prepared-source configure, bootstrap build, full build, install and runtime validation all passed.

### Phase 1 result

The validated implementation has these properties:

- The prepared source always builds `hal_plugin_prophesee` and `hal_plugin_centuryarks`; there is no CenturyArks feature option.
- The tracked patch changes only `hal_psee_plugins/lib/CMakeLists.txt` and `hal_psee_plugins/src/plugin/CMakeLists.txt`, with one reviewed hunk in each file.
- Canonical tracked `openeb/` remained byte-for-byte outside the implementation diff; all overlay changes were materialized under the ignored prepared-source path.
- Configure, the requested bootstrap targets, the full build and repository-local install passed for the macOS arm64 Release profile.
- `metavision_psee_hw_layer_obj` produced 55 hardware-layer object files under one object target, and only `metavision_psee_hw_layer` owns those objects. Neither plugin DSO directly embeds them.
- The existing `metavision_active_pixel_detection` sample can consume outputs from that same object target. This is reuse of the already produced objects, not a second hardware-layer object compilation or a second hardware-layer target.
- The standard plugin source and normalized link baseline were unchanged. The installed loader inventory reports `hal_plugin_centuryarks` / `CenturyArks` with one camera discovery and zero file discoveries, and `hal_plugin_prophesee` / `Prophesee` with two camera discoveries and one file discovery.
- Static source, generated target, object-code/disassembly and runtime evidence confirmed that only the CenturyArks entrypoint registers `31f7:0002`, `31f7:0003` and `31f7:0004` with subclass `0x19`. Interface class `0xff` and protocol `0x00` remain enforced by the existing Treuzell discovery/board code.
- The adapted entrypoint contains no fixed serial, serial hash, product or firmware gate, no EEPROM/pixel-mask/facility behavior and no `PseeFileDiscovery`.
- No-DYLD CLI identity, RAW-to-HDF5 conversion/readback, HDF5 ECF loading, install RPATH/linkage and OpenEB 5.1.1 contamination checks passed.
- A physical `31f7:0003` IMX636 device enumerated and opened through `CenturyArks:hal_plugin_centuryarks:<runtime serial>`; `--short`, `--system` and the three-second reopen smoke passed. The tracked identity is limited to serial SHA-256 prefix `cb823604ea92`, which identifies only the tested instance and is not an implementation input. The observed system release was `3.9.0`.
- PIDs `0002` and `0004` are registered from the audited vendor source but have not been tested with hardware. Live events, EEPROM, pixel masks, facility changes and Linux configure/build/runtime validation were not performed.

## 2. Milestone 2C-B0 evidence

The preceding read-only hardware baseline established:

```text
Physical USB identity:
VID 0x31f7
PID 0x0003
Interface 0xff / 0x19 / 0x00
Sensor IMX636

Stable OpenEB 5.1.1 identity:
Plugin silky_common_plugin
Integrator CenturyArks
Enumeration/open/reopen passed
Live event delivery not run
```

The observed product and firmware strings are hardware-baseline evidence only. They must not become registration keys, source constants or behavioral gates in the OpenEB 5.2 plugin.

The stable 5.1.1 installation uses a replacement topology and does not contain the standard Prophesee plugin. Phase 1 must not reproduce that layout: it must retain the standard plugin and add a new CenturyArks plugin beside it.

The B0 result itself validated only the supplied 5.1.1 environment and the physical `31f7:0003` camera. Phase 1 subsequently validated the new OpenEB 5.2 plugin for enumeration, open and one reopen smoke on that device. Live events, EEPROM data, pixel masks, facilities and the other two registered PIDs remain unvalidated.

## 3. Required Architecture B topology

Source preparation remains isolated:

```text
tracked openeb/
+ tracked adapted CenturyArks inputs
+ tracked OpenEB 5.2 patch hunks
        ↓
.tmp/openeb-5.2.0-centuryarks-source
        ↓
.build/openeb-5.2.0-centuryarks-macos
        ↓
.deps/openeb-5.2.0-centuryarks-macos
```

The prepared source has one target topology and no user-selectable CenturyArks build mode:

```text
metavision_psee_hw_layer_obj
        ↓ compiled and owned once
metavision_psee_hw_layer
        ├── hal_plugin_prophesee
        └── hal_plugin_centuryarks
```

Mandatory invariants:

- `hal_plugin_prophesee` retains its current target name, source, registrations, install rules and platform behavior.
- `hal_plugin_centuryarks` is always in the prepared source's plugin target list and install graph.
- Both plugins link the existing `metavision_psee_hw_layer` shared library.
- `metavision_psee_hw_layer_obj` is compiled once and embedded only in `metavision_psee_hw_layer`.
- `hal_plugin_centuryarks` must not directly consume `$<TARGET_OBJECTS:metavision_psee_hw_layer_obj>`.
- The existing `metavision_hal_psee_plugin_obj` plugin-common object library may be compiled once and embedded in both plugin dylibs through the current plugin loop; it is not the hardware-layer object library.
- No `metavision_silky_hw_layer` or other renamed hardware-layer target may be created.
- The canonical tracked `openeb/` build remains the preserved non-CenturyArks baseline because its source is never patched in place.

## 4. Rejected alternatives

### Vendor README wholesale overwrite

Rejected because it replaces the standard plugin, renames the hardware layer, disables V4L2 paths, duplicates hardware-layer objects and overwrites the validated Apple RPATH changes.

### Direct changes in canonical tracked `openeb/`

Rejected for Phase 1. Hardware behavior beyond enumeration/open remains incomplete, and the ignored prepared source provides a safer review and rollback boundary.

### Private copied hardware layer

Rejected. A second hardware-layer implementation would duplicate symbols, ABI ownership and future OpenEB maintenance.

### User-selectable CenturyArks build mode

Rejected for this fork. SilkyEvCam support is an explicit fork objective, the audited vendor IDs do not overlap the current standard plugin IDs, and users should not need a manual configure switch to obtain the fork's intended plugin set.

Device-specific behavior is still gated by plugin-owned VID/PID registration. Always building the plugin does not authorize generic CenturyArks behavior in standard devices or shared facilities.

## 5. Tracked implementation inputs

The implementation branch adds exactly these source-delivery inputs:

```text
third_party/centuryarks/silkyevcam-openeb-5x/
├── SOURCE_IDENTITY.md
├── LICENSE_OPEN
├── manifests/
│   └── vendor-source.sha256
├── patches/
│   └── openeb-5.2.0/
│       └── 0001-add-centuryarks-side-plugin.patch
└── src/
    └── centuryarks_plugin.cpp

scripts/
└── prepare_centuryarks_openeb_source.sh

docs/
└── centuryarks_openeb_5_2_overlay_build.md
```

Phase 1 uses one patch only. It must not include EEPROM, pixel-mask, system-information override, file-discovery, udev or facility hunks.

The Downloads directory is provenance evidence for the completed audit, not a future build input. Every input needed to prepare and build the source must be Git-controlled after an explicit source-import review.

## 6. Preparation script contract

`scripts/prepare_centuryarks_openeb_source.sh` enforces the following contract:

1. Resolve `REPO_ROOT` from Git and refuse to run outside the intended repository.
2. Require a clean canonical tracked `openeb/` tree and a clean root working tree apart from the reviewed implementation files.
3. Verify the audited upstream OpenEB identity `9003b5416676e78ba994d912087486cfa94fae73`, declared version `5.2.0` and required source-file hashes.
4. Verify that the current Apple RPATH hunks are present before applying the CenturyArks patch.
5. Verify the HDF5 ECF root gitlink and checked-out commit `b982d908a0bc0afd9104d226607bedb1a11b2a95`.
6. Refuse to continue if `.tmp/openeb-5.2.0-centuryarks-source` already exists.
7. Materialize canonical OpenEB from Git-controlled content without build products or root Git metadata.
8. Materialize HDF5 ECF separately from its pinned source because a root archive does not contain submodule content.
9. Never copy the submodule `.git` file or any root `.git/` content into the prepared source.
10. Treat `third_party/centuryarks/silkyevcam-openeb-5x/src/centuryarks_plugin.cpp` as the only authoritative adapted entrypoint and copy it to `hal_psee_plugins/src/plugin/centuryarks_plugin.cpp` in the prepared source.
11. Run patch applicability checks before applying `0001-add-centuryarks-side-plugin.patch`.
12. Apply only the reviewed Phase 1 patch.
13. Produce an ignored preparation manifest containing source hashes, patch hash and pinned dependency identity.
14. Stop without deleting or overwriting partial output when an identity or patch assertion fails.

The script must not read the vendor package from Downloads during normal preparation, download dependencies, configure CMake or remove an existing output directory.

The implemented script passed `bash -n`, verified the exact OpenEB 5.2 and HDF5 ECF identities, rejected output reuse, materialized Git-controlled source without Git metadata, applied the patch without offset or fuzz and produced an ignored preparation manifest. Its normal execution did not use the Downloads directory as a build input.

## 7. `centuryarks_plugin.cpp` contract

The adapted entrypoint is the minimum 5.2-compatible derivative of the supplied `silky_common.cpp`.

It must:

- retain the Prophesee and CenturyArks copyright notices;
- retain the Apache-2.0 header;
- add a prominent EBplus/OpenEB 5.2 modification notice;
- initialize the plugin integrator as `CenturyArks`;
- create the existing Treuzell USB discovery implementation;
- register exactly `31f7:0002`, `31f7:0003` and `31f7:0004` with subclass `0x19`;
- preserve the existing discovery requirement for vendor-specific interface class `0xff` and protocol `0x00`;
- expose no fixed serial filter;
- rely on OpenEB runtime discovery for per-device serial identity;
- compile as the entrypoint source of `hal_plugin_centuryarks`.

It must not:

- register `PseeFileDiscovery`;
- contain the current camera's serial or serial hash;
- contain product-name or firmware-version matching;
- contain PID-to-model mapping;
- contain EEPROM or pixel-mask code;
- modify facilities or device builders;
- register standard Prophesee IDs;
- directly embed hardware-layer object files.

The installed dylib basename must be `libhal_plugin_centuryarks.dylib`, making the runtime plugin name `hal_plugin_centuryarks`. Successful device identities must be constructed by OpenEB at runtime:

```text
CenturyArks:hal_plugin_centuryarks:<runtime serial>
```

## 8. OpenEB 5.2 patch contract

`0001-add-centuryarks-side-plugin.patch` may modify only the minimum prepared-source CMake locations required to:

- append `hal_plugin_centuryarks` unconditionally to the existing plugin target list;
- associate the copied `centuryarks_plugin.cpp` with that target;
- reuse the current plugin-common and shared hardware-layer link structure;
- install/copy the new target through the existing plugin loop;
- inherit the existing Apple and non-Apple RPATH branches.

The implemented patch contains exactly two CMake hunks:

1. `hal_psee_plugins/lib/CMakeLists.txt`: append `hal_plugin_centuryarks` to `plugin_list`.
2. `hal_psee_plugins/src/plugin/CMakeLists.txt`: add the prepared `centuryarks_plugin.cpp` to `hal_plugin_centuryarks`.

The stored insertion hunks use zero context to avoid trailing-whitespace context lines in the tracked patch payload. This does not relax source identity: the preparation script verifies exact canonical file hashes before using `git apply --unidiff-zero`, and rejects any reported offset or fuzz.

The patch must not carry a second copy of the entrypoint. It must not add separate link, install or RPATH logic when the existing plugin loop already supplies that behavior.

The patch must preserve:

- `hal_plugin_prophesee` and `psee_universal.cpp` unchanged;
- every standard Prophesee USB, FX3, V4L2 and RAW registration;
- `metavision_psee_hw_layer`, its alias/export and its source ownership;
- `HAS_V4L2` behavior;
- existing install destinations and component behavior;
- the Apple RPATH patch already validated on `main`;
- the non-Apple `$ORIGIN` RPATH path.

The patch must not introduce a configure option, rename an existing target, create a branded diagnostic executable or add vendor code to the standard plugin.

## 9. Single build profile

Only one CenturyArks profile is permitted:

```text
Prepared source:
$REPO_ROOT/.tmp/openeb-5.2.0-centuryarks-source

Build:
$REPO_ROOT/.build/openeb-5.2.0-centuryarks-macos

Install:
$REPO_ROOT/.deps/openeb-5.2.0-centuryarks-macos

Logs:
$REPO_ROOT/.logs/openeb-5.2.0-centuryarks-macos

Artifacts:
$REPO_ROOT/.artifacts/openeb-5.2.0-centuryarks-macos
```

Configure inputs must otherwise match the validated OpenEB 5.2 macOS RPATH profile: Unix Makefiles, GNU Make, arm64, Release, the same selected modules, samples, Protobuf, HDF5 and dependency isolation, with tests/Python/docs/coverage/LFS disabled as previously reviewed.

The only permitted source/configuration differences are:

- the prepared source directory;
- the reviewed Phase 1 side-plugin patch and adapted source;
- the CenturyArks-specific repository-local output paths.

Before source preparation or configure, repeat Git, source-identity, dependency and disk preflight. Do not delete or reuse the existing base/RPATH build and install directories.

## 10. Configure, build and install acceptance and result

Required acceptance:

1. Configure succeeds with the reviewed OpenEB 5.2 profile.
2. The generated target graph contains `hal_plugin_prophesee`, `hal_plugin_centuryarks` and `metavision_psee_hw_layer`.
3. Full build succeeds.
4. Repository-local install succeeds without `sudo`.
5. Both plugin dylibs are installed in the standard HAL plugin directory.
6. `metavision_psee_hw_layer` is built once under its existing name.
7. `metavision_psee_hw_layer_obj` appears in only the shared hardware-layer ownership path.
8. The CenturyArks link command contains the shared hardware layer but no direct hardware-layer object expansion.
9. No renamed Silky hardware-layer target or dylib exists.
10. The standard plugin source list and live/file registrations are not reduced.
11. No generated include, dependency or link artifact uses OpenEB 5.1.1 from `/usr/local`.
12. No install artifact contains a build-tree path or current repository absolute path.
13. The three required CLI run without `DYLD_LIBRARY_PATH` or `DYLD_FALLBACK_LIBRARY_PATH`.
14. RAW and HDF5 write/readback regression remains equivalent to the validated base build.
15. In a no-camera `PLUGIN_PATH_ONLY` smoke, loader TRACE shows both plugin dylibs load without a linkage or initialization failure.

The generated source lists, verbose compile commands, verbose link commands, install manifest and target graph must be retained in the ignored logs.

All fifteen acceptance items passed locally. The target graph and install contain both plugins and the existing shared hardware layer. The hardware object target produced 55 object files once; both plugin link commands reference `metavision_psee_hw_layer` and contain no direct hardware-object expansion. The existing `metavision_active_pixel_detection` sample's use of those target outputs does not create another object target or another compilation of the 55 files.

## 11. Registration acceptance

The CenturyArks plugin must register exactly:

```text
31f7:0002  subclass 0x19
31f7:0003  subclass 0x19
31f7:0004  subclass 0x19
```

Registration evidence must combine:

- the prepared target source list;
- the adapted entrypoint source;
- verbose compile and link commands;
- exported symbols and plugin initialization;
- strings and disassembly where numeric IDs are not retained as text;
- loader TRACE output;
- enumeration of the physical `31f7:0003` camera.

Static scans must confirm:

- no fixed serial or serial hash is present;
- no product or firmware string controls registration;
- no `PseeFileDiscovery` is compiled into the CenturyArks entrypoint;
- no standard Prophesee ID is registered by the CenturyArks plugin;
- no `0x31f7` ID is registered by `hal_plugin_prophesee`.

PID `0003` is the available hardware target for this milestone. PIDs `0002` and `0004` remain registered from the supplied vendor source but must be reported as `hardware not yet validated`.

The standard-plugin comparison baseline must retain all current `psee_universal.cpp` behavior:

```text
Treuzell USB registrations:
03fd:5832 subclass 0x19
03fd:5832 subclass 0x00
04b4:00f4 subclass 0x19
04b4:00f5 subclass 0x19
1fc9:5838 subclass 0x19

Other discovery:
Fx3CameraDiscovery
conditional V4l2CameraDiscovery
PseeFileDiscovery
```

The prepared source hash and hunk diff for `psee_universal.cpp` must match canonical OpenEB exactly.

The registration acceptance passed. The CenturyArks entrypoint compiled exactly the three `0x31f7` registrations, while the standard entrypoint retained its existing registrations and contained no `0x31f7` immediate. The loader probe reported one camera discovery and zero file discoveries for CenturyArks, versus two camera discoveries and one file discovery for the unchanged standard plugin.

## 12. Runtime identity and multi-camera behavior

The implementation must not assume that only one camera is connected.

Required design properties:

- each discovered device remains distinguished by its runtime-provided serial;
- registration contains no serial filter;
- an explicit full runtime selector obtained from OpenEB enumeration can select a device through `Metavision::Camera::from_serial("CenturyArks:hal_plugin_centuryarks:<runtime serial>")`;
- an empty serial continues to use OpenEB's existing first-device behavior;
- enumeration order is not treated as a stable API;
- no current-camera serial or hash is stored in source, patch, build configuration or runtime defaults;
- multiple CenturyArks cameras with the same PID remain distinguishable by runtime identity.

Full serials may appear only in ignored, access-restricted runtime logs. Tracked validation documentation may record only an anonymized serial hash and must state that it is evidence for one tested device, not implementation input.

The stable B0 identity prefix `CenturyArks:silky_common_plugin:` is legacy 5.1.1 evidence. Phase 1 intentionally establishes the new live-device prefix `CenturyArks:hal_plugin_centuryarks:` and must not add a hidden plugin-name alias. Legacy RAW/plugin-name compatibility remains deferred with file discovery.

If multiple cameras are available, an additional enumeration-only check is permitted after separate preflight. Concurrent multi-camera streaming is not a Phase 1 requirement.

## 13. Physical camera acceptance

Use the currently available `31f7:0003` camera only after build/install and static registration checks pass.

Required sequence in the repository-local OpenEB 5.2 CenturyArks environment:

1. `metavision_platform_info --software` reports `5.2.0`.
2. The process environment uses the repository-local HAL plugin directory with `PLUGIN_PATH_ONLY` and no DYLD override.
3. Loader TRACE shows both `hal_plugin_prophesee` and `hal_plugin_centuryarks` loaded.
4. `hal_plugin_centuryarks` with integrator `CenturyArks` matches and opens `31f7:0003`.
5. `hal_plugin_prophesee` does not claim or open that device.
6. `--short` exits successfully and reports the CenturyArks plugin/integrator identity.
7. `--system` exits successfully.
8. After normal command exit, wait at least three seconds.
9. A second `--short` reopens the same runtime identity successfully.
10. Full serial values remain only in ignored logs; reports use a redacted or hashed identity.

Passing this sequence validates enumeration/open and a single-process reopen smoke. It does not validate live events, unplug/replug, facilities, parameter mutation, EEPROM data or pixel-mask behavior.

This sequence passed for the available `31f7:0003` IMX636 device. The plugin integrator was `CenturyArks`, the runtime plugin was `hal_plugin_centuryarks`, `--short` and `--system` succeeded, and the device reopened after the required three-second wait. The anonymized serial evidence is `cb823604ea92`; no full device serial is stored in tracked documentation. The observed system release was `3.9.0`.

## 14. Live-event follow-up

After the new OpenEB 5.2 plugin passes enumeration/open, audit the new install prefix for a headless command that has:

- an explicit total duration or event-count limit;
- unattended normal termination;
- no device-parameter writes;
- no firmware or EEPROM operations;
- repository-local bounded output.

If no command satisfies all gates, record:

```text
Live event delivery: Not run
```

The installed-tool audit found no command satisfying all bounded, headless and non-mutating gates. Live event delivery was therefore not run.

Do not run an infinite recorder, GUI viewer or unreviewed sample. A maximum five-second live stream or RAW recording requires a separate authorization after the candidate and disk-output bound are reviewed.

## 15. macOS RPATH and isolation acceptance

Both installed plugins must have effective Apple install RPATH entries:

```text
@loader_path
@loader_path/../../..
```

The shared hardware layer must retain its validated install RPATH:

```text
@loader_path/../../..
```

Full `otool -L` and `otool -l` evidence must cover:

- the three required CLI;
- all installed `libmetavision*.dylib` files;
- `metavision_psee_hw_layer`;
- `hal_plugin_prophesee`;
- `hal_plugin_centuryarks`;
- the HDF5 ECF plugin.

Allowed dependencies remain repository-local OpenEB, controlled `@rpath`/`@loader_path`, Homebrew dependencies and macOS system libraries/frameworks.

Forbidden dependencies include:

- `/usr/local/lib/libmetavision*`;
- `/usr/local/lib/metavision/hal/plugins`;
- build-tree runtime paths;
- the current repository's absolute path embedded as an install dependency.

The installed Mach-O audit passed. Both plugins contain `@loader_path` and `@loader_path/../../..`; the shared hardware layer contains `@loader_path/../../..`. The required CLI, SDK/HAL libraries and HDF5 ECF plugin resolved with the repository-local layout, with no OpenEB 5.1.1 runtime dependency, build-tree runtime path or repository absolute install dependency.

## 16. Linux and standard-device protection

The canonical tracked OpenEB and EBplus Linux baseline remain unchanged because Phase 1 is delivered through an isolated prepared source.

Within the prepared source:

- the standard plugin remains enabled and unchanged;
- V4L2 directories and `HAS_V4L2` behavior remain intact;
- the existing non-Apple RPATH branch remains intact;
- no udev rule is added or installed;
- the new plugin owns only the three CenturyArks live USB IDs;
- standard devices must not execute CenturyArks-specific behavior.

The Phase 1 patch remains structurally cross-platform and preserves the existing non-Apple branch, but this milestone validated macOS arm64 only. Linux configure, build and runtime regression checks remain required before claiming Linux support for the prepared CenturyArks profile.

## 17. Licensing and source identity

- Preserve all supplied Prophesee and CenturyArks copyright notices.
- Include the supplied Apache License 2.0 text with tracked vendor-derived material.
- Add prominent modification notices to adapted source and patch files.
- Record the vendor package identity, communicated compatibility and exact imported file hashes.
- Exclude both `.DS_Store` files.
- Do not import or install `ca_device.rules` in Phase 1.
- Do not imply trademark permission, endorsement or compatibility beyond the audited and tested scope.

The package manifest proves source identity; it does not prove behavior for every OpenEB 5.x release or every registered camera PID.

## 18. Explicitly deferred work

Phase 1 must not implement:

- EEPROM reads;
- pixel-mask decoding or application;
- IMX636/IMX646 facility lifecycle changes;
- CenturyArks system-information overrides;
- firmware `-C` suffix logic;
- PID-to-model hardcoding;
- legacy CenturyArks RAW file discovery;
- udev rules;
- firmware update/reset/recovery;
- bias, ROI, trigger, anti-flicker, ERC or other parameter changes.

Enumeration/open must succeed without any of these features. If it does not, stop and report the first root cause instead of expanding Phase 1.

## 19. Documentation and roadmap after successful validation

`docs/centuryarks_openeb_5_2_overlay_build.md` must record the exact preparation, configure, build, install, linkage and runtime commands; source identities; anonymized device identity; checks run; and checks not run.

All Phase 1 build/install and `0003` enumeration/open checks passed, so the roadmap states:

```text
Milestone 2C implementation/build:
Complete

Milestone 2C Phase 1 side-by-side plugin:
Complete

Milestone 2C OpenEB 5.2 PID 0003 enumeration/open:
Verified

Milestone 2C PID 0002 hardware:
Not tested

Milestone 2C PID 0004 hardware:
Not tested

Milestone 2C live event stream:
Not tested
```

Milestone 2 remains `In progress`. Compile/open success must not be described as completed CenturyArks camera support.

## 20. Rollback and update strategy

Rollback for the Architecture B experiment is operational:

- stop using the CenturyArks prepared source/build/install prefixes;
- leave canonical tracked `openeb/` and the existing base/RPATH outputs untouched;
- remove generated directories only after a separate path/size review and explicit cleanup authorization;
- revert tracked source-delivery inputs only through normal user-authorized Git history.

For a supplier update or OpenEB upgrade, create a new source manifest, compare every imported file and patch hunk, update the exact target-version guard, and repeat source, build, RPATH, registration and hardware acceptance. Do not force the existing 5.2 patch onto another OpenEB release.

## 21. Implementation stop conditions

Stop immediately if:

- canonical tracked `openeb/` becomes modified;
- the root or HDF5 ECF source identity changes;
- the prepared source output already exists;
- license or source-manifest verification fails;
- the patch requires offset/fuzz application, whole-file replacement or any hunk beyond the reviewed two-file CMake scope;
- a CenturyArks configure option appears or the new target is not always present in the prepared source;
- the CenturyArks plugin replaces or disables the standard plugin;
- either plugin target is renamed unexpectedly;
- any standard Prophesee source or registration changes;
- `0x31f7` is registered by both plugins;
- the CenturyArks source contains a fixed serial, serial hash, product or firmware gate;
- the CenturyArks plugin includes file discovery;
- hardware-layer object files are directly embedded in more than the shared hardware-layer target;
- a second hardware-layer target or dylib is created;
- enumeration requires EEPROM, pixel-mask, facility or system-information modifications;
- the physical `0003` camera is opened by the standard plugin;
- either plugin fails the no-camera loader/initialization smoke;
- the OpenEB 5.2 prefix resolves any OpenEB 5.1.1 runtime dependency;
- an install name or RPATH conflicts with the reviewed repository-local layout;
- Linux standard-plugin, V4L2 or non-Apple RPATH behavior changes;
- build, install, temporary, log or artifact output escapes the approved repository-local paths;
- the disk estimate reaches the authorization threshold or remaining-space protection line;
- a failure would require unreviewed broad OpenEB changes.

The reviewed source import, overlay preparation, configure, build, install and bounded camera enumeration/open validation were completed locally. No live stream, EEPROM/mask/facility work, Linux validation, commit, push or PR was performed as part of Phase 1.
