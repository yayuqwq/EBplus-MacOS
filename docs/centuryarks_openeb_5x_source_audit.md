# CenturyArks OpenEB 5.x source audit

## 1. Purpose and scope

This document records the read-only Milestone 2C-A audit of the CenturyArks SilkyEvCam source package against the preserved OpenEB 5.1.1 working tree and the repository's tracked OpenEB 5.2.0 source, followed by the bounded Milestone 2C-B1 Phase 1 implementation evidence.

The audit determines source identity, licensing evidence, the exact relationship to the known 5.1.1 port, hunk-level applicability to 5.2.0, and the constraints for a side-by-side integration. The original 2C-A work did not import vendor source, modify OpenEB, configure, build, install or claim camera support. The later Phase 1 work imported only the reviewed tracked inputs and validated the isolated prepared-source implementation without changing canonical tracked `openeb/`.

```text
Status at completion of Milestone 2C-A: Source audit/design complete
Phase 1 integration: Implemented and validated
CenturyArks build validation: Passed on macOS arm64
CenturyArks PID 0003 enumeration/open/reopen: Passed
CenturyArks PIDs 0002/0004 hardware: Not tested
Live event delivery: Not run
```

### Post-audit Milestone 2C-B1 architecture decision

The source audit originally established that a default-disabled plugin was technically possible. After the separate Milestone 2C-B0 hardware baseline, the fork selected a different implementation policy:

- Architecture B remains the source carrier, and canonical tracked `openeb/` remained unchanged.
- The ignored prepared OpenEB 5.2 source always builds `hal_plugin_prophesee` and `hal_plugin_centuryarks` side by side.
- Both plugins share the existing, singly owned `metavision_psee_hw_layer`.
- The implementation uses one CenturyArks build/install profile and no CenturyArks feature switch.
- The stable OpenEB 5.1.1 baseline verified one `31f7:0003` device can enumerate, open and reopen through `silky_common_plugin` / `CenturyArks`; live events were not validated.

The default-OFF and optional-plugin wording later in the original audit was a historical feasibility conclusion reached during Milestone 2C-A, not the current fork policy. The implemented and normative design is always-built in the prepared CenturyArks profile and has no CenturyArks feature option. Its requirements and validation results are defined in [`centuryarks_openeb_5_2_integration_plan.md`](centuryarks_openeb_5_2_integration_plan.md).

### Phase 1 implementation evidence

- Tracked delivery consists of the adapted entrypoint, license and provenance files, an exact two-file/two-hunk OpenEB 5.2 CMake patch, and a deterministic preparation script.
- Canonical `openeb/` stayed unchanged; the patch and entrypoint were applied only to the ignored prepared source.
- Configure, bootstrap build, full build and repository-local install passed.
- The existing hardware-layer object target produced 55 object files and remains the sole compilation/ownership source for the shared hardware layer. Neither plugin DSO directly embeds those hardware objects. The existing `metavision_active_pixel_detection` sample may reuse the same target outputs, which is not a second hardware-layer object compilation or target.
- The normalized standard-plugin link baseline and `psee_universal.cpp` source were unchanged.
- `hal_plugin_centuryarks` initializes as `CenturyArks`, exposes one live camera discovery and zero file discoveries, and registers only `31f7:0002`, `31f7:0003` and `31f7:0004` with subclass `0x19`. Interface class `0xff` and protocol `0x00` come from the existing Treuzell code. `hal_plugin_prophesee` retains two camera discoveries and one file discovery and does not register `0x31f7`.
- The adapted source contains no fixed serial, serial hash, product or firmware condition, file discovery, EEPROM, mask or facility behavior.
- No-DYLD CLI/loader, RAW/HDF5, RPATH/linkage and OpenEB 5.1.1 contamination checks passed.
- One physical `31f7:0003` IMX636 device enumerated, opened and reopened through `CenturyArks:hal_plugin_centuryarks:<runtime serial>`; the anonymized serial SHA-256 prefix is `cb823604ea92`, and the observed system release was `3.9.0`.
- Hardware for PIDs `0002` and `0004`, live event delivery, EEPROM, pixel-mask, facility behavior and Linux validation remain untested.

