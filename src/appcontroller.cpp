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

    checkDaemonStatus();
    loadSettings();

    // If autoEnableBluetooth is turned on, ensure Bluetooth is powered
    if (m_autoEnableBluetooth) {
        m_bluetooth.ensureBluetoothPowered();
    }

    connect(&m_bluetooth, &BluetoothManager::targetConnectionChanged,
            this, &AppController::onTargetConnectionChanged);
    connect(&m_bluetooth, &BluetoothManager::targetConnectionChanged,
            this, &AppController::connectedTargetNameChanged);
    connect(&m_bluetooth, &BluetoothManager::devicesChanged,
            this, &AppController::onDevicesChanged);
    connect(&m_systemMonitor, &SystemMonitor::batteryChanged,
            this, &AppController::onBatteryChanged);
    connect(&m_systemMonitor, &SystemMonitor::roamingChanged,
            this, &AppController::onRoamingChanged);

    // Automation timers are only active in the daemon process (or standalone GUI if daemon is disabled)
    bool isHandlingAutomation = m_isDaemon || (!m_isDaemonActive && !m_autostartService);

    // Check if target car is already connected at startup (only if handling automation)
    if (isHandlingAutomation && m_bluetooth.isTargetConnected() && !m_hotspot.isHotspotActive()) {
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
        if (m_autoEnableBluetooth && !m_bluetooth.isBluetoothPowered()) {
            m_bluetooth.ensureBluetoothPowered();
        }
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

    // Register D-Bus object so notification clicks and remote actions activate/open the app
    QDBusConnection::sessionBus().registerService("harbour.carhotspot");
    QDBusConnection::sessionBus().registerObject("/", this, QDBusConnection::ExportAllSlots);

    if (!m_isDaemon) {
        QDBusConnection::sessionBus().registerService("harbour.carhotspot.ui");
    }
}

void AppController::loadSettings()
{
    m_allowMultipleDevices = m_settings.value("allowMultipleDevices", false).toBool();
    m_targetAddress = m_settings.value("targetAddress", "").toString();
    m_targetName = m_settings.value("targetName", "").toString();
    m_targetAddresses = m_settings.value("targetAddresses", QStringList()).toStringList();

    QVariantMap namesMap = m_settings.value("targetNames").toMap();
    for (auto it = namesMap.constBegin(); it != namesMap.constEnd(); ++it) {
        m_targetNames[it.key().toUpper()] = it.value().toString();
    }

    // Migration from single targetAddress if targetAddresses is empty
    if (m_targetAddresses.isEmpty() && !m_targetAddress.isEmpty()) {
        m_targetAddresses << m_targetAddress.toUpper();
        if (!m_targetName.isEmpty()) {
            m_targetNames[m_targetAddress.toUpper()] = m_targetName;
        }
    }

    // If single device mode is active, enforce only 1 device
    if (!m_allowMultipleDevices && m_targetAddresses.size() > 1) {
        m_targetAddresses = QStringList() << m_targetAddresses.first();
    }

    if (!m_targetAddresses.isEmpty()) {
        m_targetAddress = m_targetAddresses.first();
        m_targetName = m_targetNames.value(m_targetAddress.toUpper(), m_targetName);
    } else {
        m_targetAddress.clear();
        m_targetName.clear();
    }

    m_targetAddressesSet.clear();
    for (const QString &addr : m_targetAddresses) {
        m_targetAddressesSet.insert(addr.trimmed().toUpper());
    }

    m_autoToggle = m_settings.value("autoToggle", true).toBool();
    m_autostartService = m_settings.value("autostartService", true).toBool();
    m_showNotifications = m_settings.value("showNotifications", true).toBool();
    m_blockInRoaming = m_settings.value("blockInRoaming", true).toBool();
    m_minBatteryLevel = m_settings.value("minBatteryLevel", 20).toInt();
    m_stopDelayMinutes = m_settings.value("stopDelayMinutes", 2).toInt();
    m_autoEnableBluetooth = m_settings.value("autoEnableBluetooth", false).toBool();
    m_enableCellularAuto = m_settings.value("enableCellularAuto", true).toBool();
    m_vibrateOnConnect = m_settings.value("vibrateOnConnect", true).toBool();

    m_bluetooth.setTargetDevices(m_targetAddresses);
}

