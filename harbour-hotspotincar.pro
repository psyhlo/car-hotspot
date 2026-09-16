TARGET = harbour-hotspotincar

CONFIG += sailfishapp c++17
QT += core gui qml quick dbus

SOURCES += \
    src/main.cpp \
    src/bluetoothmanager.cpp \
    src/hotspotmanager.cpp \
    src/appcontroller.cpp

HEADERS += \
    src/bluetoothmanager.h \
    src/hotspotmanager.h \
    src/appcontroller.h

DISTFILES += \
    qml/harbour-hotspotincar.qml \
    qml/pages/MainPage.qml \
    qml/cover/CoverPage.qml \
    rpm/harbour-hotspotincar.spec \
    harbour-hotspotincar.desktop

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172
