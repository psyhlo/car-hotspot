Name:       harbour-carhotspot
Summary:    Car Hotspot for Sailfish OS
Version:    0.4.3
Release:    1
License:    GPLv3
URL:        https://github.com/psyhlo/car-hotspot

# Two flavours from one source tree:
#   %with_service 0 (default) - Jolla Harbour build. No background service: monitoring
#                               lives with the application process, and the desktop file
#                               declares Bluetooth;Internet only.
#   %with_service 1           - OpenRepos build. Ships a systemd *user* unit that runs the
#                               daemon inside the Sailjail sandbox, and installs the
#                               desktop flavour that declares the "User services"
#                               permission (AppLaunch) used to start/stop that unit
#                               through org.freedesktop.systemd1.
# Set it from build-hotspot.sh (`./build-hotspot.sh build-all` writes the OpenRepos
# flavour into RPMS-openrepos/).
%define with_service 0

# The Harbour RPM validator (and rpmlint) require stripped binaries, but the Mer
# SDK keeps debug symbols in the main package by default. Turn off automatic
# debuginfo subpackages and strip explicitly during the install stage below.
%define _enable_debug_packages 0
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
%license LICENSE
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%if %{with_service}
%{_datadir}/applications/harbour-carhotspot-daemon.desktop
/usr/lib/systemd/user/harbour-carhotspot.service
%endif

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
    rm -f "${H}.config/harbour-carhotspot/harbour-carhotspot.service"
done

%if %{with_service}
# OpenRepos flavour: enable the sandboxed user service so the car automation also runs
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
%endif
exit 0

%preun
%if %{with_service}
# Stop and un-enable the service only when the package is really removed ($1 == 0), not
# during an upgrade.
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
%endif
exit 0

%changelog
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
