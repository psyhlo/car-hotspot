Name:       harbour-carhotspot
Summary:    Car Hotspot for Sailfish OS
Version:    0.4.6.1
Release:    1
License:    GPLv3
URL:        https://github.com/psyhlo/car-hotspot

# Two flavours from one source tree. They are VARIANTS of one application, not two
# co-installable packages: same Name, same files, same application id, therefore the
# same settings directory. Installing one over the other replaces it, and the user's
# car selection and preferences survive the swap.
#
#   %with_service 0 (default) - Jolla Harbour build. No background service and no RPM
#                               scriptlets at all (Jolla's validator rejects post and
#                               preun scripts, and they would add a "/bin/sh" dependency).
#                               The desktop file declares Bluetooth;Internet only.
#   %with_service 1           - OpenRepos build. Ships a systemd *user* unit, the scriptlet
#                               that enables it and the settings migration, and installs the
#                               desktop flavour that declares the "User services"
#                               permission (AppLaunch) used to start/stop that unit
#                               through org.freedesktop.systemd1.
# Set it from build-hotspot.sh (`./build-hotspot.sh build-all` writes the OpenRepos
# flavour into RPMS-openrepos/).
%define with_service 0

# rpm decides whether an install is an upgrade by comparing name-version-release, and
# both flavours share name and version. Identical Release means "already installed" and
# nothing happens, so the full flavour must carry a Release that sorts ABOVE the store
# one: Harbour ships Release 1 (below), OpenRepos ships 1or (and 1sfos50or for the
# Sailfish OS 5.0 target), set by build-hotspot.sh. Consequence to be aware of: a user
# with both the Jolla Store and OpenRepos repositories enabled gets the OpenRepos flavour
# on update, because rpm treats it as the newer package.

# The Harbour RPM validator (and rpmlint) require stripped binaries, but the Mer
# SDK keeps debug symbols in the main package by default. Turn off automatic
# debuginfo/debugsource subpackages - they otherwise land in RPMS/, which is the
# directory the store upload files are taken from - and strip explicitly during the
# install stage below.
%define _enable_debug_packages 0
%define _debuginfo_packages 0
%define _debugsource_packages 0
%define _debugsource_policy skip
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  desktop-file-utils
BuildRequires:  qt5-qttools-linguist

%description
Automatically toggles Wi-Fi hotspot when connecting to a designated
car Bluetooth device.

%prep
%setup -q -n %{name}-%{version}

