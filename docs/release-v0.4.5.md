# Car Hotspot 0.4.5

Packaging that Jolla accepts without argument, and a Bluetooth radio that stops hogging the
controller. Both flavours are rebuilt from the same source tree; the full version stays the
default upgrade direction.

## Harbour packaging (`sfdk check` → 0 errors, 0 warnings on both arches)

| Validator complaint | Fix |
|---|---|
| `Missing valid Exec declaration, must be Exec=harbour-carhotspot` | `Exec=harbour-carhotspot` in all three desktop entries |
| `ExecDBus has invalid argument(s)` | custom `ExecDBus` key removed from the store flavour (OpenRepos keeps it, pointing at the profile that is actually installed) |
| `/usr/share/licenses/…` and `/usr/share/doc/…` *Installation not allowed* | the GPLv3 text is not installed at all; the `License:` tag is what the store shows |
| `RPM 'postinstall'/'preuninstall' script not allowed`, 2× `/bin/sh` Dependency, `use-of-home-in-%post` | `%post` and `%preun` wrapped in `%if %{with_service}` — the store package ships no scriptlets and no `/bin/sh` requires |
| debuginfo/debugsource subpackages in the upload directory | `%define _debuginfo_packages 0` and `%define _debugsource_packages 0` (`_enable_debug_packages 0` alone is not enough) |

Consequence of the scriptlet gating: a store user upgrading from 0.4.2 or older does not get the
automatic settings migration and has to pick the car once. The OpenRepos flavour still migrates.

## Release numbering, so flavour switching actually works

Both flavours share package name and version, and rpm decides upgrade vs. downgrade from
`name-version-release` alone. OpenRepos builds now carry **`1or`** (and `1sfos50or` for the
Sailfish OS 5.0 target), which sorts above the store's `1`:

- store → full: plain `rpm -Uvh` / Storeman upgrade, settings kept
- full → store: a downgrade, needs `rpm --oldpackage`

With identical Releases the full package reported "already installed" and a store→full swap did
nothing at all. The separate `1sfos50` tag on the manual-install build also removes the old NVR
collision between the 5.0 and 5.1 armv7hl builds, which contained different code.

## Sharing the radio with the Android support stack

Measured on a Jolla Phone 2026 (Sailfish OS 5.2.0.20): Mer's BlueZ owns `/org/bluez/hci1` while
`com.android.bluetooth`, `appsupport-bridge-bt-aidl` and
`android.hardware.bluetooth-service-mediatek` drive their own HCI on the **same combo controller**.
Anything that occupies one interface takes air time from the other, which is how Bluetooth
discovery from an Android app becomes flaky.

- **Paged the car at most 3 times per cycle** (15 s → 30 s → 60 s back-off) and then monitors
  passively. Before this, a car that was out of range was paged every minute around the clock,
  each attempt holding the controller for ~10 s. A cycle ends on a Bluetooth power change, a
  display wake, a real connect/disconnect or an action in the app — **and after 10 minutes of
  quiet**. That timer is deliberate: someone who never switches Bluetooth off would otherwise
  exhaust the budget once and never reach the car again. Worst case is now ~30 s of paging per
  10 minutes instead of ~10 s every 60 s, and the diagnostics page states when the budget is spent.
- **Never pages while the access point is active or starting**, so LE scanning, a BR/EDR page and
  the AP no longer compete for the same firmware slots.
- **LE-only devices are never paged or connected** (`AllowedModes = le`, or icon
  `watch`/`le-audio`/`gles`): a BLE peripheral serves one central at a time, so connecting to a
  watch from this process takes it away from the Android stack that owns it. Devices reported as
  `unknown` stay connectable for compatibility.
- **`GetManagedObjects` is asynchronous** with a 3 s cap and one request per signal burst. It used
  to be a blocking call with Qt's 25 s default timeout, run on every BlueZ event and every poll
  tick, and it froze the calling thread when bluetoothd was busy. Adapter and ConnMan power reads
  are capped at 2 s.
- Turning **Auto-toggle Hotspot** off now means the app emits no radio-driving traffic at all:
  paging is gated by that switch.

## Also

`harbour-carhotspot.pro` had a missing line continuation in `HEADERS`, so `src/carhotspotsettings.h`
was not part of the header list. `build-hotspot.sh` no longer swallows a failing `sfdk check`
(it exits non-zero), keeps a private mirror of the store packages in `RPMS-store/` because sfdk
deletes RPMS/ entries whose Release differs, and drops stale artifacts from the output directories.

## Diagnostics says what the radio is doing

Support questions of the form *"is this app interfering with my other Bluetooth device?"* could
previously only be answered from the journal, which needs root. The Diagnostics page now answers it
directly, in the app's own activity log:

```
[15:56:08] Paging CAR MA7 (attempt 1/3 this cycle, radio duty ~10s)
[15:48:46] Car paging stopped after 3 attempts - monitoring passively, re-armed by Bluetooth
           power, display wake, an action in the app or after 10 min
[15:48:46] CAR MA7 is not reported by BlueZ - nothing to page, monitoring passively
```

The last line is throttled to one per two minutes and means the car is simply out of range - a
classic device that is idle does not advertise, so there is nothing to page and the app is purely
passive. The startup message no longer says "Bluetooth switched on" for what is only the initial
state read.
