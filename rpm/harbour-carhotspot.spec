Name:       harbour-carhotspot
Summary:    Car Hotspot for Sailfish OS
Version:    0.4.1
Release:    1
License:    GPLv3
URL:        https://github.com/psyhlo/car-hotspot
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

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png

%changelog
* Sat Oct 17 2026 Milen <milen@github.com> - 0.4.1-1
- Fix background service car Bluetooth auto-reconnect after device reboot and screen wakeups
- Fix Wi-Fi remaining powered on in client mode after Hotspot shuts down
- Fix notifications disappearing instead of staying in the Events View notification bar

* Fri Oct 16 2026 Milen <milen@github.com> - 0.4.0-1
- Pure D-Bus Harbour-compliant architecture for Sailfish OS 5.2
- Redesigned Multiple Car Devices selection and status UI
