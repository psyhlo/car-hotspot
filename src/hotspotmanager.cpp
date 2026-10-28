#include "hotspotmanager.h"
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QDateTime>
#include <QSettings>

#include "carhotspotsettings.h"
#include <QTimer>
#include <QDebug>

HotspotManager::HotspotManager(QObject *parent)
    : QObject(parent)
{
    // Load previously remembered Wi-Fi state if daemon/app was restarted while Hotspot was on
    QSettings settings(carhotspotSettingsFilePath(), QSettings::IniFormat);
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
    QDBusInterface wifiTech(
        "net.connman",
        "/net/connman/technology/wifi",
        "net.connman.Technology",
        QDBusConnection::systemBus()
    );

    bool isPowered = false;
    bool isTethering = false;
    if (wifiTech.isValid()) {
        QDBusReply<QVariantMap> reply = wifiTech.call("GetProperties");
        if (reply.isValid()) {
            isPowered = reply.value().value("Powered", false).toBool();
            isTethering = reply.value().value("Tethering", false).toBool();
        }
    }

    if (isTethering) {
        return;
    }

    // 1. Invoke Connectiond ONLY on initial attempt (when retryCount == 0 and Powered matches initial state).
    // Calling startTethering again after Wi-Fi is already Powered=true causes Connectiond to overwrite
    // its internal tetheringTechPowered=true setting and leave Wi-Fi ON when tethering stops!
    // 1. Ask Connectiond first (it also brings up cellular data + DHCP/NAT for the
    // shared connection). Inside a Sailjail sandbox com.jolla.Connectiond is not part
    // of the Bluetooth/Internet permission set, so this call usually fails there; that
    // is fine because ConnMan's own Tethering property below does the actual work and
    // Wi-Fi power restoration is handled by restoreWifiStateIfNeeded().
    if (m_watchdogRetryCount == 0 && (isPowered == m_wifiWasPowered)) {
        QDBusMessage startMsg = QDBusMessage::createMethodCall(
            "com.jolla.Connectiond",
            "/Connectiond",
            "com.jolla.Connectiond",
            "startTethering"
        );
        startMsg << QStringLiteral("wifi");
        QDBusMessage connReply = QDBusConnection::sessionBus().call(startMsg, QDBus::Block, 2000);
        if (connReply.type() == QDBusMessage::ErrorMessage) {
            qDebug() << "HotspotManager: Connectiond startTethering unavailable ("
                     << connReply.errorName() << "), relying on ConnMan Tethering property";
        }
    }

    // 2. Direct D-Bus call to ConnMan Wi-Fi Technology (permitted by Internet/Connman permission)
    if (wifiTech.isValid()) {
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
    } else if (!m_targetHotspotState && !m_isHotspotActive) {
        QSettings settings(carhotspotSettingsFilePath(), QSettings::IniFormat);
        settings.sync();
        if (settings.value("wifiStateNeedsRestoration", false).toBool()) {
            restoreWifiStateIfNeeded();
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
    }
}

void HotspotManager::setHotspotActive(bool active)
{
    bool wasTargetingActive = m_targetHotspotState;
    m_targetHotspotState = active;
    m_watchdogRetryCount = 0;
    m_startFailureReported = false;
    m_statusMessage = active ? "Enabling Hotspot..." : "Disabling Hotspot...";
    emit statusMessageChanged(m_statusMessage);

    QSettings settings(carhotspotSettingsFilePath(), QSettings::IniFormat);
    settings.sync();

    if (active) {
        // Only capture Wi-Fi powered state if tethering wasn't already running or starting
        if (!m_isHotspotActive && !wasTargetingActive) {
            bool pendingRestore = settings.value("wifiStateNeedsRestoration", false).toBool();
            if (pendingRestore) {
                // Previous hotspot session had Wi-Fi OFF and hadn't finished powering down yet
                m_wifiWasPowered = false;
            } else {
                m_wifiWasPowered = queryWifiPoweredState();
            }
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
        m_lastStopTimestamp = QDateTime::currentMSecsSinceEpoch();

        m_wifiWasPowered = settings.value("wifiWasPoweredBeforeTethering", false).toBool();
        if (!m_wifiWasPowered) {
            settings.setValue("wifiStateNeedsRestoration", true);
            settings.sync();
        }

        // 1. Ask Connectiond to stop tethering, telling it whether Wi-Fi should stay
        // powered. Best-effort only: it is not reachable from a sandboxed process.
        QDBusMessage stopMsg = QDBusMessage::createMethodCall(
            "com.jolla.Connectiond",
            "/Connectiond",
            "com.jolla.Connectiond",
            "stopTethering"
        );
        stopMsg << QStringLiteral("wifi") << m_wifiWasPowered;
        QDBusMessage connReply = QDBusConnection::sessionBus().call(stopMsg, QDBus::Block, 2000);
        if (connReply.type() == QDBusMessage::ErrorMessage) {
            qDebug() << "HotspotManager: Connectiond stopTethering unavailable ("
                     << connReply.errorName() << "), Wi-Fi power handled by restoration logic";
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

        // Schedule multi-stage Wi-Fi restoration checks so even if ConnMan/wpa_supplicant
        // re-powers station mode ~1.5-3s after tearing down the AP, Wi-Fi is reliably turned OFF.
        if (!m_wifiWasPowered) {
            for (int delayMs : {1000, 2500, 4500}) {
                QTimer::singleShot(delayMs, this, &HotspotManager::restoreWifiStateIfNeeded);
            }
        }
    }

    // Check status after initial 2000ms delay to give ConnMan time to change states
    QTimer::singleShot(2000, this, &HotspotManager::checkStatus);
}

void HotspotManager::restoreWifiStateIfNeeded()
{
    // Guard 1: NEVER restore (turn off) Wi-Fi if Hotspot is currently active or being enabled!
    if (m_isHotspotActive || m_targetHotspotState) {
        qDebug() << "HotspotManager: restoreWifiStateIfNeeded skipped: Hotspot is currently active.";
        return;
    }

    // Guard 2: Query ConnMan directly for both Tethering and Powered states
    bool tethering = false;
    bool powered = false;
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
            powered = reply.value().value("Powered", false).toBool();
            if (tethering) {
                qDebug() << "HotspotManager: restoreWifiStateIfNeeded aborted: ConnMan reports Tethering is still active!";
                m_isHotspotActive = true;
                emit hotspotActiveChanged(m_isHotspotActive);
                return;
            }
        }
    }

    // Guard 3: Only restore if a restoration was explicitly scheduled by Car Hotspot
    QSettings settings(carhotspotSettingsFilePath(), QSettings::IniFormat);
    settings.sync();
    bool needsRestoration = settings.value("wifiStateNeedsRestoration", false).toBool();
    if (!needsRestoration) {
        return;
    }

    // Guard 4: Check if Wi-Fi was powered before tethering
    bool wifiWasPowered = settings.value("wifiWasPoweredBeforeTethering", false).toBool();
    if (wifiWasPowered) {
        qDebug() << "HotspotManager: Wi-Fi was originally ON before tethering, leaving it ON.";
        settings.setValue("wifiStateNeedsRestoration", false);
        settings.sync();
        return;
    }

    qint64 elapsedSinceStop = QDateTime::currentMSecsSinceEpoch() - m_lastStopTimestamp;

    // If Wi-Fi is already OFF and at least 3.5s have passed since tethering stopped
    // (so ConnMan has finished its AP -> station mode transition), restoration is complete!
    if (!powered) {
        if (m_lastStopTimestamp == 0 || elapsedSinceStop >= 3500) {
            settings.setValue("wifiStateNeedsRestoration", false);
            settings.sync();
            qDebug() << "HotspotManager: Confirmed Wi-Fi is OFF after tethering. Restoration complete.";
        }
        return;
    }

    qDebug() << "HotspotManager: Wi-Fi was OFF before tethering but is currently ON. Powering off Wi-Fi technology now...";

    if (wifiTech.isValid()) {
        wifiTech.asyncCall("SetProperty", QString("Powered"), QVariant::fromValue(QDBusVariant(false)));
    }

    // Verify again in 1500ms until ConnMan confirms Powered == false
    QTimer::singleShot(1500, this, &HotspotManager::restoreWifiStateIfNeeded);

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
            } else {
                m_lastStopTimestamp = QDateTime::currentMSecsSinceEpoch();
            }
            emit hotspotActiveChanged(m_isHotspotActive);
            m_statusMessage = m_isHotspotActive ? "Hotspot is ON" : "Hotspot is OFF";
            emit statusMessageChanged(m_statusMessage);

            if (!tethering && !m_targetHotspotState) {
                // When ConnMan reports tethering actually stopped, restore Wi-Fi across transition window
                for (int delayMs : {800, 2200, 4000}) {
                    QTimer::singleShot(delayMs, this, &HotspotManager::restoreWifiStateIfNeeded);
                }
            }
        }
    } else if (name == "Powered") {
        bool powered = value.variant().toBool();
        if (powered && !m_isHotspotActive && !m_targetHotspotState) {
            // If ConnMan/Connectiond re-powered Wi-Fi right after AP teardown while restoration is pending, turn it back off
            QTimer::singleShot(500, this, &HotspotManager::restoreWifiStateIfNeeded);
        }
    }
}