## 2. Source identity

| Field | Audited value |
| --- | --- |
| Source directory | `/Users/yayuqwq/Downloads/Package/SilkyEvCam_plugin_Source_for_MV511` |
| Package name | `SilkyEvCam_plugin_Source_for_MV511` |
| Supplier | CenturyArks, based on the supplied context and the copyright in `silky_common.cpp` |
| Communicated compatibility | OpenEB / Metavision 5.1.1, 5.2.0 and 5.3.x; supplied as vendor communication, not present in the package README |
| README-declared compatibility | `SilkyEvCam's Plugin Source files for OpenEB 5.1.1` |
| README integration method | Copy or overwrite the listed files in an OpenEB source tree, then use the normal OpenEB build |
| Regular files | 21 |
| Payload | 17 code/configuration/rules files, `README.txt`, `licensing/LICENSE_OPEN`, and two `.DS_Store` files |
| Allocated size | 176 KiB |
| License file | `licensing/LICENSE_OPEN` |

The package has no Git metadata, source commit, tag, publisher signature, checksum manifest, release identifier, download URL, or embedded provenance record. The package name and README identify a 5.1.1 overlay, not a standalone plugin project. The separately communicated version range is retained as a supplier claim, but it is not a build result and does not prove that the files can be copied unchanged into 5.2.0 or 5.3.x.

The ignored audit outputs are stored under:

```text
$REPO_ROOT/.logs/centuryarks-openeb-5x-audit/
```

They include the initial SHA-256 manifest, file metadata manifest, package-to-baseline comparison, and read-only patch applicability evidence. These files are not intended for commit.

The final SHA-256 manifest matched the initial manifest byte for byte: all 21 external source files remained unchanged during the audit.

## 3. License and copyright

`licensing/LICENSE_OPEN` contains the Apache License 2.0 text and no package-specific addendum.

The supplied license evidence supports the following bounded conclusions:

- The license text permits reproduction, modification and redistribution in source or object form, subject to its conditions.
- Recipients must receive a copy of the license.
- Modified files must carry prominent notices identifying the changes.
- Existing copyright, patent, trademark and attribution notices that remain relevant must be retained.
- A NOTICE file must be propagated only when the supplied Work actually contains one. This package contains no NOTICE file.
- The license does not grant permission to use CenturyArks, SilkyEvCam or Prophesee trade names, trademarks, service marks or product names beyond reasonable descriptive use.
- This audit makes no legal conclusion beyond the supplied license and file-header evidence.

Sixteen of the seventeen code/configuration files contain a Prophesee copyright and Apache-2.0 header. `hal_psee_plugins/src/plugin/silky_common.cpp` additionally contains:

```text
Copyright (c) 2024 CenturyArks Co.,Ltd.
```

The following files require special handling:

| File | Finding |
| --- | --- |
| `README.txt` | No copyright or license header; retained only as provenance evidence. |
| `hal_psee_plugins/resources/rules/ca_device.rules` | One-line udev rule with no copyright, license header or explicit modification notice. License/provenance clarification is recommended before redistribution. |
| `licensing/LICENSE_OPEN` | Standard Apache-2.0 text with no package-specific copyright owner declaration. |
| Two `.DS_Store` files | Apple metadata, not listed by the README and not suitable for import. |

Any later import must exclude `.DS_Store`, preserve the existing Prophesee and CenturyArks notices, identify EBplus modifications prominently, and retain the Apache-2.0 license. OpenEB's own third-party notices remain independently applicable.

## 4. Package contents

The README lists the following 17 functional payload files, all of which are present:

