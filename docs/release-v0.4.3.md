# Car Hotspot 0.4.3

Hotspot on when the car connects, off when you leave — now without touching anything outside the
sandbox, and with a background service that actually survives the app being closed.

## Highlights

**No privilege escalation anymore.** `com.nokia.time` `Run_Command` is gone: it used to execute
`mkdir/cp/ln/systemctl/dbus-send` outside the Sailjail sandbox. Timed wakeups now use
`add_event` + `Send_Cookie` and the `cookie` signal, so nothing is ever executed by the app.
Grace-period shutdown, deep-sleep timing and the reboot autostart all still work.

**Two flavours from one source tree** (`%with_service` in the spec):

| Flavour | Background service | Switches the hotspot | Where |
|---|---|---|---|
| Harbour (`with_service 0`) | none | no | Jolla Harbour, 2 files |
| OpenRepos (`with_service 1`) | `systemd --user` unit, enabled by `%post` | yes, also with the app closed | OpenRepos, 4 files |

Measured on a Jolla Phone 2026 (Sailfish OS 5.2.0.20): enabling a hotspot needs
`com.jolla.Connectiond`, which Sailjail exposes only through the `Messages` permission (system
messaging apps), and ConnMan answers `net.connman.Error.PermissionDenied` for
`Technology.SetProperty("Tethering")` from an ordinary user. A sandboxed process can read the
hotspot state but not change it — the sandboxed service variant was built and tested on the device,
and the enable request failed. So the OpenRepos unit runs the application binary directly as your
own user (no root, no shell, no SUID helper, no `QProcess`), while the UI stays sandboxed with
`Bluetooth;Internet;AppLaunch`. The Harbour flavour keeps Bluetooth monitoring in the app process
and both pages explain the difference instead of failing silently.

**UI ↔ service IPC is signals only.** Sailjail broadcasts `org.sailfishos.coveraction.*` signals
but filters method calls between apps, so signals are the sanctioned cross-process channel and the
settings file is the authoritative state. No blocking calls on the UI thread. The D-Bus surface now
exports only `openApp`, `activate` and `backgroundStatus` — another sandboxed app cannot toggle the
hotspot, restart Bluetooth or write to the diagnostics log.

**Settings moved somewhere the sandbox can keep.** Configuration and the shared activity log live in
`~/.config/harbour-carhotspot/harbour-carhotspot/carhotspot.ini` (`QStandardPaths::AppConfigLocation`),
the only config directory a sandbox sees and keeps; `%post` migrates the old
`~/.config/harbour-carhotspot/harbour-carhotspot.conf`, so devices upgrading from 0.4.x keep their car
and preferences.

**Reliability & UI**
- Manual "Hotspot OFF" is remembered across restarts; manual requests travel through the settings
  file (authoritative) plus a signal (fast path), because the broadcast filter can drop signals.
- Shared log merged by entry timestamp instead of last-writer-wins; BlueZ address fallbacks no longer
  overwrite a user's device label.
- Notification tap activates the app through the `desktop-entry` hint + `ExecDBus` (this Sailfish
  build has no remote-action support), `ExecDBus` moved into the `X-Sailjail` group so
  `desktop-file-validate` passes.
- Hidden `--selftest` runs the real tethering path and prints the result — useful for verifying a
  device over SSH without touching the screen.
- Translations: all 34 languages finished, 0 unfinished/missing entries.

## Packages

GitHub assets (name-version-release-flavour.arch.rpm so the two flavours cannot collide in one release):

| Asset | Flavour | Target | Runs on |
|---|---|---|---|
| `harbour-carhotspot-0.4.3-1-harbour.armv7hl.rpm` | Harbour | 5.1 armv7hl | SFOS 5.1+, 32-bit ARM |
| `harbour-carhotspot-0.4.3-1-harbour.aarch64.rpm` | Harbour | 5.1 aarch64 | SFOS 5.1+, 64-bit ARM |
| `harbour-carhotspot-0.4.3-1-openrepos.armv7hl.rpm` | OpenRepos | 5.1 armv7hl | SFOS 5.1, 32-bit |
| `harbour-carhotspot-0.4.3-1-openrepos.aarch64.rpm` | OpenRepos | 5.1 aarch64 | SFOS 5.1, 64-bit |
| `harbour-carhotspot-0.4.3-1sfos50-openrepos.armv7hl.rpm` | OpenRepos | 5.0 armv7hl | SFOS 5.0, 32-bit |
| `harbour-carhotspot-0.4.3-1sfos50-openrepos.aarch64.rpm` | OpenRepos | 5.0 aarch64 | SFOS 5.0, 64-bit |

The store uploads keep the plain `name-version-release.arch.rpm` names Harbour requires
(`harbour-carhotspot-0.4.3-1.armv7hl.rpm`, `.aarch64.rpm`, and for OpenRepos also
`harbour-carhotspot-0.4.3-1sfos50.*`); the content is identical, only the file name differs. The
`1sfos50` release tag exists only to keep the four file names distinct, and each package is built
against its own target sysroot. An `i486` build exists for the SDK emulator only and is never uploaded.

## Upgrading

Installing over a 0.4.2 package migrates settings automatically and — in the OpenRepos package —
enables the user unit in `%post`. After an upgrade of the OpenRepos flavour, restart the service or
just reboot: `systemctl --user restart harbour-carhotspot.service`. The in-app switch
*Run in background & Autostart* starts/stops it through `org.freedesktop.systemd1`
(permission "User services"), never through a shell.

## Verified

- Jolla Phone 2026, Sailfish OS 5.2.0.20: connect/disconnect cycles, grace-period shutdown via the
  timed `cookie`, autostart after reboot, settings migration from 0.1.x/0.4.x.
- SDK emulator (SFOS 5.1.0.11 i486): service enabled and running, D-Bus names registered, graceful
  failure when no Wi-Fi adapter exists.

GPLv3. Source: https://github.com/psyhlo/car-hotspot · OpenRepos:
https://openrepos.net/content/psyhlo/car-hotspot
