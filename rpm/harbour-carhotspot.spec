Name:       harbour-carhotspot
Summary:    Car Hotspot for Sailfish OS
Version:    0.1.27
Release:    1
License:    GPLv3
URL:        https://github.com
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
Automatically toggles Wi-Fi hotspot when connecting to a designated car Bluetooth device.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
%make_build

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}%{_bindir}
mkdir -p %{buildroot}%{_datadir}/%{name}/qml
mkdir -p %{buildroot}%{_datadir}/%{name}/translations
mkdir -p %{buildroot}%{_datadir}/translations
mkdir -p %{buildroot}%{_datadir}/applications
mkdir -p %{buildroot}%{_datadir}/icons/hicolor/86x86/apps
mkdir -p %{buildroot}%{_datadir}/icons/hicolor/108x108/apps
mkdir -p %{buildroot}%{_datadir}/icons/hicolor/128x128/apps
mkdir -p %{buildroot}%{_datadir}/icons/hicolor/172x172/apps
mkdir -p %{buildroot}%{_sysconfdir}/sudoers.d

install -m 755 harbour-carhotspot %{buildroot}%{_bindir}/%{name}
cp -r qml/* %{buildroot}%{_datadir}/%{name}/qml/
install -m 644 translations/*.qm %{buildroot}%{_datadir}/%{name}/translations/
install -m 644 translations/*.qm %{buildroot}%{_datadir}/translations/
install -m 644 harbour-carhotspot.desktop %{buildroot}%{_datadir}/applications/
install -m 644 harbour-carhotspot.service %{buildroot}%{_datadir}/%{name}/
install -m 440 harbour-carhotspot.sudoers %{buildroot}%{_sysconfdir}/sudoers.d/harbour-carhotspot
install -m 644 icons/86x86/harbour-carhotspot.png %{buildroot}%{_datadir}/icons/hicolor/86x86/apps/
install -m 644 icons/108x108/harbour-carhotspot.png %{buildroot}%{_datadir}/icons/hicolor/108x108/apps/
install -m 644 icons/128x128/harbour-carhotspot.png %{buildroot}%{_datadir}/icons/hicolor/128x128/apps/
install -m 644 icons/172x172/harbour-carhotspot.png %{buildroot}%{_datadir}/icons/hicolor/172x172/apps/

%post
chmod 440 %{_sysconfdir}/sudoers.d/harbour-carhotspot || true

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/translations/%{name}*.qm
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
%config %{_sysconfdir}/sudoers.d/harbour-carhotspot