void AppController::saveSettings()
{
    m_settings.setValue("allowMultipleDevices", m_allowMultipleDevices);
    m_settings.setValue("targetAddress", m_targetAddress);
    m_settings.setValue("targetName", m_targetName);
    m_settings.setValue("targetAddresses", m_targetAddresses);

    QVariantMap namesMap;
    for (auto it = m_targetNames.constBegin(); it != m_targetNames.constEnd(); ++it) {
        namesMap[it.key()] = it.value();
    }
    m_settings.setValue("targetNames", namesMap);

    m_settings.setValue("autoToggle", m_autoToggle);
    m_settings.setValue("autostartService", m_autostartService);
    m_settings.setValue("showNotifications", m_showNotifications);
    m_settings.setValue("blockInRoaming", m_blockInRoaming);
    m_settings.setValue("minBatteryLevel", m_minBatteryLevel);
    m_settings.setValue("stopDelayMinutes", m_stopDelayMinutes);
    m_settings.setValue("autoEnableBluetooth", m_autoEnableBluetooth);
    m_settings.setValue("enableCellularAuto", m_enableCellularAuto);
    m_settings.setValue("vibrateOnConnect", m_vibrateOnConnect);
    m_settings.sync();
}

void AppController::setAutoEnableBluetooth(bool enabled)
{
    if (m_autoEnableBluetooth != enabled) {
        m_autoEnableBluetooth = enabled;
        saveSettings();
        emit autoEnableBluetoothChanged(m_autoEnableBluetooth);
        appendLog(QString("[%1] Auto-enable Bluetooth: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), enabled ? "YES" : "NO"));
        if (enabled) {
            m_bluetooth.ensureBluetoothPowered();
        }
    }
}

void AppController::setTargetAddress(const QString &address)
{
    selectDevice(address, address);
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
        reason = tr("Device is currently in roaming! Hotspot blocked to prevent high costs.");
        return false;
    }

    // Battery check (if not charging and below threshold)
    if (!m_systemMonitor.isBatteryCharging() && m_systemMonitor.batteryChargePercentage() <= m_minBatteryLevel) {
        reason = tr("Battery too low (%1% <= %2%) and not charging! Hotspot blocked.")
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
        sendNotification(tr("Car Hotspot"), tr("Background service is ACTIVE and monitoring Bluetooth"));
    } else {
        QProcess::execute("systemctl", QStringList() << "--user" << "stop" << "harbour-carhotspot.service");
        QProcess::execute("systemctl", QStringList() << "--user" << "disable" << "harbour-carhotspot.service");
        QProcess::execute("systemctl", QStringList() << "--user" << "daemon-reload");
        checkDaemonStatus();
        sendNotification(tr("Car Hotspot"), tr("Background service STOPPED"));
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
    hints.insert("x-nemo-remote-action-default", "harbour.carhotspot / harbour.carhotspot openApp");

    QStringList actions;
    actions << "default" << "";

    QList<QVariant> args;
    args << QString("Car Hotspot");
    args << (uint)0;
    args << QString("harbour-carhotspot");
    args << summary;
    args << body;
    args << actions;
    args << hints;
    args << (int)4000;

    msg.setArguments(args);
    QDBusConnection::sessionBus().send(msg);
}

void AppController::openApp()
{
    qDebug() << "AppController::openApp() called from notification. Daemon:" << m_isDaemon;
    if (m_isDaemon) {
        QDBusInterface guiApp(
            "harbour.carhotspot.ui",
            "/",
            "harbour.carhotspot.ui",
            QDBusConnection::sessionBus()
        );
        if (guiApp.isValid()) {
            guiApp.call("activate");
        } else {
            // Launch GUI application
            QProcess::startDetached("/usr/bin/harbour-carhotspot");
        }
    } else {
        emit requestActivateWindow();
    }
}

void AppController::activate()
{
    openApp();
}

void AppController::setAllowMultipleDevices(bool enabled)
{
    if (m_allowMultipleDevices != enabled) {
        m_allowMultipleDevices = enabled;

        if (!m_allowMultipleDevices) {
            // Returning to single device mode:
            // If more than 1 device is selected, retain only the first (oldest) choice
            if (m_targetAddresses.size() > 1) {
                QString primaryAddress = m_targetAddresses.first();
                QString primaryName = m_targetNames.value(primaryAddress.toUpper(), primaryAddress);

                m_targetAddresses = QStringList() << primaryAddress;
                m_targetAddressesSet.clear();
                m_targetAddressesSet.insert(primaryAddress.toUpper());
                m_targetAddress = primaryAddress;
                m_targetName = primaryName;

                // Clean up extra names from map
                QMap<QString, QString> keptNames;
                keptNames[primaryAddress.toUpper()] = primaryName;
                m_targetNames = keptNames;

                appendLog(QString("[%1] Switched to single device mode. Retained primary device: %2 (%3)")
                          .arg(QDateTime::currentDateTime().toString("hh:mm:ss"), primaryName, primaryAddress));
            } else if (m_targetAddresses.size() == 1) {
                m_targetAddress = m_targetAddresses.first();
                m_targetName = m_targetNames.value(m_targetAddress.toUpper(), m_targetAddress);
            } else {
                m_targetAddress.clear();
                m_targetName.clear();
            }
        } else {
            // Switched to multiple devices mode:
            if (!m_targetAddress.isEmpty() && !isDeviceSelected(m_targetAddress)) {
                QString clean = m_targetAddress.toUpper();
                m_targetAddresses.append(clean);
                m_targetAddressesSet.insert(clean);
                m_targetNames[clean] = m_targetName;
            }
            appendLog(QString("[%1] Switched to multiple devices mode.").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
        }

        m_bluetooth.setTargetDevices(m_targetAddresses);
        saveSettings();
        emit allowMultipleDevicesChanged(m_allowMultipleDevices);
        emit targetAddressesChanged();
        emit targetAddressChanged(m_targetAddress);
        emit targetNameChanged(m_targetName);
        emit selectedDevicesCountChanged();
    }
}

bool AppController::isDeviceSelected(const QString &address) const
{
    return m_targetAddressesSet.contains(address.trimmed().toUpper());
}

void AppController::addDevice(const QString &address, const QString &name)
{
    QString cleanAddr = address.trimmed().toUpper();
    if (cleanAddr.isEmpty())
        return;

    QString cleanName = name.trimmed().isEmpty() ? cleanAddr : name.trimmed();

    if (!isDeviceSelected(cleanAddr)) {
        if (!m_allowMultipleDevices) {
            m_targetAddresses.clear();
            m_targetAddressesSet.clear();
            m_targetNames.clear();
        }
        m_targetAddresses.append(cleanAddr);
        m_targetAddressesSet.insert(cleanAddr);
        m_targetNames[cleanAddr] = cleanName;
        m_targetAddress = m_targetAddresses.first();
        m_targetName = m_targetNames.value(m_targetAddress);

        m_bluetooth.setTargetDevices(m_targetAddresses);
        saveSettings();
        emit targetAddressesChanged();
        emit targetAddressChanged(m_targetAddress);
        emit targetNameChanged(m_targetName);
        emit selectedDevicesCountChanged();
        appendLog(QString("[%1] Added car device: %2 (%3)").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), cleanName, cleanAddr));
    }
}

void AppController::removeDevice(const QString &address)
{
    QString cleanAddr = address.trimmed().toUpper();
    int idx = -1;
    for (int i = 0; i < m_targetAddresses.size(); ++i) {
        if (m_targetAddresses.at(i).toUpper() == cleanAddr) {
            idx = i;
            break;
        }
    }

    if (idx >= 0) {
        QString devName = m_targetNames.value(cleanAddr, cleanAddr);
        m_targetAddresses.removeAt(idx);
        m_targetAddressesSet.remove(cleanAddr);
        m_targetNames.remove(cleanAddr);

        if (!m_targetAddresses.isEmpty()) {
            m_targetAddress = m_targetAddresses.first();
            m_targetName = m_targetNames.value(m_targetAddress);
        } else {
            m_targetAddress.clear();
            m_targetName.clear();
        }

        m_bluetooth.setTargetDevices(m_targetAddresses);
        saveSettings();
        emit targetAddressesChanged();
        emit targetAddressChanged(m_targetAddress);
        emit targetNameChanged(m_targetName);
        emit selectedDevicesCountChanged();
        appendLog(QString("[%1] Removed car device: %2 (%3)").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), devName, cleanAddr));
    }
}

