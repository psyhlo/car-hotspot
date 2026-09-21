#include "appcontroller.h"
#include <QDateTime>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTimer>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusVariant>
#include <QtDBus/QDBusInterface>
#include <QDebug>

AppController::AppController(bool isDaemon, QObject *parent)
    : QObject(parent)
    , m_isDaemon(isDaemon)
    , m_settings("harbour-carhotspot", "harbour-carhotspot")
{
    // Migrate old settings if present
    QSettings oldSettings("harbour-hotspotincar", "harbour-hotspotincar");
    if (m_settings.allKeys().isEmpty() && !oldSettings.allKeys().isEmpty()) {
        for (const QString &key : oldSettings.allKeys()) {
            m_settings.setValue(key, oldSettings.value(key));
        }
        m_settings.sync();
    }

    connect(&m_bluetooth, &BluetoothManager::targetConnectionChanged,
            this, &AppController::onTargetConnectionChanged);
    connect(&m_bluetooth, &BluetoothManager::devicesChanged,
            this, &AppController::onDevicesChanged);
    connect(&m_systemMonitor, &SystemMonitor::batteryChanged,
            this, &AppController::onBatteryChanged);
    connect(&m_systemMonitor, &SystemMonitor::roamingChanged,
            this, &AppController::onRoamingChanged);

    loadSettings();

    // Check if target car is already connected at startup
    if (m_bluetooth.isTargetConnected() && !m_hotspot.isHotspotActive()) {
        onTargetConnectionChanged(true);
    }

    m_delayedStopTimer = new QTimer(this);
    m_delayedStopTimer->setSingleShot(true);
    connect(m_delayedStopTimer, &QTimer::timeout, this, &AppController::onDelayedStopTimeout);

    m_disconnectDebounceTimer = new QTimer(this);
    m_disconnectDebounceTimer->setSingleShot(true);
    connect(m_disconnectDebounceTimer, &QTimer::timeout, this, &AppController::onDisconnectDebounceTimeout);

    // Periodic watchdog timer: keeps monitoring Bluetooth and Hotspot even if system D-Bus signals are missed in background/lockscreen
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() {
        m_bluetooth.refreshDevices();
        m_hotspot.checkStatus();
        if (m_isDaemon) {
            m_settings.sync();
            loadSettings();
        } else {
            checkDaemonStatus();
            reloadSharedLog();
        }
    });
    m_pollTimer->start(3000); // Poll every 3s

    if (m_isDaemon) {
        appendLog(QString("[%1] Background daemon started").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    } else {
        reloadSharedLog();
        appendLog(QString("[%1] UI connected to service").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    }

    checkDaemonStatus();
}

void AppController::loadSettings()
{
    m_targetAddress = m_settings.value("targetAddress", "").toString();
    m_targetName = m_settings.value("targetName", "").toString();
    m_autoToggle = m_settings.value("autoToggle", true).toBool();
    m_autostartService = m_settings.value("autostartService", true).toBool();
    m_showNotifications = m_settings.value("showNotifications", true).toBool();
    m_blockInRoaming = m_settings.value("blockInRoaming", true).toBool();
    m_minBatteryLevel = m_settings.value("minBatteryLevel", 20).toInt();
    m_stopDelayMinutes = m_settings.value("stopDelayMinutes", 2).toInt();
    m_enableCellularAuto = m_settings.value("enableCellularAuto", true).toBool();
    m_vibrateOnConnect = m_settings.value("vibrateOnConnect", true).toBool();

    if (!m_targetAddress.isEmpty()) {
        m_bluetooth.setTargetDevice(m_targetAddress);
    }
}

void AppController::saveSettings()
{
    m_settings.setValue("targetAddress", m_targetAddress);
    m_settings.setValue("targetName", m_targetName);
    m_settings.setValue("autoToggle", m_autoToggle);
    m_settings.setValue("autostartService", m_autostartService);
    m_settings.setValue("showNotifications", m_showNotifications);
    m_settings.setValue("blockInRoaming", m_blockInRoaming);
    m_settings.setValue("minBatteryLevel", m_minBatteryLevel);
    m_settings.setValue("stopDelayMinutes", m_stopDelayMinutes);
    m_settings.setValue("enableCellularAuto", m_enableCellularAuto);
    m_settings.setValue("vibrateOnConnect", m_vibrateOnConnect);
    m_settings.sync();
}

void AppController::setTargetAddress(const QString &address)
{
    if (m_targetAddress != address) {
        m_targetAddress = address;
        m_bluetooth.setTargetDevice(address);
        saveSettings();
        emit targetAddressChanged(m_targetAddress);
        appendLog(QString("[%1] Target set to: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), address));
    }
}

void AppController::setAutoToggle(bool enabled)
{
    if (m_autoToggle != enabled) {
        m_autoToggle = enabled;
        saveSettings();
        emit autoToggleChanged(m_autoToggle);
        appendLog(QString("[%1] Auto-toggle: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), enabled ? "ENABLED" : "DISABLED"));
    }
}

void AppController::setAutostartService(bool enabled)
{
    if (m_autostartService != enabled) {
        m_autostartService = enabled;
        saveSettings();
        syncSystemdService(enabled);
        emit autostartServiceChanged(m_autostartService);
        appendLog(QString("[%1] Background autostart: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), enabled ? "ENABLED" : "DISABLED"));
    }
}

void AppController::setShowNotifications(bool enabled)
{
    if (m_showNotifications != enabled) {
        m_showNotifications = enabled;
        saveSettings();
        emit showNotificationsChanged(m_showNotifications);
        appendLog(QString("[%1] Notifications: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), enabled ? "ENABLED" : "DISABLED"));
    }
}

void AppController::setBlockInRoaming(bool enabled)
{
    if (m_blockInRoaming != enabled) {
        m_blockInRoaming = enabled;
        saveSettings();
        emit blockInRoamingChanged(m_blockInRoaming);
        appendLog(QString("[%1] Block in Roaming: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), enabled ? "YES" : "NO"));
    }
}

void AppController::setMinBatteryLevel(int level)
{
    if (m_minBatteryLevel != level) {
        m_minBatteryLevel = level;
        saveSettings();
        emit minBatteryLevelChanged(m_minBatteryLevel);
        appendLog(QString("[%1] Battery threshold: %2%").arg(QDateTime::currentDateTime().toString("hh:mm:ss")).arg(level));
    }
}

void AppController::setStopDelayMinutes(int minutes)
{
    if (m_stopDelayMinutes != minutes) {
        m_stopDelayMinutes = minutes;
        saveSettings();
        emit stopDelayMinutesChanged(m_stopDelayMinutes);
        appendLog(QString("[%1] Turn-off delay: %2 min").arg(QDateTime::currentDateTime().toString("hh:mm:ss")).arg(minutes));
    }
}

void AppController::setEnableCellularAuto(bool enabled)
{
    if (m_enableCellularAuto != enabled) {
        m_enableCellularAuto = enabled;
        saveSettings();
        emit enableCellularAutoChanged(m_enableCellularAuto);
        appendLog(QString("[%1] Auto-enable cellular data: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), enabled ? "YES" : "NO"));
    }
}

void AppController::setVibrateOnConnect(bool enabled)
{
    if (m_vibrateOnConnect != enabled) {
        m_vibrateOnConnect = enabled;
        saveSettings();
        emit vibrateOnConnectChanged(m_vibrateOnConnect);
        appendLog(QString("[%1] Haptic vibration: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), enabled ? "YES" : "NO"));
    }
}

bool AppController::checkSafetyConditions(QString &reason)
{
    // Roaming check
    if (m_blockInRoaming && m_systemMonitor.isRoaming()) {
        reason = "Device is currently in roaming! Hotspot blocked to prevent high costs.";
        return false;
    }

    // Battery check (if not charging and below threshold)
    if (!m_systemMonitor.isBatteryCharging() && m_systemMonitor.batteryChargePercentage() <= m_minBatteryLevel) {
        reason = QString("Battery too low (%1% <= %2%) and not charging! Hotspot blocked.")
                 .arg(m_systemMonitor.batteryChargePercentage())
                 .arg(m_minBatteryLevel);
        return false;
    }

    return true;
}

void AppController::checkDaemonStatus()
{
    QProcess p;
    p.start("systemctl", QStringList() << "--user" << "is-active" << "harbour-carhotspot.service");
    if (p.waitForFinished(1000)) {
        QString out = p.readAllStandardOutput().trimmed();
        bool active = (out == "active");
        if (m_isDaemonActive != active) {
            m_isDaemonActive = active;
            emit isDaemonActiveChanged(m_isDaemonActive);
        }
    }
}

void AppController::syncSystemdService(bool enable)
{
    QString configDir = QDir::homePath() + "/.config/systemd/user";
    QString legacyServicePath = configDir + "/harbour-carhotspot.service";
    if (QFile::exists(legacyServicePath)) {
        QFile::remove(legacyServicePath);
    }

    if (enable) {
        QProcess::execute("systemctl", QStringList() << "--user" << "daemon-reload");
        QProcess::execute("systemctl", QStringList() << "--user" << "enable" << "harbour-carhotspot.service");
        QProcess::execute("systemctl", QStringList() << "--user" << "restart" << "harbour-carhotspot.service");
        checkDaemonStatus();
        sendNotification("Car Hotspot", "Background service is ACTIVE and monitoring Bluetooth");
    } else {
        QProcess::execute("systemctl", QStringList() << "--user" << "stop" << "harbour-carhotspot.service");
        QProcess::execute("systemctl", QStringList() << "--user" << "disable" << "harbour-carhotspot.service");
        QProcess::execute("systemctl", QStringList() << "--user" << "daemon-reload");
        checkDaemonStatus();
        sendNotification("Car Hotspot", "Background service STOPPED");
    }
}

void AppController::sendNotification(const QString &summary, const QString &body)
{
    if (!m_showNotifications)
        return;

    QDBusMessage msg = QDBusMessage::createMethodCall(
        "org.freedesktop.Notifications",
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        "Notify"
    );

    QVariantMap hints;
    hints.insert("category", "x-nemo.general");
    hints.insert("x-nemo-preview-summary", summary);
    hints.insert("x-nemo-preview-body", body);
    hints.insert("sound-file", "message-new-email");

    QList<QVariant> args;
    args << QString("Car Hotspot");
    args << (uint)0;
    args << QString("harbour-carhotspot");
    args << summary;
    args << body;
    args << QStringList();
    args << hints;
    args << (int)4000;

    msg.setArguments(args);
    QDBusConnection::sessionBus().send(msg);
}

void AppController::selectDevice(const QString &address, const QString &name)
{
    m_targetAddress = address;
    m_targetName = name;
    m_bluetooth.setTargetDevice(address);
    saveSettings();
    emit targetAddressChanged(m_targetAddress);
    emit targetNameChanged(m_targetName);
    appendLog(QString("[%1] Selected car: %2 (%3)").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), name, address));
}

void AppController::toggleHotspotManual(bool active)
{
    // If manual toggle occurs, stop any running grace or debounce timer
    if (m_disconnectDebounceTimer && m_disconnectDebounceTimer->isActive()) {
        m_disconnectDebounceTimer->stop();
    }
    if (m_delayedStopTimer && m_delayedStopTimer->isActive()) {
        m_delayedStopTimer->stop();
    }

    if (active) {
        QString failReason;
        if (!checkSafetyConditions(failReason)) {
            appendLog(QString("[%1] Warning: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), failReason));
            sendNotification("Hotspot blocked", failReason);
            return;
        }
        ensureCellularConnected();
        triggerFeedback();
    }

    appendLog(QString("[%1] Manual toggle Hotspot: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), active ? "ON" : "OFF"));
    m_hotspot.setHotspotActive(active);
    sendNotification(
        "Car Hotspot", 
        active ? "Hotspot manually enabled" : "Hotspot manually disabled"
    );
}

void AppController::appendLog(const QString &text)
{
    // Reload latest shared log first so events from both daemon and GUI merge properly
    reloadSharedLog();

    if (!m_logStatus.isEmpty()) {
        m_logStatus = text + "\n" + m_logStatus;
        QStringList lines = m_logStatus.split("\n");
        if (lines.size() > 40) {
            lines = lines.mid(0, 40);
            m_logStatus = lines.join("\n");
        }
    } else {
        m_logStatus = text;
    }

    m_settings.setValue("activityLog", m_logStatus);
    m_settings.sync();
    emit logStatusChanged(m_logStatus);
}

void AppController::reloadSharedLog()
{
    m_settings.sync();
    QString diskLog = m_settings.value("activityLog", "").toString();
    if (m_logStatus != diskLog && !diskLog.isEmpty()) {
        m_logStatus = diskLog;
        emit logStatusChanged(m_logStatus);
    }
}

void AppController::triggerFeedback()
{
    if (!m_vibrateOnConnect)
        return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastFeedbackTime < 30000) {
        qDebug() << "AppController: Skipping vibration feedback due to 30s cooldown";
        return;
    }
    m_lastFeedbackTime = now;

    // 1. Direct hardware vibrator sysfs (/sys/class/leds/vibrator/) used by Sony Xperia & Sailfish OS
    QFile vibDuration("/sys/class/leds/vibrator/duration");
    QFile vibActivate("/sys/class/leds/vibrator/activate");
    if (vibDuration.open(QIODevice::WriteOnly | QIODevice::Text) &&
        vibActivate.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream durStream(&vibDuration);
        durStream << "350\n";
        vibDuration.close();

        QTextStream actStream(&vibActivate);
        actStream << "1\n";
        vibActivate.close();
    } else {
        // Fallback for devices with legacy timed_output vibrator
        QFile legacyVib("/sys/class/timed_output/vibrator/enable");
        if (legacyVib.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&legacyVib);
            out << "350\n";
            legacyVib.close();
        }
    }

    // 2. Trigger Sailfish OS NGF (Non-Graphical Feedback) on SYSTEM bus
    QDBusInterface ngf(
        "com.nokia.NonGraphicFeedback1.Backend",
        "/",
        "com.nokia.NonGraphicFeedback1.Backend",
        QDBusConnection::systemBus()
    );

    if (ngf.isValid()) {
        ngf.call("Play", "vibra", QVariantMap());
    }

    // 3. Direct timedclient vibration fallback
    QProcess::startDetached("timedclient-qt5", QStringList() << "--vibrate" << "350");
}

void AppController::ensureCellularConnected()
{
    if (!m_enableCellularAuto)
        return;

    // 1. Enable cellular technology via ConnMan
    QDBusInterface cellTech(
        "net.connman",
        "/net/connman/technology/cellular",
        "net.connman.Technology",
        QDBusConnection::systemBus()
    );

    if (cellTech.isValid()) {
        cellTech.call("SetProperty", "Powered", QVariant::fromValue(QDBusVariant(true)));
    }

    // 2. Privileged helper fallback in case unprivileged call was blocked by D-Bus policy
    QProcess::execute("sudo", QStringList() << "/usr/bin/harbour-carhotspot-helper" << "cellular-on");
}

void AppController::onTargetConnectionChanged(bool connected)
{
    // If background systemd daemon is active, it handles the connection automation.
    // The GUI should not duplicate actions, timers, vibrations or notifications.
    if (!m_isDaemon && m_isDaemonActive) {
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString carLabel = !m_targetName.isEmpty() ? m_targetName : "Car";

    if (connected) {
        // 1. If disconnect debounce timer was active (micro-disconnect flutter < 5s), cancel it!
        if (m_disconnectDebounceTimer && m_disconnectDebounceTimer->isActive()) {
            m_disconnectDebounceTimer->stop();
            appendLog(QString("[%1] %2 reconnected (flutter absorbed, no spam).").arg(timestamp, carLabel));
            return;
        }

        // 2. If a delayed shutdown grace timer was ticking (driver returned within grace period):
        if (m_delayedStopTimer && m_delayedStopTimer->isActive()) {
            m_delayedStopTimer->stop();
            appendLog(QString("[%1] Reconnected to %2 within grace period. Hotspot maintained.").arg(timestamp, carLabel));
            // Hotspot was already active! Do NOT vibrate, do NOT send annoying popup notification.
            return;
        }

        // 3. If Hotspot is ALREADY active:
        if (m_hotspot.isHotspotActive()) {
            appendLog(QString("[%1] %2 connected (Hotspot already active).").arg(timestamp, carLabel));
            return;
        }

        // 4. Genuine NEW connection event when Hotspot was OFF
        appendLog(QString("[%1] %2 connected!").arg(timestamp, carLabel));

        if (m_autoToggle) {
            QString failReason;
            if (!checkSafetyConditions(failReason)) {
                appendLog(QString("[%1] Safety Check Failed: %2").arg(timestamp, failReason));
                sendNotification("Hotspot blocked", failReason);
                return;
            }

            // Ensure cellular data is active
            ensureCellularConnected();

            appendLog(QString("[%1] Auto-enabling Hotspot...").arg(timestamp));
            m_hotspot.setHotspotActive(true);
            triggerFeedback();
            sendNotification("Car Hotspot", QString("%1 connected: Wi-Fi Hotspot turned ON").arg(carLabel));
        } else {
            sendNotification("Car Hotspot", QString("%1 connected").arg(carLabel));
        }
    } else {
        // Bluetooth reported disconnected:
        if (m_autoToggle && m_hotspot.isHotspotActive()) {
            // Debounce for 5 seconds to filter out brief BlueZ/handshake drops
            appendLog(QString("[%1] %2 link dropped. Filtering brief drop (5s)...").arg(timestamp, carLabel));
            m_disconnectDebounceTimer->start(5000);
        } else {
            appendLog(QString("[%1] %2 disconnected.").arg(timestamp, carLabel));
            sendNotification("Car Hotspot", QString("%1 disconnected").arg(carLabel));
        }
    }
}

void AppController::onDisconnectDebounceTimeout()
{
    if (!m_isDaemon && m_isDaemonActive) {
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString carLabel = !m_targetName.isEmpty() ? m_targetName : "Car";

    // Safety guard: if car reconnected during debounce, abort stop sequence
    if (m_bluetooth.isTargetConnected()) {
        appendLog(QString("[%1] %2 is currently connected. Disconnect sequence cancelled.").arg(timestamp, carLabel));
        return;
    }

    appendLog(QString("[%1] %2 disconnect confirmed.").arg(timestamp, carLabel));

    if (m_autoToggle && m_hotspot.isHotspotActive()) {
        if (m_stopDelayMinutes > 0) {
            appendLog(QString("[%1] Starting turn-off grace timer (%2 min)...").arg(timestamp).arg(m_stopDelayMinutes));
            sendNotification("Car Hotspot", QString("%1 disconnected. Hotspot will turn OFF in %2 min").arg(carLabel).arg(m_stopDelayMinutes));
            m_delayedStopTimer->start(m_stopDelayMinutes * 60 * 1000);
        } else {
            appendLog(QString("[%1] Auto-disabling Hotspot immediately...").arg(timestamp));
            m_hotspot.setHotspotActive(false);
            sendNotification("Car Hotspot", QString("%1 disconnected: Wi-Fi Hotspot turned OFF").arg(carLabel));
        }
    }
}

void AppController::onDelayedStopTimeout()
{
    if (!m_isDaemon && m_isDaemonActive) {
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString carLabel = !m_targetName.isEmpty() ? m_targetName : "Car";

    // Safety guard: if car reconnected during grace period, abort stop sequence
    if (m_bluetooth.isTargetConnected()) {
        appendLog(QString("[%1] %2 is currently connected. Grace stop cancelled.").arg(timestamp, carLabel));
        return;
    }

    appendLog(QString("[%1] Grace period (%2 min) expired. Auto-disabling Hotspot.").arg(timestamp).arg(m_stopDelayMinutes));
    m_hotspot.setHotspotActive(false);
    sendNotification("Car Hotspot", "Grace timer expired: Wi-Fi Hotspot turned OFF");
}

void AppController::onBatteryChanged(int percentage, bool charging)
{
    if (!m_isDaemon && m_isDaemonActive) {
        return;
    }

    // If Hotspot is running, not charging, and battery dropped below limit -> auto-stop to protect phone
    if (m_hotspot.isHotspotActive() && !charging && percentage > 0 && percentage <= m_minBatteryLevel) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        appendLog(QString("[%1] Battery critical (%2%)! Auto-stopping Hotspot.").arg(timestamp).arg(percentage));
        m_hotspot.setHotspotActive(false);
        sendNotification("Hotspot turned OFF", QString("Battery dropped to %1%. Hotspot stopped to prevent battery drain.").arg(percentage));
    }
}

void AppController::onRoamingChanged(bool roaming)
{
    if (!m_isDaemon && m_isDaemonActive) {
        return;
    }

    // If roaming detected while Hotspot is running and block option is on -> immediate shutdown
    if (roaming && m_blockInRoaming && m_hotspot.isHotspotActive()) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        appendLog(QString("[%1] Roaming detected! Auto-stopping Hotspot to prevent high costs.").arg(timestamp));
        m_hotspot.setHotspotActive(false);
        sendNotification("Hotspot turned OFF", "Roaming detected! Wi-Fi Hotspot turned off to avoid extra carrier charges.");
    }
}

void AppController::onDevicesChanged()
{
    if (!m_targetAddress.isEmpty()) {
        const auto devs = m_bluetooth.devices();
        for (const QVariant &v : devs) {
            QVariantMap m = v.toMap();
            if (m.value("address").toString().compare(m_targetAddress, Qt::CaseInsensitive) == 0) {
                QString currentName = m.value("name").toString();
                if (m_targetName != currentName) {
                    m_targetName = currentName;
                    saveSettings();
                    emit targetNameChanged(m_targetName);
                }
                break;
            }
        }
    }
}