| Area | Files | Role |
| --- | --- | --- |
| HAL diagnostic sample | `hal/cpp/samples/metavision_platform_info/CMakeLists.txt`, `metavision_platform_info.cpp` | Adds a branded sample target and Linux USB diagnostic IDs. |
| Plugin build | `hal_psee_plugins/lib/CMakeLists.txt`, sample/plugin/device CMake files | Replaces the standard target topology with a Silky-specific one and changes source ownership. |
| USB/board layer | `tz_libusb_board_command.h/.cpp`, `psee_libusb.h/.cpp` | Adds EEPROM access and pixel-mask decoding. |
| Device layer | `imx636_tz_device.cpp`, `imx646_tz_device.cpp` | Applies 64 EEPROM mask entries during construction. |
| Identification | `tz_hw_identification.cpp` | Adds SilkyEvCam-specific firmware/system labels. |
| Plugin entrypoint | `src/plugin/silky_common.cpp` | Registers CenturyArks USB IDs and sets the plugin integrator. |
| Linux rule | `resources/rules/ca_device.rules` | Declares a udev permission rule for vendor ID `31f7`. |

The package does not contain a CenturyArks-specific device builder or sensor class. It relies on the existing Treuzell discovery, `TzDeviceBuilder`, Gen31, IMX636 and IMX646 implementations already present in OpenEB.

## 5. Comparison baselines

### A. CenturyArks package

```text
/Users/yayuqwq/Downloads/Package/SilkyEvCam_plugin_Source_for_MV511
```

### B. Preserved OpenEB 5.1.1 working tree

```text
Repository: /Users/yayuqwq/Metavision/openeb
Branch: macos/openeb-5.1.1-working
Commit: 01692d4c8a17c855358783eb4c21329f3492b6ef
Parent/tag: 0499c95d921755ab2085125a29750ef365e6e8ae / 5.1.1
Status during audit: clean
```

### C. Tracked OpenEB 5.2.0

```text
Repository path: $REPO_ROOT/openeb
Declared version: 5.2.0
Import commit: 613a498daeddecbe926741c78b7489740e867aa8
Imported OpenEB commit: 9003b5416676e78ba994d912087486cfa94fae73
Current repository baseline: main merge 84ababc928c5e221410d8776106e604488e06a13
```

The current tree also contains the validated Apple install-RPATH patch. Of the vendor package's same-path files, that patch changes:

- `openeb/hal/cpp/samples/metavision_platform_info/CMakeLists.txt`
- `openeb/hal_psee_plugins/lib/CMakeLists.txt`

## 6. Package-to-5.1.1 relationship

The relationship is stronger than a visual similarity:

- Seventeen of the eighteen code/license payload files are byte-for-byte identical to the corresponding blobs in `01692d4`.
- Those seventeen comprise fourteen files modified by `01692d4`, two files added by it (`ca_device.rules` and `silky_common.cpp`), and the unchanged official `LICENSE_OPEN`.
- The remaining package file, `hal_psee_plugins/src/boards/CMakeLists.txt`, differs semantically only by commenting out `add_subdirectory(v4l2)`.

`01692d4` also contains three changes that are not supplied by the CenturyArks package:

| External 5.1.1 change | Classification |
| --- | --- |
| `.gitignore` entries for `.DS_Store` and `prophesee/` | Local workspace maintenance; not CenturyArks functionality. |
| Deletion of `metavision_platform_info/CMakeLists.txt.install` | Not requested by the package and inconsistent with the remaining install rule; do not port. |
| Braces and `(void)main_dev` changes in `tz_device_control.cpp` | Compiler-warning adaptation, not CenturyArks functionality. |

The external build cache also contains uncommitted machine-specific compiler flags and `/usr/local` installation settings. Consequently, `01692d4` is a preserved working environment, not a self-contained, reproducible CenturyArks patch set.

## 7. Package-to-5.2 relationship

For every package path that exists in both official OpenEB baselines, the 5.1.1 and 5.2.0 blob IDs are identical. Relevant helper APIs are also unchanged, including:

