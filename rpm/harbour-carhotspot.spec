Name:       harbour-carhotspot
Summary:    Car Hotspot for Sailfish OS
Version:    0.4.2
Release:    1
License:    GPLv3
URL:        https://github.com/psyhlo/car-hotspot

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
%qmake5
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

# Strip debug symbols so the Harbour RPM validator reports a clean binary
%{__strip} --strip-unneeded %{buildroot}%{_bindir}/%{name} || true

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png

%changelog
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
- Redesigned Multiple Car Devices selection and status UI
