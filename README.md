# 🚗 Car Hotspot (`harbour-carhotspot`) for Sailfish OS

Turns the Wi-Fi hotspot on when your car's Bluetooth connects and off again when you leave — with
grace period, battery guard and roaming guard. No accounts, no cloud, no root.
OpenRepos: <https://openrepos.net/content/psyhlo/car-hotspot> · GPLv3.

## ✨ Features

- **Auto-toggle**: hotspot on when the car connects, off after a configurable grace period (0–10 min)
  so a short Bluetooth drop does not kill it.
- **Car selection**: paired/detected device list, manual MAC entry, optional multi-car mode.
- **Wi-Fi state restoration**: remembers whether Wi-Fi was on before tethering and restores it.
- **Guards**: minimum battery level (5–50 %, not charging), roaming guard for unexpected carrier
  charges, mobile data powered + connected before tethering (dual-SIM aware, oFono/ConnMan).
- **Feedback**: banner/lockscreen notifications with one-tap activation, vibration and sound.
- **UI**: live status banner (car BT, hotspot, battery, roaming), cover actions, scrollable
  activity log, diagnostics page with a Bluetooth subsystem recovery action.
- **Background service** (OpenRepos flavour): `systemd --user` unit running as your own user — no
  root, no shell, no SUID helper — starts on boot and keeps working with the app closed.
- **Localization**: all 34 Sailfish OS languages, finished.

## 🍟 Two flavours

One source tree, one switch: `%with_service` in `rpm/harbour-carhotspot.spec`
(`CONFIG+=service` in the .pro).

| Flavour | Built by | Background service | Hotspot switching | Distribution |
|---|---|---|---|---|
| **Harbour** (default) | `build-all` | none | no (platform policy, see below) | Jolla Harbour — 2 files |
| **OpenRepos** | `build-openrepos` | `systemd --user` unit, enabled by `%post` | yes, also with the app closed | OpenRepos — 4 files |

**Why only the OpenRepos flavour can switch the hotspot** (measured on a Jolla Phone 2026,
Sailfish OS 5.2.0.20): enabling tethering needs `com.jolla.Connectiond`, which Sailjail grants only
through the `Messages` permission (system messaging apps), and ConnMan's
`Technology.SetProperty("Tethering")` answers `PermissionDenied` for an ordinary user. A sandboxed
process can therefore read the hotspot state but not change it — the sandboxed service variant was
built and tested, and the enable request failed. The shipped service is simply not wrapped in
firejail; it still runs nothing privileged and nothing through a shell, and the UI process stays
sandboxed. That is exactly why this application belongs on OpenRepos and not on Jolla Harbour.
The UI says so: in the Harbour flavour both pages show a notice with an **Open OpenRepos page**
button, and switching on *Run in background & Autostart* explains it instead of doing nothing.

**Harbour upload files** (exactly these two land in `RPMS/`, both with the current store target):

| File | Target | For |
|---|---|---|
| `harbour-carhotspot-0.4.6.1-1.armv7hl.rpm` | `SailfishOS-5.1.0.11-armv7hl` | SFOS 5.1+, 32-bit ARM (Xperia X, XA2, F2C, …) |
| `harbour-carhotspot-0.4.6.1-1.aarch64.rpm` | `SailfishOS-5.1.0.11-aarch64` | SFOS 5.1+, 64-bit ARM (Jolla Phone 2026) |

**OpenRepos upload files** (four, one per OS release and architecture, in `RPMS-openrepos/`):

| File | Target | For |
|---|---|---|
| `harbour-carhotspot-0.4.6.1-1or.armv7hl.rpm` | `SailfishOS-5.1.0.11-armv7hl` | SFOS 5.1, 32-bit |
| `harbour-carhotspot-0.4.6.1-1or.aarch64.rpm` | `SailfishOS-5.1.0.11-aarch64` | SFOS 5.1, 64-bit |
| `harbour-carhotspot-0.4.6.1-1sfos50or.armv7hl.rpm` | `SailfishOS-5.0.0.62-armv7hl` | SFOS 5.0, 32-bit |
| `harbour-carhotspot-0.4.6.1-1sfos50or.aarch64.rpm` | `SailfishOS-5.0.0.62-aarch64` | SFOS 5.0, 64-bit |

