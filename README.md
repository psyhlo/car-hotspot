# 🚗 Car Hotspot (`harbour-carhotspot`) for Sailfish OS

[![OpenRepos](https://img.shields.io/badge/OpenRepos-car--hotspot-blue.svg)](https://openrepos.net/content/psyhlo/car-hotspot)

**Car Hotspot** is an intelligent, automated Wi-Fi hotspot management utility for Sailfish OS. It seamlessly turns your smartphone into an in-car Wi-Fi router by activating mobile tethering the moment your phone connects to your car's Bluetooth hands-free / infotainment system, and safely turning it off when you leave.

---

### ✨ Features

- 🔄 **Smart Automation**: Automatically activates Wi-Fi hotspot upon pairing with your designated car Bluetooth device.
- ⏳ **Grace Period (Delayed Shutdown)**: Configurable disconnect grace timer (0 to 10 minutes). Avoids turning off the hotspot during brief disconnections (e.g., fuel stops or momentary signal drops).
- 📶 **Cellular Keep-Alive**: Automatically ensures mobile data is powered on and connected via ConnMan technology before starting tethering.
- 🔋 **Battery Drain Protection**: User-defined minimum battery threshold (5%–50%). Will not start or will automatically power down if the battery is low and the device is not on a charger.
- 🌍 **Roaming Guard**: Protects against unexpected carrier charges by preventing auto-activation when roaming abroad.
- 📳 **Haptic Feedback & Sound**: Haptic vibration confirmation (via NGF session bus) and sound cues when connected to the car and hotspot starts successfully.
- 🚀 **Background Daemon & Autostart**: Optional `systemd --user` background service (`harbour-carhotspot.service`) that monitors Bluetooth in the background and starts on phone boot.
- 🌐 **System-Aware Localization**: Follows system language settings (English and Bulgarian supported out of the box).
- 📱 **Active Silica UI & Cover**: Real-time status banner, paired devices discovery, manual override, and cover actions.

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

