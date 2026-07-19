# CenturyArks OpenEB 5.2 optional integration plan

## 1. Recommended architecture

The next implementation stage should combine two concerns that the three candidate architectures describe separately:

- **Architecture B for source delivery and experimental isolation:** a tracked, hunk-level vendor overlay prepares a deterministic ignored OpenEB 5.2.0 source copy.
- **Architecture C for runtime topology:** the prepared source builds an optional side-by-side `hal_plugin_centuryarks` while retaining `hal_plugin_prophesee` unchanged.

This B+C design is recommended for the first implementation and no-hardware build stage. It protects the canonical tracked `openeb/` baseline while producing the runtime topology intended for a long-term integration.

```text
hal_plugin_prophesee       always built by the existing profile
hal_plugin_centuryarks     built only when explicitly enabled
metavision_psee_hw_layer   shared existing hardware-layer target
```

After real hardware validates enumeration, open, streaming and the EEPROM/mask requirements, the same C topology may be considered for a guarded tracked A+C landing. That later decision requires a separate review; it is not implied by this plan.

## 2. Rejected alternatives

### Vendor README wholesale overwrite

Rejected. It would:

- rename and replace `metavision_psee_hw_layer`;
- remove `hal_plugin_prophesee` from the default plugin list;
- remove standard FX3, USB and V4L2 discovery from the active plugin;
- disable Linux V4L2 directories;
- overwrite the validated Apple RPATH patch;
- duplicate hardware-layer object code inside the vendor plugin;
- enable EEPROM/mask reads for standard IMX devices without a vendor identity guard.

### Architecture A as the first experimental carrier

Direct guarded changes in tracked `openeb/` could eventually provide a clean final integration, but they are not the preferred first carrier while no CenturyArks camera is available. The EEPROM chain modifies shared hardware-layer ownership and lifecycle. Landing those changes before hardware evidence would increase the upgrade and regression burden of the canonical OpenEB snapshot.

### Architecture B with full replacement files

Rejected. The tracked overlay must contain adapted hunks and new vendor-owned source, not the supplied 5.1.1 full-file replacements. Full replacement files would silently discard future OpenEB and EBplus RPATH changes.

### Architecture C with a copied private hardware layer

Rejected. Copying the complete PSEE hardware layer into a second plugin would duplicate a large shared implementation, create symbol/ABI drift, and make future OpenEB upgrades substantially harder. The side plugin should share `metavision_psee_hw_layer` and use narrowly gated vendor hooks.

## 3. Target directory layout

Proposed tracked inputs for the next implementation branch:

```text
third_party/centuryarks/silkyevcam-openeb-5x/
├── SOURCE_IDENTITY.md
├── LICENSE_OPEN
├── manifests/
│   └── vendor-source.sha256
├── patches/
│   └── openeb-5.2.0/
│       ├── 0001-add-optional-centuryarks-plugin.patch
│       ├── 0002-add-gated-centuryarks-eeprom-mask.patch
│       └── 0003-add-optional-centuryarks-metadata.patch
└── src/
    └── centuryarks_plugin.cpp

scripts/
└── prepare_centuryarks_openeb_source.sh
```

Proposed ignored outputs:

```text
$REPO_ROOT/.tmp/openeb-5.2.0-centuryarks-source
$REPO_ROOT/.build/openeb-5.2.0-centuryarks-macos
$REPO_ROOT/.deps/openeb-5.2.0-centuryarks-macos
$REPO_ROOT/.logs/centuryarks-openeb-5.2.0-macos
$REPO_ROOT/.artifacts/centuryarks-openeb-5.2.0-macos
```

The script must refuse to overwrite any existing output path and must never modify `$REPO_ROOT/openeb`.

## 4. Feature option and defaults

The prepared source should define:

```cmake
option(ENABLE_CENTURYARKS_PLUGIN
       "Build optional CenturyArks SilkyEvCam support"
       OFF)
```