%build
%if %{with_service}
%qmake5 CONFIG+=service
%else
%qmake5
%endif
lrelease translations/*.ts
make

%install
rm -rf %{buildroot}
%qmake5_install
mkdir -p %{buildroot}%{_datadir}/%{name}/translations
install -m 644 translations/*.qm %{buildroot}%{_datadir}/%{name}/translations/

desktop-file-install --delete-original \
  --dir %{buildroot}%{_datadir}/applications \
  %{buildroot}%{_datadir}/applications/*.desktop

%if %{with_service}
# Replace the launcher entry with the flavour that may control the user service, and
# install the Sailjail profile used by the unit. Both keep the same application id, so
# the settings and data directory stay identical.
desktop-file-install --dir %{buildroot}%{_datadir}/applications \
  harbour-carhotspot-service.desktop
mv -f %{buildroot}%{_datadir}/applications/harbour-carhotspot-service.desktop \
     %{buildroot}%{_datadir}/applications/harbour-carhotspot.desktop
desktop-file-install --dir %{buildroot}%{_datadir}/applications \
  service/harbour-carhotspot-daemon.desktop
install -m 644 -D service/harbour-carhotspot.service \
  %{buildroot}/usr/lib/systemd/user/harbour-carhotspot.service
%endif

# Strip debug symbols so the Harbour RPM validator and rpmlint report a clean binary
%{__strip} --strip-unneeded %{buildroot}%{_bindir}/%{name} || true

%files
%defattr(-,root,root,-)
# The GPLv3 text is not installed: Jolla's validator rejects both /usr/share/licenses/
# <package>/ and /usr/share/doc/<package>/. The License: tag above is what the store shows,
# and the text itself ships in the source archive.
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%if %{with_service}
%{_datadir}/applications/harbour-carhotspot-daemon.desktop
/usr/lib/systemd/user/harbour-carhotspot.service
%endif

# Both scriptlets exist only in the OpenRepos flavour: Jolla's RPM validator rejects
# "postinstall" and "preuninstall" scripts outright and turns them into a
# "/bin/sh" dependency ("Dependency not allowed"), which rpmlint additionally reports as
# use-of-home-in-%post. The Harbour flavour therefore installs nothing after the package
# payload and never touches a home directory.
#
# Consequence: upgrading a *Harbour* package from 0.4.2 or older does not migrate the old
# Qt settings file, so the car has to be selected once. The OpenRepos flavour does migrate,
# and because both flavours share the sandbox-visible settings file, a user who installs
# the full flavour keeps everything.
%if %{with_service}
%post
# Settings migration. The Qt default configuration file
#   ${HOME}/.config/<OrganizationName>/<ApplicationName>.conf
# is NOT visible from inside the Sailjail sandbox (only
# ${HOME}/.config/<OrganizationName>/<ApplicationName> is whitelisted), so the values are
# copied into the sandbox-visible directory once. Runs as root during installation.
for H in /home/*/; do
    CONF="${H}.config/harbour-carhotspot/harbour-carhotspot.conf"
    [ -f "$CONF" ] || continue
    DIR="${CONF%/*}/harbour-carhotspot"
    NEW="$DIR/carhotspot.ini"
    [ -f "$NEW" ] && continue
    mkdir -p "$DIR" 2>/dev/null || continue
    cp -a "$CONF" "$NEW" 2>/dev/null || continue
    OWN="$(stat -c '%u:%g' "$CONF" 2>/dev/null)"
    [ -n "$OWN" ] && chown "$OWN" "$DIR" "$NEW" 2>/dev/null
done

# Enable the user service so the car automation also runs
# after a reboot, without any interaction in the UI. The application itself only starts
# and stops the unit over org.freedesktop.systemd1 (permission "User services").
for H in /home/*/; do
    U="$(basename "$H")"
    UID_NUM="$(id -u "$U" 2>/dev/null)" || continue
    WANTS="${H}.config/systemd/user/user-session.target.wants"
    mkdir -p "$WANTS" 2>/dev/null \
        && ln -sf /usr/lib/systemd/user/harbour-carhotspot.service "$WANTS/harbour-carhotspot.service" 2>/dev/null
    chown -R "$UID_NUM:$(id -g "$U" 2>/dev/null)" "${H}.config/systemd" 2>/dev/null
    if [ -S "/run/user/$UID_NUM/dbus/user_bus_socket" ]; then
        su -s /bin/sh "$U" -c "XDG_RUNTIME_DIR=/run/user/$UID_NUM DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$UID_NUM/dbus/user_bus_socket systemctl --user daemon-reload; XDG_RUNTIME_DIR=/run/user/$UID_NUM DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$UID_NUM/dbus/user_bus_socket systemctl --user --loglevel=err --no-pager start harbour-carhotspot.service" >/dev/null 2>&1
    fi
done
exit 0

%preun
# Stop and un-enable the service only when the package is really removed ($1 == 0), not
# during an upgrade. This also covers replacing the full flavour with the Harbour one:
# the unit is stopped, disabled and its file removed by the package being replaced.
if [ "$1" = "0" ]; then
    for H in /home/*/; do
        U="$(basename "$H")"
        UID_NUM="$(id -u "$U" 2>/dev/null)" || continue
        rm -f "${H}.config/systemd/user/user-session.target.wants/harbour-carhotspot.service"
        if [ -S "/run/user/$UID_NUM/dbus/user_bus_socket" ]; then
            su -s /bin/sh "$U" -c "XDG_RUNTIME_DIR=/run/user/$UID_NUM DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/$UID_NUM/dbus/user_bus_socket systemctl --user --loglevel=err disable --now harbour-carhotspot.service" >/dev/null 2>&1
        fi
    done