Both flavours use the same package **name and version**, and rpm decides upgrade vs. downgrade from
`name-version-release` alone. The `or` suffix therefore makes every OpenRepos package *newer* than
the Harbour package, so moving from the store build to the full build is a plain upgrade and never a
silent "already installed" no-op. Going the other way (full → store) is a downgrade and needs
`rpm --oldpackage`. The `sfos50` part keeps the older-target builds from sharing an NVR with the 5.1
build — otherwise the two different binaries would be indistinguishable to rpm. Each package is
linked against its own target sysroot, so the 5.0 files cannot reference newer symbols. `i486` is the
SDK emulator build — it lives in `RPMS-emulator/` and is never uploaded. Harbour accepts only
`name-version-release.arch.rpm`, so `RPMS/` holds no aliases (the manual-install alias is in
`RPMS-sfos5.0/`). Listing texts are in `docs/harbour-submission.md`, screenshots in
`screenshots/harbour/1.png … 3.png` (1080×2378; device originals in `screenshots/raw/`).

## 📥 Installation & usage

Install from [OpenRepos](https://openrepos.net/content/psyhlo/car-hotspot) or Storeman, or manually:

```bash
devel-su
rpm -Uvh harbour-carhotspot-0.4.6.1-1.aarch64.rpm
```

Then: open Car Hotspot → pick your car under *Paired / Detected Devices* → **Save** → enable
*Auto-toggle Hotspot* and set the grace period. Diagnostics page: pull down on the main page.

Upgrading from a store (Harbour) build to the OpenRepos build is a normal `rpm -Uvh` / Storeman
upgrade — settings are shared, so nothing has to be configured again. Replacing the OpenRepos build
with the store build is a downgrade: `rpm -Uvh --oldpackage`, and the background service stops for
good because the store package ships no unit and no install scripts.

## 📶 Bluetooth radio behaviour

Car Hotspot does not poll Bluetooth. The car kit that is switched on connects to the phone by itself,
BlueZ reports `org.bluez.Device1.Connected` over D-Bus, and that signal is what wakes the app and turns
the hotspot on. Phone-initiated paging exists only for kits that never initiate, and it is bounded: a
burst is two attempts (15 s then 30 s apart), and after a burst that finds no car the quiet window grows
- 30 min, then 1 h, then 2 h. Picking a car, a Bluetooth power change or a real connect/disconnect
brings it back to 30 min.

Nothing is paged while the hotspot is up or coming up (a page, LE scanning and the access point all want
the same combo controller), nothing is paged when *Auto Connect to Car* is off, and LE-only devices are
never paged or connected at all - a BLE peripheral accepts one central at a time, so taking one from
`com.android.bluetooth` is how a watch stops being discoverable from the Android app. The Diagnostics
page says what the app did to the radio: `Paging CAR MA7 (attempt 1/2 this cycle, radio duty ~10s)`,
`… is not reported by BlueZ - nothing to page, monitoring passively`, and when the last attempt was used.

Advanced: `pageOnWake=true` in `carhotspot.ini` re-arms paging on a screen wake, still at most one burst
per fifteen minutes. See [docs/release-v0.4.6.1.md](docs/release-v0.4.6.1.md).

## 🛠️ Building from source

Needs the [Sailfish SDK](https://sailfishos.org/develop/) (sfdk 3.x with the SailfishOS-5.1.0.11
targets; the 5.0 targets are optional but recommended for the older packages).

```bash
./build-hotspot.sh build-all          # Harbour flavour -> 2 upload files in RPMS/
./build-hotspot.sh build-openrepos    # OpenRepos flavour -> 4 upload files in RPMS-openrepos/
./build-hotspot.sh build-emulator     # i486 emulator build -> RPMS-emulator/
./build-hotspot.sh check              # Harbour rules / rpmlint on RPMS/ — exits non-zero on failure
./build-hotspot.sh deploy-openrepos   # install on the phone and enable the service
./build-hotspot.sh deploy-emulator    # install and start in the emulator
./build-hotspot.sh shots              # store screenshots -> screenshots/harbour/
./build-hotspot.sh device-logs        # app/service log on the phone
./build-hotspot.sh clean
```

| Directory | Target | Runs on |
|---|---|---|
| `RPMS/` | 5.1 targets | the two Harbour upload files |
| `RPMS-store/` | 5.1 targets | private mirror of those two files, restored into `RPMS/` after an OpenRepos run |
| `RPMS-openrepos/` | 5.0 + 5.1 targets | the four OpenRepos upload files |
| `RPMS-sfos5.0/` | `SailfishOS-5.0.0.62-armv7hl` | SFOS 4.x / 5.0 (`GLIBC_2.4`) + install alias |
| `RPMS-sfos5.1/` | `SailfishOS-5.1.0.11-armv7hl` | archive copy of the 32-bit store build |
| `RPMS-emulator/` | `SailfishOS-5.1.0.11-i486` | SDK emulator only — never upload |

Always `rm -f Makefile *.o moc_* harbour-carhotspot` when switching targets (the script does it):
leftover 32-bit objects otherwise get linked into an aarch64 build and the RPM silently ships the
previous binary. Privileged install steps go through `devel-su`, which is an interactive root shell
on Sailfish; `pkcon install-local` needs an authentication agent and hangs over SSH, so the script
pipes the Developer-mode password plus `rpm -Uvh --replacepkgs` into that shell (export
`DEVICE_ROOT_PASSWORD` or type it when asked). `shots` uses a hidden screenshot mode driven by the
settings keys `devShotEnabled` / `devShotTrigger` / `devShotDir` and waits for you to open the app
from the launcher — the compositor only renders an app that is actually on screen, so an app started
over SSH yields blank grabs.

## 🔒 Sandbox model

Every device operation is a plain Qt D-Bus call: no `QProcess`, no shell, no SUID helper, nothing
executed outside the sandbox. Checked against `sailfishos/sailjail-permissions`:

| Operation | Granted by |
|---|---|
| BlueZ adapter/device, `Device1.Connect`, `com.jolla.Bluetooth` power | `Bluetooth` |
| ConnMan queries (`GetServices`, `GetTechnologies`, `GetProperties`, signals) | `Internet` |
| ConnMan `SetProperty("Tethering")` | **denied for users** (`PermissionDenied`) |
| oFono modem / registration (roaming guard) | `Internet` |
| `com.nokia.mce` battery, charger, display signals | Base |
| `com.nokia.time` `add_event` / `cancel` / `cookie` — RTC wakeups, **never** `Run_Command` | Base |
| Vibration (`com.nokia.NonGraphicFeedback1.Backend`), notifications | Base |
| `org.freedesktop.systemd1` `StartUnit`/`EnableUnit` (OpenRepos flavour only) | `AppLaunch` ("User services") |

Gone by design: `com.nokia.time` `Run_Command`, which used to run `mkdir/cp/ln/systemctl/dbus-send`
outside the sandbox (privilege-escalation surface, certain Harbour rejection), and the exported
automation slots — the D-Bus surface now only exposes `openApp`, `activate` and `backgroundStatus`,
so another sandboxed app cannot toggle the hotspot or restart Bluetooth. UI and service exchange
**signals only**, because Base broadcasts `org.sailfishos.coveraction.*` signals but filters method
calls between apps; the settings file is the authoritative channel and the signal is the fast path.

Settings live in `~/.config/harbour-carhotspot/harbour-carhotspot/carhotspot.ini` — the only config
directory a sandbox can see and keep. The OpenRepos package migrates the pre-0.4.3 `.conf` file in
`%post`; the store package ships **no scriptlets at all** (Jolla rejects them), so a store user coming
from 0.4.2 or older has to pick the car once again.

## ✅ Verifying on a device

1. Install, pick the car, toggle the hotspot from the cover action, read the diagnostics log.
2. OpenRepos flavour: `systemctl --user status harbour-carhotspot.service`, then confirm `net.connman`
   reports `Tethering=true`, a client gets an address, and Wi-Fi returns to its previous power state.
3. Screen off / deep sleep: lock the phone, walk to the car, connect — the hotspot must come up and
   the grace-period shutdown must fire (timed `com.nokia.time` cookie, no `Run_Command`).
4. Tapping a notification must bring the app to the foreground (`desktop-entry` hint; lipstick then
   activates the registered bus name). The OpenRepos flavour additionally declares
   `ExecDBus=/usr/bin/sailjail -p harbour-carhotspot.desktop -- …`; Jolla rejects a custom `ExecDBus`
   entry, so the store flavour has none and relies on the desktop entry alone.
5. Tethering without touching the screen: `/usr/bin/harbour-carhotspot --selftest`.

## 🧰 If every `sfdk build` hangs

`sfdk config` still working but builds printing nothing usually means `sfdk-bus` inside the Build
Engine VM is stuck at 100 % CPU:

```bash
ssh -i E:/SailfishOS/vmshare/ssh/private_keys/sdk -p 2222 root@127.0.0.1 \
    "systemctl restart sfdk-bus.service sfdk-bus-expose-nonce.service"
```

## 📁 Repository layout

```
src/          AppController, BluetoothManager, HotspotManager, SystemMonitor,
              CarHotspotDBusAdapter (signals-only IPC), TimedDbus, carhotspotsettings.h
qml/          Silica UI: MainPage, ToolsPage, CoverPage
service/      systemd user unit + its Sailjail profile (OpenRepos flavour)
rpm/          spec (flavour macro, changelog, packaging, %post migration)
translations/ 34 × .ts, all finished
tools/        l10n helpers
docs/         harbour-submission.md (Summary, Description, QA message)
screenshots/  harbour/ upload PNGs, raw/ device originals
build-hotspot.sh   build / deploy / check / shots helper
```

## 📜 License

GPLv3.
