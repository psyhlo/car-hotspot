# 🚗 Car Hotspot (`harbour-carhotspot`) for Sailfish OS

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

1. Copy the RPM package to your device and install via terminal or File Manager:
   ```bash
   devel-su pkcon update harbour-carhotspot-0.1.25-1.aarch64.rpm
   ```
2. Open **Car Hotspot** from the app launcher.
3. Select your car's Bluetooth device from the list (or manually enter its MAC address).
4. Enable **Auto-toggle Hotspot** and configure your preferred grace period timer.
