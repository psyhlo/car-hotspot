TARGET = harbour-carhotspot

# Single Source of Truth for Version: read directly from rpm spec file
SPEC_FILE = $$_PRO_FILE_PWD_/rpm/harbour-carhotspot.spec
APP_VERSION = $$system(sed -n -e 's/^Version:[[:space:]]*//p' $$SPEC_FILE)
isEmpty(APP_VERSION): APP_VERSION = 0.0.0
DEFINES += APP_VERSION=$$APP_VERSION

CONFIG += sailfishapp c++17
QT += core gui qml quick dbus

# Two distribution flavours of the same source tree:
#
#   (default)        Jolla Harbour build. Monitoring and automation live with the
#                    application process; nothing installs a service and the desktop file
#                    declares only Bluetooth;Internet. The UI points the user at the
#                    OpenRepos flavour when they want automation without manual action.
#   CONFIG+=service   OpenRepos build (rpm spec macro %with_service). Adds the
#                    org.freedesktop.systemd1 based service control (permission
#                    "User services" = AppLaunch) and ships the systemd user unit.
#                    See rpm/harbour-carhotspot.spec for the packaging side.
contains(CONFIG, service): DEFINES += CARHOTSPOT_SERVICE_BUILD=1

SOURCES += \
    src/main.cpp \
    src/bluetoothmanager.cpp \
    src/dbusadapter.cpp \
    src/hotspotmanager.cpp \
    src/systemmonitor.cpp \
    src/appcontroller.cpp

HEADERS += \
    src/timeddbus.h \
    src/bluetoothmanager.h \
    src/dbusadapter.h \
    src/hotspotmanager.h \
    src/systemmonitor.h \
    src/appcontroller.h
    src/carhotspotsettings.h

target.path = /usr/bin

qml.files = qml
qml.path = /usr/share/$$TARGET

desktop.files = harbour-carhotspot.desktop
desktop.path = /usr/share/applications

icon86.files = icons/86x86/harbour-carhotspot.png
icon86.path = /usr/share/icons/hicolor/86x86/apps

icon108.files = icons/108x108/harbour-carhotspot.png
icon108.path = /usr/share/icons/hicolor/108x108/apps

icon128.files = icons/128x128/harbour-carhotspot.png
icon128.path = /usr/share/icons/hicolor/128x128/apps

icon172.files = icons/172x172/harbour-carhotspot.png
icon172.path = /usr/share/icons/hicolor/172x172/apps

TRANSLATIONS += $$files(translations/*.ts)

translations.files = translations/*.qm
translations.path = /usr/share/$$TARGET/translations

INSTALLS += target qml desktop icon86 icon108 icon128 icon172 translations


