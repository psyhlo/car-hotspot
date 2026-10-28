# Store listing texts - Car Hotspot 0.4.3

Field limits: Summary 200 chars (used: 163), Description 4000 chars (used: 2828).

## Summary

```
Automatic car hotspot: watch your car's Bluetooth, get notified when you connect and control Wi-Fi tethering. Full background automation is in the OpenRepos build.
```

## Description

```
Car Hotspot watches your car's Bluetooth connection and takes care of the Wi-Fi hotspot for you: when the car connects, the hotspot is switched on; when you disconnect, it is switched off again after a short grace period. Everything runs locally on the phone - no accounts, no cloud, no root.

WHAT IT DOES
- Detects when your car (or several cars) connects and disconnects over Bluetooth
- Turns the Wi-Fi hotspot on for the passengers, and off again when you leave the car
- Waits a configurable grace period (default 2 minutes) before switching off, so a brief Bluetooth drop does not kill the hotspot
- Safety guards: refuses to start - or stops - the hotspot when the battery is below a chosen level and the phone is not charging; ensures mobile data is enabled before tethering; can power Bluetooth on automatically
- Vibration and system notifications when the car connects and when the hotspot starts or stops
- Manual switch, live status banner (car Bluetooth, hotspot, battery, roaming), a diagnostics page with an activity log, and a Bluetooth subsystem recovery action for stuck rfkill/BlueZ states

SETTINGS
Auto-toggle Hotspot, Run in background & autostart, Delayed turn-off, Minimum battery level, Auto-enable Bluetooth, Auto-enable Mobile Data, Vibration feedback, System notifications, Multiple car devices, Paired/detected device picker, Manual control, Diagnostics log.

TWO FLAVOURS - PLEASE READ BEFORE TESTING
Sailfish OS exposes hotspot switching only through com.jolla.Connectiond and through ConnMan's Tethering property, and both are restricted by the platform: a sandboxed third-party application receives PermissionDenied. That is platform policy, not a bug in this app.
- This Harbour package is the fully sandboxed flavour (Permissions=Bluetooth;Internet, no shell, no root). It monitors the car, shows live status, vibrates, notifies, and lets you select your car and configure every preference. The hotspot switch itself is unavailable here, and the app states that openly on screen and in the diagnostics log.
- The OpenRepos package has the same UI plus a systemd *user* service (running as your own user, no root, no QProcess/shell) that performs the hotspot switch, keeps monitoring in the background, survives reboot and works while the app is closed.

PRIVACY & SECURITY
- No network access from the app; it only talks to BlueZ and ConnMan over D-Bus
- No shell commands, no QProcess, no root, no files outside the app's own directories
- GPLv3 source: https://openrepos.net/content/psyhlo/car-hotspot

COMPATIBILITY
Sailfish OS 5.0 and 5.1 (armv7hl and aarch64); tested on a Jolla Phone with Sailfish OS 5.2. Any Bluetooth car kit or hands-free unit that reports a connection works; devices can be tagged as a car.

SUPPORT
OpenRepos page: https://openrepos.net/content/psyhlo/car-hotspot
```

## Message to QA

```
The Harbour package is the sandboxed flavour (built without the background service): the UI asks for Bluetooth;Internet only, and there is no QProcess/shell or root anywhere in it. It monitors Bluetooth, shows live status, vibrates and posts notifications; the manual hotspot switch logs a warning instead of failing silently, because switching requires com.jolla.Connectiond or a ConnMan Tethering property write, which Sailfish grants only to system components (verified on device: PermissionDenied for an ordinary user). The OpenRepos package of the same app adds an unconfined systemd user service (own user, still no shell and no root) that does the switching and runs in the background - that is the flavour with full automation, as the on-screen notice explains.

No account and no credentials are needed. To test: enable Bluetooth and pair a car kit, open the app, select the device under "Paired / Detected Devices", press Save, then try the toggles. Pull down on the main page for "Manual Control & Diagnostics" (activity log, manual switch, Bluetooth recovery). If no car is nearby, any paired Bluetooth device behaves the same, and a device without Wi-Fi (e.g. the emulator) reports a graceful "no Wi-Fi adapter" error rather than crashing.
```

## Screenshots (1080x2378 PNG, Harbour wants >= 1080 px width)

| File | Shows |
|---|---|
| screenshots/harbour/1.png | Main page: status banner, Automation & Safety toggles |
| screenshots/harbour/2.png | Main page scrolled down: battery guard, car picker, manual switch |
| screenshots/harbour/3.png | Manual Control & Diagnostics page |

Device panel is 1032x2272; the PNGs were upscaled to 1080 px width (lanczos) because Harbour requires at least 1080 px. Originals are kept in screenshots/raw/.