The option must have these properties:

- `OFF` creates no CenturyArks target, registration, installed plugin, compile definition or udev rule.
- `OFF` retains the same standard plugin target graph and install layout as the current OpenEB 5.2 baseline.
- `ON` adds only the optional vendor plugin and the minimal shared hooks required by that plugin.
- `ON` does not rename, disable or replace `hal_plugin_prophesee` or `metavision_psee_hw_layer`.
- Linux and macOS use the same default (`OFF`).
- Linux udev installation, if later accepted, uses a separate explicit option and remains off by default.

Until another version is audited, the prepared source should reject an enabled option when:

```cmake
PROJECT_VERSION is not exactly 5.2.0
```

The preparation script, rather than CMake, should verify the audited source-file hashes and imported OpenEB identity.

## 5. Source import strategy

The implementation must not use the Downloads directory as a build dependency.

The authorized future import should:

1. Record the supplier package name, communicated compatibility, README claim, file sizes and SHA-256 values.
2. Import the Apache-2.0 license and the vendor-owned plugin entrypoint source with its Prophesee and CenturyArks notices intact.
3. Add a prominent notice to every modified or derived file identifying the EBplus/OpenEB 5.2 adaptation.
4. Exclude both `.DS_Store` files.
5. Exclude the branded sample alias and V4L2 removal hunks.
6. Defer `ca_device.rules` until its provenance and permission policy are reviewed.
7. Represent modifications to existing OpenEB files as reviewable patch hunks, not copied whole files.

The preparation script should create the ignored source from Git-controlled inputs:

- materialize the tracked `openeb/` tree from the root repository;
- materialize `hdf5_ecf` separately from its pinned commit because a root archive does not contain submodule content;
- do not copy the submodule's `.git` file or any root `.git` metadata;
- verify OpenEB version, audited file hashes and the HDF5 ECF pinned commit;
- run patch checks before applying any hunk;
- stop without cleaning partial output if any identity or patch check fails.

This avoids copying ignored build products or using a relative submodule gitdir that would be invalid inside the prepared source copy.

## 6. OpenEB 5.2 hunk mapping and implementation phases

### Phase 1: side plugin and registration

Implement first:

- add `hal_plugin_centuryarks` to the existing plugin list only when enabled;
- compile an adapted `centuryarks_plugin.cpp` entrypoint;
- register only `31f7:0002`, `31f7:0003` and `31f7:0004`, subclass `0x19`;
- set plugin integrator identity to `CenturyArks`;
- keep `hal_plugin_prophesee` and its registrations unchanged;
- initially omit the duplicate `PseeFileDiscovery` registration.

This phase is sufficient for compile/link validation and for later live-device enumeration/open testing. It does not require EEPROM or metadata changes.

### Phase 2: shared hardware-layer guard

Before adding EEPROM behavior, introduce one audited way for the board command to identify a CenturyArks USB device. Prefer explicit stored VID/PID identity over product-name parsing.

Requirements:

- only VID `0x31f7` and the three audited PIDs return true;
- the standard plugin cannot accidentally mark a standard Prophesee device as CenturyArks;
- the identity API has no behavior when the option is off;
- tests cover all accepted and rejected IDs.

### Phase 3: EEPROM and mask decoding

Add the vendor chain with adaptations:

- reuse `I2cEeprom::read` without duplicating the implementation;
- use a bounded return type rather than unvalidated output pointers where practical;
- validate mask index 0 through 63;
- validate X/Y against the selected sensor geometry;
- preserve clear error/fallback behavior for missing or invalid EEPROM data;
- perform no EEPROM transfer until the board identity is confirmed as CenturyArks;
- conditionally place `i2c_eeprom.cpp` in the hardware-layer link boundary without compiling it into two DSOs.