- `TzCameraDiscovery::add_usb_id`
- `initialize_psee_plugin(Plugin &, std::string)`
- `I2cEeprom::read`
- `Gen41DigitalEventMask::get_pixel_masks` and `set_mask`
- `TzPseeFpgaDevice::get_system_version`
- the IMX636 and IMX646 constructor and facility-spawn structure

This is evidence that the vendor hunks target source structures still present in 5.2.0. It is not evidence that the overlay is safe as an integration architecture.

A read-only `git apply --check` of the exact 5.1.1 vendor-file patch against the current tracked `openeb/` failed at the two CMake files already modified by the Apple RPATH work. More importantly, even hunks that apply textually would still replace the standard plugin, remove V4L2 paths, or activate vendor behavior unconditionally.

## 8. Hunk-level mapping

Only the five conclusion values defined for this milestone are used below.

| CenturyArks file/function | Purpose | OpenEB 5.2 destination | 5.2 structural difference | Conclusion | Default-build impact | Linux impact | Standard Prophesee impact | Required validation |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `metavision_platform_info/CMakeLists.txt:13-15` | Add duplicate `silkyevcam_platform_info` target | Existing sample CMake | Current target has validated Apple install RPATH; vendor alias is not installed | Not applicable | Avoid duplicate target | None if omitted | None | No test required unless a branded diagnostic is separately requested |
| `metavision_platform_info.cpp:318-320` | Add `31f7` IDs to `lsusb` diagnosis | Existing sample source | Same source structure; hunk is inside Linux diagnostics | Not applicable | None | Optional diagnostic only | None | Linux-only diagnostic test if later requested |
| `hal_psee_plugins/lib/CMakeLists.txt:41-100` | Rename shared hardware layer | Existing plugin build CMake | Current standard target/export and Apple RPATH must remain | Not applicable | Direct copy changes ABI/target names | Breaks existing consumers | Replaces standard hardware-layer identity | Do not implement |
| `hal_psee_plugins/lib/CMakeLists.txt:177-273` | Replace standard plugin with `silky_common_plugin` | Existing plugin-list loop | Current loop supplies install/copy/RPATH behavior | Port with OpenEB 5.2 adaptation | Always add only the side target in the prepared profile; canonical `openeb/` remains unchanged | Existing `$ORIGIN` path retained; Linux build not run | Standard plugin remains enabled | Passed target graph, ownership, normalized link, install and RPATH audit |
| `tz_libusb_board_command.h::read_pixel_mask_data` | Expose mask-record decoding | Same class | API exists only in vendor file | Port with OpenEB 5.2 adaptation | Not present in Phase 1; any future implementation requires a device-identity gate | Cross-platform | Must reject non-CenturyArks devices | Compile, bounds and unit tests plus hardware evidence |
| `psee_libusb.h::eeprom_read_4bytes` | Read four EEPROM bytes | Same class | Generic control transfer and `I2cEeprom` already exist | Port with OpenEB 5.2 adaptation | Not present in Phase 1; any future helper requires a device-identity gate | Cross-platform libusb | Must not issue vendor transfers for standard devices | Mocked transfer/error tests and hardware read |
| `ca_device.rules` | Linux access for VID `31f7` | Linux rules directory | File is not referenced by the existing rules CMake | Needs clarification or hardware evidence | None on macOS | `MODE="666"` and installation policy need security review | No direct device overlap | Linux udev and permission validation |
| Facility-casting sample CMake | Link renamed hardware layer | Existing sample CMake | Rename is rejected | Not applicable | None | None | Preserves standard target | No test required |
| `src/boards/CMakeLists.txt:14` | Disable V4L2 board directory | Existing CMake | V4L2 subdirectory already returns when unsupported | Already present in OpenEB 5.2 | Default behavior already safe on macOS | Direct copy would remove Linux functionality | Removes standard V4L2 support | Preserve current file |
| `TzHWIdentification::get_system_info` | Silky firmware labels and FPGA version | Same function | APIs unchanged; vendor uses brittle product-name parsing and suppresses normal device info | Port with OpenEB 5.2 adaptation | Deferred, nonessential phase | Cross-platform | Must preserve normal information for standard devices | Model-string, metadata and non-vendor regression tests |
| `TzLibUSBBoardCommand::read_pixel_mask_data` | Decode EEPROM word | Same class/source | No version API change; vendor uses magic layout | Port with OpenEB 5.2 adaptation | Not present in Phase 1; any future implementation requires a device-identity gate | Cross-platform | Must not touch standard EEPROM | Unit decode tests and hardware evidence |
| `LibUSBDevice::eeprom_read_4bytes` | EEPROM address `0x50` read | Same class/source | Existing `I2cEeprom::read` resizes the buffer; wrapper is absent | Port with OpenEB 5.2 adaptation | Not present in Phase 1; any future helper requires a device-identity gate | Cross-platform | Must not change generic transfers | Short-read, error, endian and timeout tests |
| `src/devices/CMakeLists.txt` | Remove V4L2 device directory | Existing CMake | Guarded V4L2 subdirectory already provides platform isolation | Already present in OpenEB 5.2 | No change required | Direct copy breaks Linux | Removes standard device support | Preserve current file |
| `TzImx636` constructor mask loop | Apply 64 EEPROM masks | Existing constructor/facility creation | Registered mask facility is created in `spawn_facilities` | Port with OpenEB 5.2 adaptation | Not present in Phase 1; only a future explicit CenturyArks device gate could permit it | Cross-platform | Unguarded copy performs 64 vendor reads on standard IMX636 | Facility-lifecycle, coordinate and hardware mask tests |
| `TzImx646` constructor mask loop | Apply 64 EEPROM masks | Existing constructor/facility creation | Same as IMX636 | Port with OpenEB 5.2 adaptation | Not present in Phase 1; only a future explicit CenturyArks device gate could permit it | Cross-platform | Unguarded copy performs 64 vendor reads on standard IMX646 | Facility-lifecycle, coordinate and hardware mask tests |
| `devices/others/CMakeLists.txt` | Move `i2c_eeprom.cpp` into hardware-layer object | Existing source ownership rule | Needed because the new caller is in the hardware-layer object | Port with OpenEB 5.2 adaptation | Conditional ownership only | Cross-platform | Avoid duplicate DSO symbols and default boundary changes | Link-map and duplicate-symbol audit |
| `src/plugin/CMakeLists.txt` | Replace `psee_universal.cpp` | Existing plugin source wiring | Standard source and `HAS_V4L2` definition remain required | Port with OpenEB 5.2 adaptation | Add a distinct, always-built prepared-profile source without detaching the standard source | Existing V4L2 branch preserved; Linux build not run | Standard source remains byte-identical | Passed single-profile configure/build and source/link comparison |
| `silky_common.cpp` registration | Register `31f7:0002/0003/0004`, subclass `0x19`, integrator `CenturyArks` | New side-by-side plugin source | Plugin API is unchanged and the custom-integrator overload already exists | Port with OpenEB 5.2 adaptation | Always built in the prepared profile; behavior remains restricted to the three registered IDs | Structurally cross-platform; Linux build not run | IDs remain exclusive to vendor plugin | Passed static/object-code audit, loader probe and PID `0003` enumeration/open/reopen |
| `silky_common.cpp` PSEE file discovery | Open RAW files through vendor plugin | Side-by-side plugin source | Standard plugin already registers the same file discovery | Needs clarification or hardware evidence | Omitted from Phase 1 | Cross-platform | Duplicate fallback order may change reported plugin identity | Synthetic/legacy CenturyArks RAW header tests |
| `licensing/LICENSE_OPEN` | Apache-2.0 license | Existing `openeb/licensing/LICENSE_OPEN` | File is byte-identical | Already present in OpenEB 5.2 | None | None | None | Preserve notices for imported vendor-derived source |