fi
exit 0
%endif

%changelog
* Thu Oct 29 2026 Milen <milen@github.com> - 0.4.6.1-1
- Paging is now a fallback instead of a monitoring strategy: a car kit that is switched on
  connects to the phone by itself and BlueZ reports org.bluez.Device1.Connected over D-Bus,
  which is what wakes the service and starts the hotspot. A burst is 2 page attempts (was 3)
  and the quiet window between bursts grows with every burst that found no car - 30 min, then
  1 h, then 2 h at most - so an absent car costs the combo controller almost nothing. A real
  Bluetooth power event, a connect/disconnect or picking a car resets the ladder to 30 min
- A screen wake no longer re-arms paging: charging, notifications and a glance at the clock all
  wake the display and none of them means the car is near. Advanced option pageOnWake=true in
  the settings file restores that behaviour for kits that only connect when the phone asks
- Car connection state is applied from the PropertiesChanged payload itself, so the automation
  reacts to the signal instead of waiting for the next watchdog tick, and the periodic full
  GetManagedObjects read is a 60s safety net instead of one per tick (was every 3s)

* Thu Oct 29 2026 Milen <milen@github.com> - 0.4.6-1
- Fix for the paging budget being refilled far too often: on a real device a screen wake fired
  every few seconds and each wake reset the attempts and the back-off, so the car was paged
  every 10-30s instead of three times per ten minutes - the exact radio load the budget exists
  to prevent, on the combo controller shared with the Android support stack. A wake (or an
  unrelated setting change) now only re-arms when the previous burst is at least ten minutes
  old and never skips the running back-off; an actual car selection still pages immediately
- The reason for a re-arm is now visible on the Diagnostics page ("Car paging re-armed (screen
  wake, 10 min since the last burst)"), so paging activity can be explained without root

* Thu Oct 29 2026 Milen <milen@github.com> - 0.4.5-1
- The Diagnostics page now states what the app is doing to the radio: each page attempt is
  logged ("Paging CAR MA7 (attempt 1/3 this cycle, radio duty ~10s)"), as is "not reported by
  BlueZ - nothing to page" when the car is simply out of range, throttled to one line per two
  minutes. Previously a support question like "is this app interfering with my other Bluetooth
  device?" could only be answered from the journal, which needs root
- The startup line no longer claims "Bluetooth switched on": the first powered-changed
  emission is the initial state read, not a power event
- Harbour packaging compliance: the store build now passes "sfdk check" with zero errors and
  zero warnings. Exec is declared as "Exec=harbour-carhotspot" (the validator rejects the
  absolute path), the custom ExecDBus key is dropped from the store desktop file, the GPLv3
  text is no longer installed (both /usr/share/licenses/<pkg>/ and /usr/share/doc/<pkg>/ are
  rejected - the License tag is what the store shows), and the %post/%preun scriptlets exist
  only in the OpenRepos flavour because Jolla rejects them and turns them into a forbidden
  /bin/sh dependency. Debuginfo/debugsource subpackages are switched off properly
- Release numbering: OpenRepos builds use 1or / 1sfos50or so they sort above the store
  Release 1. Both flavours share name and version, and rpm judges upgrades by NVR alone -
  with an identical Release a store->full swap reported "already installed" and did nothing
- Radio friendliness: phone-initiated paging of the car is limited to 3 attempts per cycle
  (15s/30s/60s back-off) and then stops, instead of paging an absent car every minute around
  the clock; a BR/EDR page holds the combo controller for ~10s and the Android support stack
  shares that radio. A cycle ends on a Bluetooth power change, a display wake, a real
  connect/disconnect or an action in the app, and also after 10 minutes of quiet - a user who
  never switches Bluetooth off must still be able to reach the car, so the worst case is
  ~30s of paging per 10 minutes instead of a page every 60s. Paging never runs while the
  access point is active or starting
- LE-only devices are never paged or connected: a BLE peripheral serves one central at a time,
  so connecting to a watch or GLE sensor takes it away from the Android stack that owns it
