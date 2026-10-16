#include "hotspotmanager.h"
#include "timeddbus.h"
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QSettings>
#include <QTimer>
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

    return false;
}

void HotspotManager::enableWifiTetheringDbus()
{
    // 1. Invoke Connectiond (Sailfish OS session daemon, runs as privileged)
    QDBusMessage startMsg = QDBusMessage::createMethodCall(
        "com.jolla.Connectiond",
        "/Connectiond",
        "com.jolla.Connectiond",
        "startTethering"
    );
    startMsg << QStringLiteral("wifi");
    QDBusMessage connReply = QDBusConnection::sessionBus().call(startMsg);
    if (connReply.type() == QDBusMessage::ErrorMessage) {
        // Running inside Sailjail where session bus filters Connectiond -> trigger via com.nokia.time
        TimedDbus::runInstantCommand(QStringLiteral("/usr/bin/dbus-send --session --type=method_call --dest=com.jolla.Connectiond /Connectiond com.jolla.Connectiond.startTethering string:wifi"));
    }

    // 2. Direct D-Bus call to ConnMan Wi-Fi Technology (permitted by Internet/Connman permission)
    QDBusInterface wifiTech(
        "net.connman",
        "/net/connman/technology/wifi",
        "net.connman.Technology",
        QDBusConnection::systemBus()
    );

    if (wifiTech.isValid()) {
        QDBusReply<QVariantMap> reply = wifiTech.call("GetProperties");
        bool isPowered = reply.isValid() && reply.value().value("Powered", false).toBool();
        bool isTethering = reply.isValid() && reply.value().value("Tethering", false).toBool();

        if (isTethering) {
            return;
        }

        if (!isPowered) {
            wifiTech.asyncCall("SetProperty", QString("Powered"), QVariant::fromValue(QDBusVariant(true)));
            // Give Wi-Fi chip 650ms to power on before enabling Access Point mode
            QTimer::singleShot(650, this, [this]() {
                if (!m_targetHotspotState || m_isHotspotActive)
                    return;
                QDBusInterface wt(
                    "net.connman",
                    "/net/connman/technology/wifi",
                    "net.connman.Technology",
                    QDBusConnection::systemBus()
                );
                if (wt.isValid()) {
                    wt.asyncCall("SetProperty", QString("Tethering"), QVariant::fromValue(QDBusVariant(true)));
                }
            });
        } else {
            wifiTech.asyncCall("SetProperty", QString("Tethering"), QVariant::fromValue(QDBusVariant(true)));
        }
    }
}

void HotspotManager::checkStatus()
{
    bool tethering = false;

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
        }
    }

    if (m_isHotspotActive != tethering) {
        m_isHotspotActive = tethering;
        emit hotspotActiveChanged(m_isHotspotActive);
    }
    m_statusMessage = m_isHotspotActive ? "Hotspot is ON" : "Hotspot is OFF";
    emit statusMessageChanged(m_statusMessage);

    // Watchdog: Allow up to 4 retries (~8-10 seconds total) for ConnMan and the Wi-Fi chip
    // to bring up the Access Point. Do NOT trigger a false alarm while AP is initializing.
    if (m_targetHotspotState && !m_isHotspotActive) {
        if (m_watchdogRetryCount < 4) {
            m_watchdogRetryCount++;
            qDebug() << "HotspotManager: Hotspot AP initializing (retry" << m_watchdogRetryCount << "/4)...";
            enableWifiTetheringDbus();
            QTimer::singleShot(2000, this, &HotspotManager::checkStatus);
        } else if (!m_startFailureReported) {
            // All retries exhausted (>8-10 seconds elapsed) and tethering is genuinely not running
            m_startFailureReported = true;
            bool wifiOn = queryWifiPoweredState();
            QString reason = wifiOn
                ? QStringLiteral("Wi-Fi was turned on in client mode instead of Hotspot. Try toggling Wi-Fi off/on or restart the Wi-Fi module.")
                : QStringLiteral("Hotspot failed to start and Wi-Fi is off. Check that mobile data/SIM is available.");
            qWarning() << "HotspotManager:" << reason;
            emit hotspotStartFailed(reason);
        }
    } else if (m_isHotspotActive) {
        m_watchdogRetryCount = 0;
        m_startFailureReported = false;
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
    m_startFailureReported = false;
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

        enableWifiTetheringDbus();
    } else {
        // When turning off, clear failure flag and reset watchdog
        m_watchdogRetryCount = 0;
        m_startFailureReported = false;

        // 1. Invoke Connectiond stopTethering(type, keepPowered)
        // Passing m_wifiWasPowered tells Connectiond whether to leave Wi-Fi ON or turn it OFF!
        QDBusMessage stopMsg = QDBusMessage::createMethodCall(
            "com.jolla.Connectiond",
            "/Connectiond",
            "com.jolla.Connectiond",
            "stopTethering"
        );
        stopMsg << QStringLiteral("wifi") << m_wifiWasPowered;
        QDBusMessage connReply = QDBusConnection::sessionBus().call(stopMsg);
        if (connReply.type() == QDBusMessage::ErrorMessage) {
            TimedDbus::runInstantCommand(
                QStringLiteral("/usr/bin/dbus-send --session --type=method_call --dest=com.jolla.Connectiond /Connectiond com.jolla.Connectiond.stopTethering string:wifi boolean:%1")
                .arg(m_wifiWasPowered ? QStringLiteral("true") : QStringLiteral("false"))
            );
        }

        // 2. Direct D-Bus call to ConnMan Wi-Fi Technology as fallback
        QDBusInterface wifiTech(
            "net.connman",
            "/net/connman/technology/wifi",
            "net.connman.Technology",
            QDBusConnection::systemBus()
        );
        if (wifiTech.isValid()) {
            wifiTech.asyncCall("SetProperty", QString("Tethering"), QVariant::fromValue(QDBusVariant(false)));
        }
    }

    // Check status after initial 2000ms delay to give ConnMan time to change states
    QTimer::singleShot(2000, this, &HotspotManager::checkStatus);
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

    // Ask Connectiond (which runs with privileged group) to ensure Wi-Fi is powered off
    QDBusMessage stopMsg = QDBusMessage::createMethodCall(
        "com.jolla.Connectiond",
        "/Connectiond",
        "com.jolla.Connectiond",
        "stopTethering"
    );
    stopMsg << QStringLiteral("wifi") << false;
    QDBusMessage connReply = QDBusConnection::sessionBus().call(stopMsg);
    if (connReply.type() == QDBusMessage::ErrorMessage) {
        TimedDbus::runInstantCommand(QStringLiteral("/usr/bin/dbus-send --session --type=method_call --dest=com.jolla.Connectiond /Connectiond com.jolla.Connectiond.stopTethering string:wifi boolean:false"));
    }

    if (wifiTech.isValid()) {
        wifiTech.asyncCall("SetProperty", QString("Powered"), QVariant::fromValue(QDBusVariant(false)));
    }

    emit restoreWifiRequested(false);
}

void HotspotManager::onPropertyChanged(const QString &name, const QDBusVariant &value)
{
    if (name == "Tethering") {
        bool tethering = value.variant().toBool();
        if (m_isHotspotActive != tethering) {
            m_isHotspotActive = tethering;
            if (m_isHotspotActive) {
                m_watchdogRetryCount = 0;
                m_startFailureReported = false;
            }
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