## 9. Device registration findings

`silky_common.cpp` registers exactly:

```text
VID 0x31f7 / PID 0x0002 / interface class 0xFF / subclass 0x19
VID 0x31f7 / PID 0x0003 / interface class 0xFF / subclass 0x19
VID 0x31f7 / PID 0x0004 / interface class 0xFF / subclass 0x19
```

The existing Treuzell board command additionally requires protocol `0` and three bulk endpoints. The package does not map the three PIDs to named SilkyEvCam models and does not add device-builder registrations. Existing firmware/device descriptors must therefore select the already registered Gen31, IMX636 or IMX646 build methods.

The standard 5.2 plugin currently registers different IDs (`03fd:5832`, `04b4:00f4/00f5`, and `1fc9:5838`). Side-by-side plugins can therefore avoid live-device duplication if `31f7` remains exclusively owned by the CenturyArks plugin. Adding the same IDs to both plugins would make enumeration and open order-dependent.

## 10. EEPROM and pixel-mask chain

The vendor chain is:

```text
I2cEeprom::read
  -> LibUSBDevice::eeprom_read_4bytes
    -> TzLibUSBBoardCommand::read_pixel_mask_data
      -> TzImx636 / TzImx646
        -> Gen41DigitalEventMask::set_mask
```

The encoded layout is:

- EEPROM I2C address `0x50`.
- Mask records in blocks 2 through 65, corresponding to byte offsets 8 through 263.
- Four bytes per record.
- The read word is XORed with `0xffffffff`.
- Bit 31 marks a valid record.
- X uses bits 0 through 10.
- Y uses bits 16 through 26.
- Up to 64 mask registers are programmed.

The 5.2 APIs used by this chain are present. The missing evidence is behavioral: byte order, valid-bit convention, coordinate ranges, EEPROM failure semantics, whether all three PIDs use the same layout, and whether the masks must be applied before or during facility registration. No non-CenturyArks IMX device may execute these vendor reads.

## 11. Plugin and build structure

The package is a replacement topology:

```text
metavision_psee_hw_layer_obj
  -> metavision_silky_hw_layer
  -> silky_common_plugin
```

It renames the standard hardware-layer dylib, removes `hal_plugin_prophesee` from the plugin list, embeds hardware-layer object files directly into the vendor plugin, and also links the renamed hardware-layer dylib. This risks duplicated symbols and breaks the standard target/export identity.

The implemented Phase 1 topology is instead:

```text
metavision_psee_hw_layer          shared, existing target
  <- hal_plugin_prophesee         existing and unchanged
  <- hal_plugin_centuryarks       always built, side by side
```

The existing plugin loop applies the current `@loader_path` and prefix-relative Apple RPATH to both plugins because both targets are in the same target list. The installed audit confirmed the required RPATH entries. No hardware-layer rename or duplicate dylib was created.

The build produced one `metavision_psee_hw_layer_obj` target with 55 object files, owned by the existing shared `metavision_psee_hw_layer`. Neither plugin DSO directly embeds those objects. The existing `metavision_active_pixel_detection` sample can reuse the already produced target outputs; that consumer does not create a second hardware-layer object target or compile a second set of the 55 objects.

## 12. Platform-specific content

| Classification | Package content |
| --- | --- |
| macOS-required | None. The package adds no Apple-only code or RPATH behavior. |
| Linux-only | `lsusb` diagnostic IDs and `ca_device.rules`. |
| Cross-platform device support | Plugin registration, EEPROM/mask chain and system-information customization. |
| Sample-only | `silkyevcam_platform_info` target and facility-sample relink. |
| Unrelated/local | V4L2 removal, `.DS_Store`, external `.gitignore`, deleted sample recipe and warning suppression. |

V4L2 must remain enabled in the default Linux source graph and continue to self-disable through `HAS_V4L2` on unsupported platforms. The vendor file's unconditional removal is not a macOS requirement.

## 13. Linux and standard-device risks