void AppController::toggleDeviceSelection(const QString &address, const QString &name)
{
    if (isDeviceSelected(address)) {
        removeDevice(address);
    } else {
        addDevice(address, name);
    }
}

void AppController::clearDevices()
{
    m_targetAddresses.clear();
    m_targetAddressesSet.clear();
    m_targetNames.clear();
    m_targetAddress.clear();
    m_targetName.clear();

    m_bluetooth.setTargetDevices(m_targetAddresses);
    saveSettings();
    emit targetAddressesChanged();
    emit targetAddressChanged(m_targetAddress);
    emit targetNameChanged(m_targetName);
    emit selectedDevicesCountChanged();
    appendLog(QString("[%1] Cleared all car devices.").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
}

QString AppController::getDeviceName(const QString &address) const
{
    return m_targetNames.value(address.trimmed().toUpper(), address);
}

QString AppController::connectedTargetName() const
{
    QString name = m_bluetooth.connectedTargetName();
    if (!name.isEmpty())
        return name;
    QString activeAddr = m_bluetooth.connectedTargetAddress();
    if (!activeAddr.isEmpty()) {
        return m_targetNames.value(activeAddr.toUpper(), activeAddr);
    }
    if (!m_targetName.isEmpty())
        return m_targetName;
    return QString();
}

void AppController::selectDevice(const QString &address, const QString &name)
{
    if (m_allowMultipleDevices) {
        toggleDeviceSelection(address, name);
    } else {
        m_targetAddresses.clear();
        m_targetNames.clear();
        addDevice(address, name);
    }
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
            sendNotification(tr("Hotspot blocked"), failReason);
            return;
        }
        ensureCellularConnected();
        triggerFeedback();
    }

    appendLog(QString("[%1] Manual toggle Hotspot: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), active ? "ON" : "OFF"));
    m_hotspot.setHotspotActive(active);
    sendNotification(
        tr("Car Hotspot"), 
        active ? tr("Hotspot manually enabled") : tr("Hotspot manually disabled")
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
    // If background systemd daemon is active or configured to autostart, it handles connection automation.
    // The GUI should NOT duplicate actions, timers, vibrations or notifications.
    if (!m_isDaemon && (m_isDaemonActive || m_autostartService)) {
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString activeName = connectedTargetName();
    if (!activeName.isEmpty()) {
        m_lastConnectedCarLabel = activeName;
    }
    QString carLabel = !m_lastConnectedCarLabel.isEmpty() ? m_lastConnectedCarLabel : (!m_targetName.isEmpty() ? m_targetName : "Car");

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
                sendNotification(tr("Hotspot blocked"), failReason);
                return;
            }

            // Ensure cellular data is active
            ensureCellularConnected();

            appendLog(QString("[%1] Auto-enabling Hotspot...").arg(timestamp));
            m_hotspot.setHotspotActive(true);
            triggerFeedback();
            sendNotification(tr("Car Hotspot"), tr("%1 connected: Wi-Fi Hotspot turned ON").arg(carLabel));
        } else {
            sendNotification(tr("Car Hotspot"), tr("%1 connected").arg(carLabel));
        }
    } else {
        // Bluetooth reported disconnected:
        if (m_autoToggle && m_hotspot.isHotspotActive()) {
            // Debounce for 5 seconds to filter out brief BlueZ/handshake drops
            appendLog(QString("[%1] %2 link dropped. Filtering brief drop (5s)...").arg(timestamp, carLabel));
            m_disconnectDebounceTimer->start(5000);
        } else {
            appendLog(QString("[%1] %2 disconnected.").arg(timestamp, carLabel));
            sendNotification(tr("Car Hotspot"), tr("%1 disconnected").arg(carLabel));
        }
    }
}

