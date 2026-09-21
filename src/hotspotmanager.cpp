#include "hotspotmanager.h"
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QSettings>
#include <QTimer>
#include <QProcess>
#include <QDebug>

HotspotManager::HotspotManager(QObject *parent)
    : QObject(parent)
{
    // Load previously remembered Wi-Fi state if daemon/app was restarted while Hotspot was on
    QSettings settings("harbour-carhotspot", "harbour-carhotspot");
    m_wifiWasPowered = settings.value("wifiWasPoweredBeforeTethering", false).toBool();

    // ConnMan technology interface for WiFi
    QDBusConnection::systemBus().connect(
        "net.connman",
        "/net/connman/technology/wifi",
        "net.connman.Technology",
        "PropertyChanged",
        this,
        SLOT(onPropertyChanged(QString, QDBusVariant))
    );

    checkStatus();
}

bool HotspotManager::queryWifiPoweredState()
{
    // 1. Direct QDBusInterface to ConnMan WiFi Technology
    QDBusInterface wifiTech(
        "net.connman",
        "/net/connman/technology/wifi",
        "net.connman.Technology",
        QDBusConnection::systemBus()
    );

    if (wifiTech.isValid()) {
        QDBusReply<QVariantMap> reply = wifiTech.call("GetProperties");
        if (reply.isValid()) {
            return reply.value().value("Powered", false).toBool();
        }
    }

    // 2. Fallback: query via dbus-send
    QProcess p;
    p.start("dbus-send", QStringList() 
        << "--system" 
        << "--print-reply" 
        << "--dest=net.connman" 
        << "/net/connman/technology/wifi" 
        << "net.connman.Technology.GetProperties"
    );

    if (p.waitForFinished(1500)) {
        QString out = p.readAllStandardOutput();
        int pIdx = out.indexOf("\"Powered\"");
        if (pIdx != -1) {
            QString snippet = out.mid(pIdx, 80);
            return snippet.contains("boolean true", Qt::CaseInsensitive);
        }
    }

    return false;
}

void HotspotManager::checkStatus()
{
    // Direct QDBusInterface to ConnMan WiFi Technology
    QDBusInterface wifiTech(
        "net.connman",
        "/net/connman/technology/wifi",
        "net.connman.Technology",
        QDBusConnection::systemBus()
    );

    if (wifiTech.isValid()) {
        QDBusReply<QVariantMap> reply = wifiTech.call("GetProperties");
        if (reply.isValid()) {
            bool tethering = reply.value().value("Tethering", false).toBool();
            if (m_isHotspotActive != tethering) {
                m_isHotspotActive = tethering;
                emit hotspotActiveChanged(m_isHotspotActive);
            }
            m_statusMessage = m_isHotspotActive ? "Hotspot is ON" : "Hotspot is OFF";
            emit statusMessageChanged(m_statusMessage);
            return;
        }
    }

    // Fallback: query via dbus-send
    QProcess p;
    p.start("dbus-send", QStringList() 
        << "--system" 
        << "--print-reply" 
        << "--dest=net.connman" 
        << "/net/connman/technology/wifi" 
        << "net.connman.Technology.GetProperties"
    );

    if (p.waitForFinished(2000)) {
        QString out = p.readAllStandardOutput();
        int tIdx = out.indexOf("\"Tethering\"");
        if (tIdx != -1) {
            QString snippet = out.mid(tIdx, 80);
            bool tethering = snippet.contains("boolean true", Qt::CaseInsensitive);
            if (m_isHotspotActive != tethering) {
                m_isHotspotActive = tethering;
                emit hotspotActiveChanged(m_isHotspotActive);
            }
            m_statusMessage = m_isHotspotActive ? "Hotspot is ON" : "Hotspot is OFF";
            emit statusMessageChanged(m_statusMessage);
        }
    }
}

void HotspotManager::updateHotspotState(bool active)
{
    if (m_isHotspotActive != active) {
        m_isHotspotActive = active;
        emit hotspotActiveChanged(m_isHotspotActive);
        m_statusMessage = m_isHotspotActive ? "Hotspot is ON" : "Hotspot is OFF";
        emit statusMessageChanged(m_statusMessage);

        if (!active) {
            restoreWifiStateIfNeeded();
        }
    }
}

