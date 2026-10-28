#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QString>

// Sailjail/firejail only makes these three directories visible (and persistent) for a
// sandboxed application:
//   ${HOME}/.config/<OrganizationName>/<ApplicationName>
//   ${HOME}/.local/share/<OrganizationName>/<ApplicationName>
//   ${HOME}/.cache/<OrganizationName>/<ApplicationName>
// The Qt default settings file "${HOME}/.config/<OrganizationName>/<ApplicationName>.conf"
// is NOT one of them: inside the sandbox it lands in a private copy that is thrown away
// when the process exits, so the UI process and the systemd daemon process would not see
// each other's values (and nothing would survive a reboot). Everything shared between the
// two processes therefore goes into QStandardPaths::AppConfigLocation, which is exactly
// the whitelisted configuration directory.
inline QString carhotspotSettingsFilePath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath() + "/.config/harbour-carhotspot/harbour-carhotspot";
    }
    QDir().mkpath(dir);

    const QString path = dir + "/carhotspot.ini";
    // Best effort migration of the Qt default file. A sandboxed process cannot read it
    // (that path is private), the RPM %post scriptlet does the real migration as root.
    const QString legacy = QDir::homePath() + "/.config/harbour-carhotspot/harbour-carhotspot.conf";
    if (!QFileInfo::exists(path) && QFileInfo::exists(legacy)) {
        QFile::copy(legacy, path);
    }
    return path;
}

// Usage: QSettings settings(carhotspotSettingsFilePath(), QSettings::IniFormat);
// (QSettings has a deleted copy constructor, so it cannot be returned by value.)
// Qt writes the native configuration file on Unix in the same INI syntax, so reading the
// migrated file with IniFormat keeps the typed values (@Variant entries included).