void AppController::onDisconnectDebounceTimeout()
{
    if (!m_isDaemon && (m_isDaemonActive || m_autostartService)) {
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString carLabel = !m_lastConnectedCarLabel.isEmpty() ? m_lastConnectedCarLabel : (!m_targetName.isEmpty() ? m_targetName : "Car");

    // Safety guard: if car reconnected during debounce, abort stop sequence
    if (m_bluetooth.isTargetConnected()) {
        appendLog(QString("[%1] %2 is currently connected. Disconnect sequence cancelled.").arg(timestamp, carLabel));
        return;
    }

    appendLog(QString("[%1] %2 disconnect confirmed.").arg(timestamp, carLabel));

    if (m_autoToggle && m_hotspot.isHotspotActive()) {
        if (m_stopDelayMinutes > 0) {
            appendLog(QString("[%1] Starting turn-off grace timer (%2 min)...").arg(timestamp).arg(m_stopDelayMinutes));
            sendNotification(tr("Car Hotspot"), tr("%1 disconnected. Hotspot will turn OFF in %2 min").arg(carLabel).arg(m_stopDelayMinutes));
            m_delayedStopTimer->start(m_stopDelayMinutes * 60 * 1000);
        } else {
            appendLog(QString("[%1] Auto-disabling Hotspot immediately...").arg(timestamp));
            m_hotspot.setHotspotActive(false);
            sendNotification(tr("Car Hotspot"), tr("%1 disconnected: Wi-Fi Hotspot turned OFF").arg(carLabel));
        }
    }
}

void AppController::onDelayedStopTimeout()
{
    if (!m_isDaemon && (m_isDaemonActive || m_autostartService)) {
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString carLabel = !m_lastConnectedCarLabel.isEmpty() ? m_lastConnectedCarLabel : (!m_targetName.isEmpty() ? m_targetName : "Car");

    // Safety guard: if car reconnected during grace period, abort stop sequence
    if (m_bluetooth.isTargetConnected()) {
        appendLog(QString("[%1] %2 is currently connected. Grace stop cancelled.").arg(timestamp, carLabel));
        return;
    }

    appendLog(QString("[%1] Grace period (%2 min) expired. Auto-disabling Hotspot.").arg(timestamp).arg(m_stopDelayMinutes));
    m_hotspot.setHotspotActive(false);
    sendNotification(tr("Car Hotspot"), tr("Grace timer expired: Wi-Fi Hotspot turned OFF"));
}

void AppController::onBatteryChanged(int percentage, bool charging)
{
    if (!m_isDaemon && (m_isDaemonActive || m_autostartService)) {
        return;
    }

    // If Hotspot is running, not charging, and battery dropped below limit -> auto-stop to protect phone
    if (m_hotspot.isHotspotActive() && !charging && percentage > 0 && percentage <= m_minBatteryLevel) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        appendLog(QString("[%1] Battery critical (%2%)! Auto-stopping Hotspot.").arg(timestamp).arg(percentage));
        m_hotspot.setHotspotActive(false);
        sendNotification(tr("Hotspot turned OFF"), tr("Battery dropped to %1%. Hotspot stopped to prevent battery drain.").arg(percentage));
    }
}

void AppController::onRoamingChanged(bool roaming)
{
    if (!m_isDaemon && (m_isDaemonActive || m_autostartService)) {
        return;
    }

    // If roaming detected while Hotspot is running and block option is on -> immediate shutdown
    if (roaming && m_blockInRoaming && m_hotspot.isHotspotActive()) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        appendLog(QString("[%1] Roaming detected! Auto-stopping Hotspot to prevent high costs.").arg(timestamp));
        m_hotspot.setHotspotActive(false);
        sendNotification(tr("Hotspot turned OFF"), tr("Roaming detected! Wi-Fi Hotspot turned off to avoid extra carrier charges."));
    }
}

void AppController::onDevicesChanged()
{
    bool updated = false;
    const auto devs = m_bluetooth.devices();
    for (const QVariant &v : devs) {
        QVariantMap m = v.toMap();
        QString addr = m.value("address").toString().toUpper();
        if (isDeviceSelected(addr)) {
            QString currentName = m.value("name").toString();
            if (!currentName.isEmpty() && m_targetNames.value(addr) != currentName) {
                m_targetNames[addr] = currentName;
                updated = true;
                if (m_targetAddress.compare(addr, Qt::CaseInsensitive) == 0) {
                    m_targetName = currentName;
                    emit targetNameChanged(m_targetName);
                }
            }
        }
    }
    if (updated) {
        saveSettings();
        emit targetAddressesChanged();
    }
}
