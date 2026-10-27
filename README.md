# 🚗 Car Hotspot (`harbour-carhotspot`) for Sailfish OS

[![OpenRepos](https://img.shields.io/badge/OpenRepos-car--hotspot-blue.svg)](https://openrepos.net/content/psyhlo/car-hotspot)

**Car Hotspot** is an intelligent, automated Wi-Fi hotspot management utility for Sailfish OS. It seamlessly turns your smartphone into an in-car Wi-Fi router by activating mobile tethering the moment your phone connects to your car's Bluetooth hands-free / infotainment system, and safely turning it off when you leave.

---

### ✨ Features

- 🔄 **Smart Automation**: Automatically activates Wi-Fi hotspot upon pairing with your designated car Bluetooth device, and safely turns it off when you disconnect.
- 🚘 **Multi-Car Device Support**: Optional multi-device mode allowing you to select multiple car Bluetooth devices (single selection by default; switching back retains your primary vehicle).
- 📶 **Smart Wi-Fi State Restoration**: Remembers your previous Wi-Fi power state before the hotspot started and seamlessly restores it when turning off the hotspot.
- ⏳ **Grace Period (Delayed Shutdown)**: Configurable disconnect grace timer (0 to 10 minutes). Avoids turning off the hotspot during brief disconnections (e.g., fuel stops or momentary signal drops).
- 📡 **Cellular Keep-Alive & Dual-SIM Support**: Automatically ensures mobile data is powered on and connected across Single and Dual-SIM devices (with privileged helper fallback) before starting tethering.
- 🔋 **Battery Drain Protection**: User-defined minimum battery threshold (5%–50%). Will not start or will automatically power down if the battery is low and the device is not on a charger.
- 🌍 **Roaming Guard**: Multi-SIM aware roaming protection against unexpected carrier charges by preventing auto-activation when roaming abroad.
- 🔔 **Interactive System Notifications**: Lockscreen and banner notifications for status changes, with one-tap action to bring the app directly to the foreground.
- 📳 **Haptic Feedback & Sound**: Haptic vibration confirmation (via NGF session bus) and sound cues when connected to the car and hotspot starts successfully.
- 🚀 **Background Daemon & Autostart**: Optional `systemd --user` background service (`harbour-carhotspot.service`) that monitors Bluetooth in the background and starts on phone boot.
- 📋 **Live Activity Log**: Scrollable in-app event log tracking real-time connections, battery, cellular, and diagnostic events.
- 🌐 **Comprehensive Localization**: Translated into all 34 Sailfish OS official languages out of the box.
- 📱 **Active Silica UI & Cover**: Real-time status banner, paired devices discovery, manual MAC entry override, and cover actions.

---

### 📥 Installation & Usage

- **OpenRepos / Storeman**:
  Install or update directly via [OpenRepos](https://openrepos.net/content/psyhlo/car-hotspot) or the Storeman client.

- **Manual RPM Installation**:
  1. Download or copy the RPM package for your architecture (`aarch64` or `armv7hl`) from [OpenRepos](https://openrepos.net/content/psyhlo/car-hotspot) to your device.
  2. Install via terminal or File Manager:
     ```bash
     devel-su pkcon update harbour-carhotspot-*.rpm
     ```
  3. Open **Car Hotspot** from the app launcher.
  4. Select your car's Bluetooth device from the list (or manually enter its MAC address).
  5. Enable **Auto-toggle Hotspot** and configure your preferred grace period timer.

---

### 🛠️ Building from source

Requires the [Sailfish SDK](https://sailfishos.org/develop/) (sfdk 3.x, SailfishOS-5.1.0.11 targets).

```bash
./build-hotspot.sh build-all      # uploadable packages -> RPMS/ (+ variants in RPMS-sfos5.*/)
./build-hotspot.sh check          # Jolla Harbour / rpmlint validation of the packages in RPMS/
./build-hotspot.sh run-device     # build + install + start on a Jolla Phone (USB)
./build-hotspot.sh run-emulator   # build + install + start in the local emulator
```

**Package matrix** (`build-all` produces all of these):

| Package | Target used | Runs on |
|---|---|---|
| `RPMS/*.aarch64.rpm` | `SailfishOS-5.1.0.11-aarch64` | SFOS 5.1+ (64-bit ARM, Jolla Phone 2026) |
| `RPMS/*.armv7hl.rpm` | `SailfishOS-5.0.0.62-armv7hl` | SFOS 4.x, 5.0, 5.1, 5.2 (needs only `GLIBC_2.4`) |
| `RPMS-sfos5.1/*.armv7hl.rpm` | `SailfishOS-5.1.0.11-armv7hl` | SFOS 5.1+ only (`GLIBC_2.34`) |
| `RPMS-sfos5.0/*.rpm` | `SailfishOS-5.0.0.62-armv7hl` | SFOS 4.x / 5.0 (`GLIBC_2.4`) |

A binary linked against an old glibc also runs on newer releases, so the **armv7hl package is
built with the 5.0 target** and a single upload covers every 32-bit ARM device. Install that
target once with `sfdk tools install SailfishOS-5.0.0.62-armv7hl`.

`RPMS-sfos5.0/` additionally contains a `…-0.4.2-1.sfos5.0.armv7hl.rpm` copy for manual installs
on old devices (`devel-su pkcon install-local …`). That file name is deliberately **not**
Harbour-valid (the validator requires exactly `name-version-release.arch.rpm`), so never upload
it — upload the plain-named copy.

Notes:

- **Version**: `rpm/harbour-carhotspot.spec` is the single source of truth. `build-hotspot.sh`
  passes `-c no-fix-version`, otherwise sfdk derives a version from git tags and appends a
  `+branch.timestamp.hash` suffix that Harbour/Storeman cannot sort properly.
- **Clean between targets**: object files are always removed before switching target
  architecture; leftover `.o` files from another arch end up inside the RPM and rpmbuild
  cannot even strip them (`file ... ARM aarch64` inside an `armv7hl` package).
- **Windows**: run `sfdk` from `cmd.exe`/PowerShell, not from Git-Bash. Git-Bash breaks sfdk's
  session handling (`Session management is not possible`) and D-Bus calls then hang.

### 🧰 Troubleshooting the Build Engine VM

If every `sfdk build` / `sfdk check` hangs with no output (and `sfdk config` still works), the
`sfdk-bus` daemon inside the *Sailfish SDK Build Engine* VM is usually stuck in a 100 % CPU loop
and never answers D-Bus calls. Fix it without reinstalling the SDK:

```bash
# Windows: the build engine forwards SSH on 127.0.0.1:2222
ssh -i E:/SailfishOS/vmshare/ssh/private_keys/sdk -p 2222 root@127.0.0.1 \
    "systemctl restart sfdk-bus.service sfdk-bus-expose-nonce.service"
```

Verify with `top -bn1` in the VM (`dbus-daemon` should drop to ~0 % CPU) and by running
`sfdk tools list` again.

### 📁 Repository layout

```
src/          C++ backend: AppController (automation + daemon mode), BluetoothManager,
              HotspotManager, SystemMonitor, TimedDbus (com.nokia.time wakeups)
qml/          Silica UI: MainPage, ToolsPage, CoverPage
rpm/          RPM spec (version, changelog, packaging)
translations/ 34 × .ts files (all finished)
tools/        l10n helpers: add_missing_translations.py, check_missing_strings.py,
              generate_all_translations.py
build-hotspot.sh   build / deploy / check helper
```

All privileged operations go through D-Bus (`net.connman`, `com.jolla.Connectiond`,
`org.ofono`, `com.nokia.mce`, `com.nokia.time`, BlueZ) — no `QProcess`, no SUID helper,
no `sudoers`, so the package stays Sailjail/Harbour compliant.

### 📜 License

GPLv3 — see [LICENSE](LICENSE).

