# CenturyArks SilkyEvCam OpenEB source identity

## Audited supplier package

- Package name: `SilkyEvCam_plugin_Source_for_MV511`
- Supplier: CenturyArks, as identified by the supplied package context and source copyright
- Package README claim: OpenEB 5.1.1
- Communicated compatibility: OpenEB / Metavision 5.1.1, 5.2.0 and 5.3.x; this is a supplier statement supplied by the user, not a build result contained in the package
- Audited package contents: 21 regular files, 135,102 bytes
- Imported OpenEB upstream commit: `9003b5416676e78ba994d912087486cfa94fae73`
- Current EBplus OpenEB baseline commit: `708a36e1829d126a5c51ec9c99630d2014ad92f4`
- Canonical `openeb/` tree at that baseline: `b407c407aa46d3b97edc9b2096fb120a96c8b465`
- Audited integration target: OpenEB 5.2.0 plus the repository's Apple install-RPATH changes
- Source audit: [`docs/centuryarks_openeb_5x_source_audit.md`](../../../docs/centuryarks_openeb_5x_source_audit.md)

The supplier package has no embedded Git commit, tag, publisher signature, release identifier, or checksum manifest. The repository manifest in [`manifests/vendor-source.sha256`](manifests/vendor-source.sha256) records the complete package observed during the read-only audit; it is an EBplus audit record, not a supplier signature.

## Tracked payload in this directory

Only the following supplier-derived payload is retained:

- `LICENSE_OPEN` is a byte-for-byte copy of the supplied `licensing/LICENSE_OPEN` file.
- `src/centuryarks_plugin.cpp` is derived only from the supplied `hal_psee_plugins/src/plugin/silky_common.cpp` entrypoint. It preserves the original Prophesee and CenturyArks notices and adds a prominent 2026 modification notice.

The adapted entrypoint is intentionally limited to live-device registration for `31f7:0002`, `31f7:0003`, and `31f7:0004`, USB subclass `0x19`, with plugin integrator identity `CenturyArks`. It does not import the supplier's RAW file discovery, sample alias, Linux USB diagnostics, udev rule, product or firmware presentation, EEPROM access, pixel-mask behavior, facility changes, or replacement build topology.

The two `.DS_Store` files and all other supplier package files are excluded from the tracked payload. Their hashes remain in the complete package manifest so that a later supplier package can be compared against the audited identity.

## Hardware evidence

- Before Phase 1, one physical `31f7:0003` device enumerated, opened, and reopened under the preserved OpenEB 5.1.1 CenturyArks baseline.
- Phase 1 subsequently verified OpenEB 5.2 enumeration, open, system reporting, and a normal-exit reopen smoke for one physical `31f7:0003` device.
- PIDs `0002` and `0004`, live event delivery, EEPROM behavior, pixel masks, and vendor-specific facilities remain unvalidated on hardware.
- No full device serial is recorded here, and no device serial or serial hash is a source, build, registration, or runtime condition.

## Source hashes

```text
Supplied licensing/LICENSE_OPEN:
ab1119dedc6ca90aef94f95ad78b10580cca0a1de76e3ca3052b166be8399f03

Supplied hal_psee_plugins/src/plugin/silky_common.cpp:
461a0c8d405d2820e31b3c08715974a9a7136cada2c8855e995b87fa46745fe4
```

These inputs do not establish build compatibility or hardware support. Configure, build, plugin loading, device enumeration, device open, streaming, EEPROM and pixel-mask behavior require separate validation.

The supplied evidence does not establish official certification or endorsement by CenturyArks, Prophesee, or another party.
