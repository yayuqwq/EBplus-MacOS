# macOS HDF5/H5 Generic Offline File Validation

## 1. Status

**Status:** Passed — bounded macOS arm64 build-tree HDF5/H5 generic-offline
compatibility validation for one known ECF fixture after the file-facility
compatibility fix.

Milestone 5 remains <code>Planned</code>.

## 2. Environment / source identity

This record is limited to:

- macOS Apple Silicon arm64;
- branch <code>fix/generic-offline-file-facilities</code> based on main
  <code>36576ce6b3c4143933cf56088c47f6c96643c4c1</code>;
- the build-tree executable <code>.build/ebplus-macos/gui/gui_for_openeb</code>;
- the repository-local OpenEB 5.2 CenturyArks profile; and
- fresh, repository-local <code>.hdf5</code> and <code>.h5</code> working
  copies of one known ECF fixture.

No installed executable, physical camera, physical facility mutation, DAT,
algorithm, model, export, or Linux workflow was exercised by this validation.

## 3. Pre-fix defect

Before this fix, opening the exercised <code>.h5</code> fixture in the GUI
produced:

~~~text
Metavision SDK Stream exception
Error 102113: Device unavailable.
~~~

The existing <code>BaseException</code> file-open boundary converted this into
a controlled detailed dialog. No <code>SIGABRT</code> was observed for this
HDF5/H5 defect.

The root cause was a HDF5/H5 generic offline source without a HAL
<code>Device</code>. GUI setup emitted <code>connected</code>; hardware-oriented
panels then queried facilities; and the <code>CameraController</code> facility
accessors unconditionally called <code>get_device()</code> before the nullable
<code>get_facility()</code> semantics could apply.

## 4. Shared OpenEB semantics

The audited OpenEB source dispatches:

- <code>.hdf5</code> and <code>.h5</code> to <code>OfflineGenericPrivate</code>;
- <code>.dat</code> to <code>OfflineGenericPrivate</code>; and
- <code>.raw</code> to the separate <code>OfflineRawPrivate</code> path.

<code>OfflineGenericPrivate::device()</code> has no HAL Device and throws
<code>DeviceUnavailable</code>. The audited generic-offline source has no
Apple/Linux branch for this behavior; only HDF5 feature availability is
conditional.

This is source-level shared-semantics evidence, not Linux validation. Linux
native compilation, GUI runtime, and HDF5/H5 runtime were not run and remain
unverified.

## 5. Fix

The platform-neutral <code>CameraController</code> change implements the
following facility behavior:

- live sources preserve their previous direct
  <code>get_device().get_facility()</code> lookup;
- RAW file sources still attempt real HAL Device/facility access when a Device
  exists;
- generic offline HDF5/H5/DAT sources return <code>nullptr</code> when
  <code>get_device()</code> is unavailable; and
- exceptions from <code>get_facility()</code> itself are not broadly swallowed.

OpenEB 5.2 does not publish a stable typed public comparison for its internal
<code>DeviceUnavailable</code> value. The narrow file-source fallback therefore
catches <code>CameraException</code> only around <code>get_device()</code>, not
around subsequent facility lookup.

<code>CameraController::connect_file()</code> now rolls a partial setup back
through <code>teardown()</code> before emitting <code>disconnected</code> and the
detailed error. This covers failures after callbacks, pipeline state, or a
synchronous <code>connected</code> slot have begun setup.

## 6. Automated regression evidence

The following are retained prior Codex/maintainer execution-report evidence;
this closure did not rerun build or tests:

- incremental build: Passed;
- <code>CameraControllerLifecycle.EmptyRawFailureIsCaught</code>: 1/1 Passed;
- full CTest: 309/309 Passed.

The lifecycle regression creates only a build-tree zero-byte RAW. It verifies
the controlled failed-open boundary, disconnected controller state, no file
source state, and <code>nullptr</code> from all facility accessors after failure.
It does not substitute for an automated generic HDF5 facility test.

## 7. CLI fixture validation

Fresh <code>.hdf5</code> and <code>.h5</code> working copies both passed
repository-local <code>metavision_file_info</code> prevalidation:

| Field | HDF5 | H5 |
| --- | --- | --- |
| Exit | <code>0</code> | <code>0</code> |
| Encoding | ECF | ECF |
| Duration | 95,871 us | 95,871 us |
| CD events | 521,252 | 521,252 |
| First timestamp | 0 | 0 |
| Last timestamp | 95,871 | 95,871 |

This covers one known ECF payload under <code>.hdf5</code> and <code>.h5</code>
extension facets. It does not establish a broader HDF5/H5 corpus result.

## 8. GUI runtime

| Field | Value |
| --- | --- |
| PID | <code>89063</code> |
| Start | <code>2026-07-26T07:31:34Z</code> |
| End | <code>2026-07-26T07:35:32Z</code> |
| Exit | <code>0</code> |

Manual GUI evidence recorded the following passed facets:

- HDF5 picker/open and non-empty changing event visualization;
- autoplay, one pause/resume sequence, and one seek;
- HDF5 to H5 switch with continued autoplay;
- HDF5 Recent reopen and autoplay;
- one natural HDF5 EOF while the GUI remained responsive; and
- normal GUI close.

## 9. Facility graceful degradation

The previous <code>102113 DeviceUnavailable</code> failure did not recur.
Biases, ROI, ESP, and Trigger initialization did not generate the prior
unhandled generic-offline Device error.

Their exact visual presentation — disabled, empty, unavailable, or hidden —
was not individually transcribed. This is not all-hardware-panel visual
verification, all-facility verification, or Milestone 6 live-facility
validation.

## 10. Runtime logs

The retained GUI stderr contains no <code>102113</code>,
<code>Device unavailable</code>, <code>SIGABRT</code>, <code>SIGSEGV</code>,
uncaught termination, dyld fatal, Qt fatal, or HDF5/ECF fatal marker.

One <code>IMKCFRunLoopWakeUpReliable</code> mach-port message was observed. It
is recorded as a non-fatal log message only; no broader interpretation is
made.

## 11. Passed

- generic-offline facility compatibility for the tested HDF5/H5 fixture;
- HDF5 open, display, and playback;
- H5 extension/open/switch behavior;
- one pause/resume sequence and one seek;
- HDF5 Recent reopen;
- natural EOF responsiveness; and
- clean process exit.

## 12. Not run

- DAT GUI and DAT conversion;
- other HDF5/H5 files, different geometry, large files, corrupt HDF5,
  permission failure, and missing HDF5 plugin cases;
- detailed visual state of hardware-oriented panels;
- algorithms, models, export, installed executable, and portable loader
  closure;
- Linux compile/runtime/HDF5/H5 validation; and
- performance and long-duration stability.

## 13. Cross-platform boundary

No Apple-specific conditional was introduced. No Linux-specific source file
was modified.

The fix is shared platform-neutral C++ because the audited generic-offline
semantics are shared. This is not Linux validation.

## 14. Milestone impact

This closes the reproduced generic-offline HDF5/H5
<code>DeviceUnavailable</code> compatibility defect for the tested macOS
build-tree fixture.

It does not close Milestone 5. DAT remains unverified.