Initialize the same `Gen41DigitalEventMask` instance that is registered with `DeviceBuilder`, rather than creating a separate temporary instance. Apply this independently to IMX636 and IMX646 and retain all existing facility construction order outside the guarded block.

### Phase 4: optional identity presentation

Treat the vendor `TzHWIdentification::get_system_info` changes as a separate, non-blocking feature:

- detect CenturyArks through explicit plugin/device identity, not fixed product-name substrings;
- preserve standard per-device information;
- add vendor firmware fields without suppressing generic fields unless hardware evidence requires it;
- add Gen31 FPGA version only when the corresponding device exists;
- do not make branded output a prerequisite for enumeration, open or streaming.

### Phase 5: RAW compatibility, if required

Add `PseeFileDiscovery` to `hal_plugin_centuryarks` only after tests establish the desired behavior for:

- new recordings created by `hal_plugin_centuryarks`;
- legacy recordings naming `silky_common_plugin`;
- recordings with `CenturyArks` integrator but no plugin name;
- recordings with no plugin/integrator identity.

## 7. Standard plugin coexistence

The following invariants are mandatory:

- `hal_plugin_prophesee` remains in the default and opt-in builds.
- The standard plugin retains its USB, FX3, V4L2 and RAW registrations.
- Only `hal_plugin_centuryarks` registers VID `0x31f7`.
- No target links the complete hardware-layer object files both directly and through the shared hardware-layer dylib.
- Full serial identifiers remain distinct through `integrator:plugin:serial`.
- A static or unit test fails if an upstream standard plugin later adds one of the CenturyArks VID/PID pairs without an explicit coexistence decision.

The OpenEB plugin loader does not guarantee a useful ordering for duplicate device registrations. Exclusive VID/PID ownership is therefore a correctness requirement, not merely a naming preference.

## 8. Linux baseline protection

- Default Linux configure/build behavior must be identical with the option off.
- Do not comment out or remove any V4L2 subdirectory.
- Preserve the existing `HAS_V4L2` feature detection and compile definitions.
- Preserve the non-Apple `$ORIGIN` plugin RPATH branch.
- Do not add the CenturyArks IDs to the standard plugin.
- Do not install `ca_device.rules` unless a separately reviewed Linux-only option is enabled.
- Review the supplied `MODE="666"` rule before use; do not silently install it to `/etc/udev/rules.d`.
- A Linux configure/build regression remains required before a final tracked A+C landing.

## 9. macOS RPATH interaction

The optional target should be added to the current plugin-list loop so it inherits the validated behavior:

```text
Plugin install RPATH:
@loader_path
@loader_path/../../..

Shared hardware-layer install RPATH:
@loader_path/../../..
```

No new absolute RPATH, `/usr/local`, `/opt/homebrew` or repository path may be introduced. The optional install must pass the same no-`DYLD_LIBRARY_PATH` and full `otool -L/-l` audit as the base build.

## 10. Build profiles and output paths

The next implementation milestone should preserve the current successful OpenEB 5.2 profile and change only:

- the prepared source directory;
- the new isolated build/install/log/artifact paths;
- `ENABLE_CENTURYARKS_PLUGIN=ON` for the opt-in profile.

Two profiles are required:

| Profile | Option | Purpose |
| --- | --- | --- |
| Prepared-source baseline | `OFF` | Prove the overlay infrastructure itself does not change target graph, install layout or runtime behavior. |
| CenturyArks opt-in | `ON` | Build and validate the additional plugin and guarded hardware hooks. |

Before creating the source copy or build trees, repeat Git, dependency, source-identity and disk preflight. Do not reuse or delete the existing base/RPATH comparison directories.

## 11. No-hardware compile acceptance

The integration can and should be compiled without a camera. Required acceptance includes:

1. Prepared-source identity and patch checks pass.
2. Option-OFF configure/build/install matches the base OpenEB target and install-file sets.
3. Option-ON configure succeeds without additional downloads.
4. `hal_plugin_prophesee`, `hal_plugin_centuryarks` and `metavision_psee_hw_layer` all build.
5. The standard plugin remains installed.
6. The vendor plugin contains exactly the three expected live USB registrations.
7. No duplicate symbols or duplicate hardware-layer object ownership appears in link commands.
8. Both plugins load in `PLUGIN_PATH_ONLY` mode with no camera connected.
9. No-device `metavision_platform_info --short` and `--system` reach discovery without a loader failure.
10. The three required CLI remain directly runnable without DYLD overrides.
11. Standard RAW and HDF5 regression remains unchanged.
12. All installed Mach-O objects pass RPATH and 5.1.1 contamination checks.
13. Unit tests cover VID/PID gating and pure EEPROM mask-word decoding without USB hardware.

Passing these checks means the integration compiles and loads. It does not mean SilkyEvCam hardware is supported.

## 12. Hardware acceptance deferred

With a matching camera available, validate each supported PID separately where hardware permits:

```text
Device enumeration
Device open
Correct plugin/integrator identity
PID-to-model mapping
Firmware and system information
EEPROM read success/failure
Pixel-mask coordinates and physical effect
Live event stream
Facility access
Parameter changes
Clean shutdown
Reconnect
Standard Prophesee coexistence
```

No-device discovery, compile success or supplier compatibility communication can substitute for these checks.

## 13. Licensing and third-party notices

- Retain the Prophesee and CenturyArks copyrights from `silky_common.cpp`.
- Add prominent modification notices to adapted source and patch files.
- Include Apache License 2.0 with the imported vendor material.
- Review OpenEB's `OPEN_SOURCE_3RDPARTY_NOTICES` obligations independently.
- Do not import `.DS_Store`.
- Do not import or distribute `ca_device.rules` until its headerless provenance and permission policy are accepted.
- Use CenturyArks and SilkyEvCam names only to identify source origin and intended hardware; do not imply endorsement or trademark permission.

## 14. Rollback and update strategy

For the B+C experiment, rollback is operationally simple:

- stop using the CenturyArks-specific source/build/install prefix;
- leave the canonical tracked `openeb/` and existing base/RPATH outputs untouched;
- remove generated directories only after a separate path-and-size review and explicit cleanup authorization;
- revert tracked overlay files through normal Git history only if a later user-authorized change was committed.

For a supplier update or OpenEB upgrade:

1. create a new source manifest;
2. compare every vendor file and hunk against the previous package;
3. audit the new exact OpenEB target version;
4. regenerate adapted patches instead of forcing old patches;
5. rerun option-OFF, option-ON, Linux, RPATH and hardware acceptance.

Do not label the integration as compatible with all OpenEB 5.x releases.

## 15. Implementation stop conditions

Stop the next implementation immediately if any of the following occurs:

- the canonical tracked `openeb/` source is dirty or does not match the audited source manifest;
- OpenEB is not exactly the reviewed 5.2.0 baseline;
- the HDF5 ECF gitlink or pinned source is unavailable or changed;
- the preparation output path already exists;
- a patch requires whole-file replacement or overwrites the Apple RPATH changes;
- option OFF changes the standard target graph, plugin list or install layout;
- `hal_plugin_prophesee` is disabled, renamed or loses a registration;
- VID `0x31f7` is registered by more than one plugin;
- the link graph embeds hardware-layer object files twice;
- vendor EEPROM reads can run on a non-CenturyArks device;
- Linux V4L2 behavior changes in the default profile;
- an install/runtime path points to `/usr/local` OpenEB 5.1.1 or to a build tree;
- license/provenance review blocks an imported file;
- the disk estimate reaches the authorization threshold or the remaining-space protection line;
- a compile failure would require broad, unreviewed OpenEB source changes.

The source evidence behind this plan is recorded in [`centuryarks_openeb_5x_source_audit.md`](centuryarks_openeb_5x_source_audit.md).