- Wholesale copying removes Linux V4L2 source paths and discovery.
- The udev rule is not wired into the install graph and grants world read/write access with `MODE="666"`; both provenance and policy need review.
- Replacing the standard plugin removes the standard Prophesee USB, FX3 and V4L2 discovery registrations.
- The vendor mask loop is not device-gated and would affect standard IMX636/646 devices if shared unchanged.
- Duplicate registration of `31f7` across plugins would make plugin-loader ordering observable.
- Duplicate file discoveries may affect which plugin identity is reported for legacy RAW files.
- Renaming `metavision_psee_hw_layer` breaks exported target and sample assumptions.

## 14. Version claims and limitations

```text
Vendor package identity:
SilkyEvCam_plugin_Source_for_MV511

Communicated compatibility:
OpenEB / Metavision 5.1.1, 5.2.0 and 5.3.x

README-declared compatibility:
OpenEB 5.1.1

Audited integration target:
OpenEB 5.2.0 commit 9003b5416676e78ba994d912087486cfa94fae73,
plus the repository's current Apple RPATH patch

Validated CenturyArks build target:
OpenEB 5.2.0 prepared source on macOS arm64: configure, bootstrap build,
full build and repository-local install passed

Validated CenturyArks hardware:
PID 0003 / IMX636: enumeration, open, `--system` and reopen passed;
serial SHA-256 prefix `cb823604ea92`; observed system release `3.9.0`

Unvalidated CenturyArks hardware and behavior:
PIDs 0002 and 0004, live event delivery, EEPROM, pixel masks and facilities
```

The exact blob comparison supports a focused 5.2.0 port. It does not establish compatibility with every OpenEB 5.x release, and no 5.3.x source was audited. A future implementation should reject versions other than the specifically audited target until a new source audit is completed.

## 15. Open questions

- Which PID corresponds to VGA, HD V03 and HD Lite?
- Do all three PIDs use the same EEPROM address, record layout and 64-entry mask count?
- Is the `-C` firmware suffix contractual or display-only?
- Is suppressing normal per-device information required?
- Must legacy SilkyEvCam RAW files resolve through a plugin named `silky_common_plugin`, or is matching the `CenturyArks` integrator sufficient?
- What permission model should replace or justify the supplied `MODE="666"` udev rule?
- Does CenturyArks authorize redistribution of the headerless udev rule as part of the Apache-2.0 package?
- Is 5.3.x compatibility tied to a newer package revision not present here?

## 16. Audit conclusion

**Can the vendor files be copied directly over OpenEB 5.2?** No. Direct copying would replace the standard plugin and hardware-layer target, disable V4L2, overwrite validated Apple RPATH changes and enable vendor behavior without an isolation gate.

**Can the standard Prophesee plugin remain enabled?** Yes. The live USB IDs are currently disjoint. A separate CenturyArks plugin can exclusively own `31f7:0002/0003/0004` while the standard plugin remains unchanged.

**Can CenturyArks support be default OFF?** A default-OFF design was technically feasible in the original 2C-A analysis. The current fork deliberately does not use that design: the prepared CenturyArks profile always builds both plugins and exposes no CenturyArks feature option. Device behavior is still limited by the CenturyArks plugin's exclusive VID/PID registrations, while canonical tracked `openeb/` remains unchanged.

**Can the integration be compiled without hardware?** Yes, and this was demonstrated before camera use: configure, bootstrap/full build, install, target ownership, symbols, plugin loading, RPATH, static registration and the RAW/HDF5 regressions all passed independently of the physical camera acceptance.

**What remains impossible to validate without a camera?** USB enumeration/open and runtime system identity require hardware; these passed for one PID `0003` IMX636 instance, including a normal-exit reopen smoke. PIDs `0002` and `0004`, live event delivery, EEPROM transactions, mask decoding and physical effect, facilities, parameter changes and unplug/replug behavior remain unvalidated.

The implementation architecture and stop conditions are defined in [`centuryarks_openeb_5_2_integration_plan.md`](centuryarks_openeb_5_2_integration_plan.md).
