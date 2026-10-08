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
    bool tethering = false;
    bool checked = false;

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
            tethering = reply.value().value("Tethering", false).toBool();
            checked = true;
        }
    }

    // Fallback: query via dbus-send
    if (!checked) {
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
            int tIdx = out.indexOf("\"Tethering\"");
            if (tIdx != -1) {
                QString snippet = out.mid(tIdx, 80);
                tethering = snippet.contains("boolean true", Qt::CaseInsensitive);
                checked = true;
            }
        }
    }

    if (m_isHotspotActive != tethering) {
        m_isHotspotActive = tethering;
        emit hotspotActiveChanged(m_isHotspotActive);
    }
    m_statusMessage = m_isHotspotActive ? "Hotspot is ON" : "Hotspot is OFF";
    emit statusMessageChanged(m_statusMessage);

    // Watchdog: If we requested Hotspot to be ON, but it is not active yet (e.g. stuck in Wi-Fi station mode), enforce tethering-on!
    if (m_targetHotspotState && !m_isHotspotActive && m_watchdogRetryCount < 3) {
        m_watchdogRetryCount++;
        qDebug() << "HotspotManager: Hotspot not active yet (watchdog retry" << m_watchdogRetryCount << "). Forcing tethering-on...";
        QProcess::startDetached("sudo", QStringList() << "/usr/bin/harbour-carhotspot-helper" << "tethering-on");
        QTimer::singleShot(1500, this, &HotspotManager::checkStatus);
    } else if (m_isHotspotActive) {
        m_watchdogRetryCount = 0;
    }
}

void HotspotManager::updateHotspotState(bool active)
{
    if (m_isHotspotActive != active) {
        m_isHotspotActive = active;
        emit hotspotActiveChanged(m_isHotspotActive);
        m_statusMessage = m_isHotspotActive ? "Hotspot is ON" : "Hotspot is OFF";
        emit statusMessageChanged(m_statusMessage);
    }
}

void HotspotManager::setHotspotActive(bool active)
{
    m_targetHotspotState = active;
    m_watchdogRetryCount = 0;
    m_statusMessage = active ? "Enabling Hotspot..." : "Disabling Hotspot...";
    emit statusMessageChanged(m_statusMessage);

    QSettings settings("harbour-carhotspot", "harbour-carhotspot");

    if (active) {
        // Only capture Wi-Fi powered state if tethering wasn't already running
        if (!m_isHotspotActive) {
            m_wifiWasPowered = queryWifiPoweredState();
            settings.setValue("wifiWasPoweredBeforeTethering", m_wifiWasPowered);
            settings.setValue("wifiStateNeedsRestoration", !m_wifiWasPowered);
            settings.sync();
            emit wifiWasPoweredBeforeChanged(m_wifiWasPowered);
            qDebug() << "HotspotManager: Wi-Fi was powered before tethering:" << m_wifiWasPowered;
        }

        // 1. Invoke Connectiond (Sailfish OS session daemon)
        QDBusInterface connDaemon(
            "com.jolla.Connectiond",
            "/Connectiond",
            "com.jolla.Connectiond",
            QDBusConnection::sessionBus()
        );
        if (connDaemon.isValid()) {
            connDaemon.asyncCall("startTethering", QString("wifi"));
        }

        // 2. Invoke privileged helper to guarantee ConnMan switches to Access Point mode
        QProcess::startDetached("sudo", QStringList() << "/usr/bin/harbour-carhotspot-helper" << "tethering-on");
    } else {
        // When wifi was not powered initially, ensure connectionagent's internal state
        // has tetheringTechPowered = false so that it will automatically power Wi-Fi off.
        if (!m_wifiWasPowered) {
            QSettings caSettings("nemomobile", "connectionagent");
            caSettings.beginGroup("Connectionagent");
            caSettings.setValue("tetheringTechPowered", false);
            caSettings.sync();
        }

        // 1. Invoke Connectiond stopTethering
        QDBusInterface connDaemon(
            "com.jolla.Connectiond",
            "/Connectiond",
            "com.jolla.Connectiond",
            QDBusConnection::sessionBus()
        );
        if (connDaemon.isValid()) {
            connDaemon.asyncCall("stopTethering", QString("wifi"), false);
        }

        // 2. Invoke privileged helper tethering-off
        QProcess::startDetached("sudo", QStringList() << "/usr/bin/harbour-carhotspot-helper" << "tethering-off");
    }

    // Check status after 1200ms delay
    QTimer::singleShot(1200, this, &HotspotManager::checkStatus);
}

void HotspotManager::restoreWifiStateIfNeeded()
{
    // Guard 1: NEVER restore (turn off) Wi-Fi if Hotspot is currently active!
    if (m_isHotspotActive) {
        qDebug() << "HotspotManager: restoreWifiStateIfNeeded skipped: Hotspot is currently active.";
        return;
    }

    // Guard 2: Double check with ConnMan directly to ensure Tethering is genuinely stopped
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
            if (tethering) {
                qDebug() << "HotspotManager: restoreWifiStateIfNeeded aborted: ConnMan reports Tethering is still active!";
                m_isHotspotActive = true;
                emit hotspotActiveChanged(m_isHotspotActive);
                return;
            }
        }
    }

    // Guard 3: Only restore if a restoration was explicitly scheduled by Car Hotspot
    QSettings settings("harbour-carhotspot", "harbour-carhotspot");
    bool needsRestoration = settings.value("wifiStateNeedsRestoration", false).toBool();
    if (!needsRestoration) {
        return;
    }

    // Clear restoration flag immediately so no other thread/process duplicates this
    settings.setValue("wifiStateNeedsRestoration", false);
    settings.sync();

    // Guard 4: Check if Wi-Fi was powered before tethering
    bool wifiWasPowered = settings.value("wifiWasPoweredBeforeTethering", true).toBool();
    if (wifiWasPowered) {
        qDebug() << "HotspotManager: Wi-Fi was originally ON before tethering, leaving it ON.";
        return;
    }

    qDebug() << "HotspotManager: Wi-Fi was OFF before tethering. Powering off Wi-Fi technology now...";

    // 1. Privileged helper via sudo
    QProcess::execute("sudo", QStringList() << "/usr/bin/harbour-carhotspot-helper" << "wifi-off");

    // 2. Direct privileged dbus-send via sudo fallback
    QProcess::execute("sudo", QStringList() 
        << "dbus-send" << "--system" << "--dest=net.connman"
        << "/net/connman/technology/wifi"
        << "net.connman.Technology.SetProperty"
        << "string:Powered" << "variant:boolean:false"
    );

    // 3. Direct QDBusInterface in case permission is granted
    if (wifiTech.isValid()) {
        wifiTech.call("SetProperty", "Powered", QVariant::fromValue(QDBusVariant(false)));
    }

    emit restoreWifiRequested(false);
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
                // When ConnMan reports tethering actually stopped, restore Wi-Fi
                QTimer::singleShot(800, this, [this]() {
                    restoreWifiStateIfNeeded();
                });
            }
        }
    }
}