void HotspotManager::setHotspotActive(bool active)
{
    m_statusMessage = active ? "Enabling Hotspot..." : "Disabling Hotspot...";
    emit statusMessageChanged(m_statusMessage);

    if (active) {
        m_wifiStateRestored = false;
        // Only capture Wi-Fi powered state if tethering wasn't already running
        if (!m_isHotspotActive) {
            m_wifiWasPowered = queryWifiPoweredState();
            QSettings settings("harbour-carhotspot", "harbour-carhotspot");
            settings.setValue("wifiWasPoweredBeforeTethering", m_wifiWasPowered);
            settings.sync();
            emit wifiWasPoweredBeforeChanged(m_wifiWasPowered);
            qDebug() << "HotspotManager: Wi-Fi was powered before tethering:" << m_wifiWasPowered;
        }
    }

    // 1. Always send D-Bus call to com.jolla.Connectiond (works in background, locked screen, daemon)
    QDBusInterface connDaemon(
        "com.jolla.Connectiond",
        "/Connectiond",
        "com.jolla.Connectiond",
        QDBusConnection::sessionBus()
    );

    if (connDaemon.isValid()) {
        if (active) {
            connDaemon.asyncCall("startTethering", QString("wifi"));
        } else {
            // When wifi was not powered initially, ensure connectionagent's internal state
            // has tetheringTechPowered = false so that it will automatically power Wi-Fi off.
            if (!m_wifiWasPowered) {
                QSettings caSettings("nemomobile", "connectionagent");
                caSettings.beginGroup("Connectionagent");
                caSettings.setValue("tetheringTechPowered", false);
                caSettings.sync();
            }

            // stopTethering expects (QString type, bool keepPowered).
            // Passing false instructs connectionagent to restore Wi-Fi power to off if it was off!
            connDaemon.asyncCall("stopTethering", QString("wifi"), false);
        }
    }

    // 2. Also trigger QML declarative ConnectionAgent if GUI is alive
    emit hotspotToggleRequested(active);

    if (!active) {
        // Schedule Wi-Fi restore check in case stopTethering takes a moment
        QTimer::singleShot(1500, this, [this]() {
            restoreWifiStateIfNeeded();
        });
    }

    // 3. Verify status after brief delay
    checkStatus();
}

void HotspotManager::restoreWifiStateIfNeeded()
{
    if (m_wifiStateRestored)
        return;

    QSettings settings("harbour-carhotspot", "harbour-carhotspot");
    bool wifiWasPowered = settings.value("wifiWasPoweredBeforeTethering", m_wifiWasPowered).toBool();

    qDebug() << "HotspotManager: restoreWifiStateIfNeeded. wifiWasPoweredBeforeTethering was:" << wifiWasPowered;

    if (!wifiWasPowered) {
        m_wifiStateRestored = true;
        qDebug() << "HotspotManager: Wi-Fi was OFF before tethering. Powering off Wi-Fi technology now...";

        // 1. Connectiond session D-Bus interface (stopTethering with keepPowered=false)
        QSettings caSettings("nemomobile", "connectionagent");
        caSettings.beginGroup("Connectionagent");
        caSettings.setValue("tetheringTechPowered", false);
        caSettings.sync();

        QDBusInterface connDaemon(
            "com.jolla.Connectiond",
            "/Connectiond",
            "com.jolla.Connectiond",
            QDBusConnection::sessionBus()
        );
        if (connDaemon.isValid()) {
            connDaemon.asyncCall("stopTethering", QString("wifi"), false);
        }

        // 2. Direct QDBusInterface in case permission is granted
        QDBusInterface wifiTech(
            "net.connman",
            "/net/connman/technology/wifi",
            "net.connman.Technology",
            QDBusConnection::systemBus()
        );
        if (wifiTech.isValid()) {
            wifiTech.call("SetProperty", "Powered", QVariant::fromValue(QDBusVariant(false)));
        }

        // 3. Privileged helper / fallback if available
        QProcess::execute("sudo", QStringList() << "/usr/bin/harbour-carhotspot-helper" << "wifi-off");

        // 4. Emit signal for QML NetworkTechnology
        emit restoreWifiRequested(false);
    } else {
        m_wifiStateRestored = true;
        qDebug() << "HotspotManager: Wi-Fi was originally ON, leaving it ON.";
    }
}

void HotspotManager::onPropertyChanged(const QString &name, const QDBusVariant &value)
{
    if (name == "Tethering") {
        bool tethering = value.variant().toBool();
        if (m_isHotspotActive != tethering) {
            m_isHotspotActive = tethering;
            emit hotspotActiveChanged(m_isHotspotActive);
            m_statusMessage = m_isHotspotActive ? "Hotspot is ON" : "Hotspot is OFF";
            emit statusMessageChanged(m_statusMessage);

            if (!tethering) {
                // When ConnMan reports tethering stopped, restore Wi-Fi
                QTimer::singleShot(800, this, [this]() {
                    restoreWifiStateIfNeeded();
                });
            }
        }
    }
}