- BlueZ reads are asynchronous with a 3s cap and coalesced per signal burst; the previous
  blocking GetManagedObjects (Qt default timeout 25s) could freeze the calling thread, and
  adapter/ConnMan power reads are now capped at 2s

* Wed Oct 28 2026 Milen <milen@github.com> - 0.4.3-1
- Device verification on a Jolla Phone 2026 (Sailfish OS 5.2): switching the hotspot needs
  com.jolla.Connectiond, which Sailjail exposes only through the "Messages" permission, and
  ConnMan answers net.connman.Error.PermissionDenied for Technology.SetProperty("Tethering")
  from an ordinary user. The user service therefore runs the application binary directly
  (own user, no root, no shell) instead of through "sailjail -p", and the sandbox variant is
  kept as a documented systemctl override for Bluetooth monitoring only
- Settings and the shared activity log moved into QStandardPaths::AppConfigLocation, the
  only configuration directory a Sailjail sandbox can actually see and keep; the RPM %post
  migrates the old ~/.config/harbour-carhotspot/harbour-carhotspot.conf into it
- Manual hotspot requests are passed through the shared settings file (authoritative) plus a
  signal (fast path), because the D-Bus broadcast filter can drop signals
- Removed the notification remote-action hints: this Sailfish OS build has no remote-action
  support, a tap activates the application through the desktop-entry hint and ExecDBus
- Added a hidden --selftest mode that runs the real tethering path and reports the result
- Drop the com.nokia.time Run_Command shell execution entirely: it ran commands outside
  the Sailjail sandbox (Base.permission blocks exec/programs on purpose), which is both a
  privilege-escalation surface and a guaranteed Harbour rejection
- OpenRepos flavour (rpm macro with_service=1) ships a systemd user unit that runs the
  daemon inside the sandbox via "sailjail -p harbour-carhotspot-daemon.desktop", and the UI
  enables/starts/stops it through org.freedesktop.systemd1 (permission "User services")
- Harbour flavour keeps monitoring in the application process and tells the user about the
  OpenRepos flavour when background automation without manual action is wanted
- UI and service process now communicate with D-Bus signals only (Sailjail broadcasts
  org.sailfishos.coveraction.* but filters method calls between them); no blocking calls
  on the UI thread anymore
- Export only openApp/activate/status on D-Bus instead of every slot, so other sandboxed
  apps cannot toggle the hotspot, restart Bluetooth or write to the diagnostics log
- Grace-period shutdown now consumes the timed `cookie` signal (Send_Cookie + Wakeup
  event flag) instead of executing /bin/true
- Remember a manual "Hotspot OFF" across restarts; merge the shared activity log by entry
  timestamp instead of last-writer-wins; keep BlueZ address fallbacks from overwriting a
  user's device label
- Notification remote action uses the semicolon service;/path;interface;method format with
  a named action, plus desktop-entry hint and ExecDBus activation
- Package the GPLv3 LICENSE file, drop the empty documentation.list

* Tue Oct 27 2026 Milen <milen@github.com> - 0.4.2-1
- Strip binaries in the RPM so the Jolla Harbour RPM validator and rpmlint report no warnings
- Ship a LICENSE file (GPLv3) as required for Harbour submissions
- Play haptic feedback through NGF on the session bus (sysfs vibrator is not writable under Sailjail)
- Fix the Custom Bluetooth MAC field losing its binding to the selected device
- Selecting a car through the targetAddress property no longer deselects it in multi-device mode
- Add the missing translation entries (5 strings) for all 34 languages

* Sat Oct 17 2026 Milen <milen@github.com> - 0.4.1-1
- Fix background service car Bluetooth auto-reconnect after device reboot and screen wakeups
- Fix Wi-Fi remaining powered on in client mode after Hotspot shuts down
- Fix notifications disappearing instead of staying in the Events View notification bar

* Fri Oct 16 2026 Milen <milen@github.com> - 0.4.0-1
- Pure D-Bus Harbour-compliant architecture for Sailfish OS 5.2
