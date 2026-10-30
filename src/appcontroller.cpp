#include "appcontroller.h"
#include "timeddbus.h"
#include <QDateTime>
#include <QList>
#include <QVariant>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QTimer>
#include <QEvent>
#include <QGuiApplication>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusVariant>
#include <QtDBus/QDBusObjectPath>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QDebug>

AppController::AppController(bool isDaemon, QObject *parent)
    : QObject(parent)
    , m_isDaemon(isDaemon)
    , m_settings(carhotspotSettingsFilePath(), QSettings::IniFormat)
{
    TimedDbus::registerTypes();

    // Migrate settings of the very old "hotspot in car" builds (same INI syntax).
    QSettings oldSettings(QDir::homePath() + "/.config/harbour-hotspotincar/harbour-hotspotincar.conf",
                          QSettings::IniFormat);
    if (m_settings.allKeys().isEmpty() && !oldSettings.allKeys().isEmpty()) {
        for (const QString &key : oldSettings.allKeys()) {
            m_settings.setValue(key, oldSettings.value(key));
        }
        m_settings.sync();
    }

    m_ipc = new CarHotspotDBusAdapter(m_isDaemon, this);
    // lipstick calls openApp()/activate() on the exported object; the adapter emits
    // requestActivate, which raises the window in this (UI) process.
    connect(m_ipc, &CarHotspotDBusAdapter::requestActivate,
            this, &AppController::onActivateRequested);

    // timed sends a `cookie` signal when a scheduled event fires. This is the only
    // sanctioned way to be woken by the RTC out of deep sleep: no command is executed.
    TimedDbus::listenForCookies(this, SLOT(onTimedCookie(uint)));

    m_delayedStopTimer = new QTimer(this);
    m_delayedStopTimer->setSingleShot(true);
    connect(m_delayedStopTimer, &QTimer::timeout, this, &AppController::onDelayedStopTimeout);

    m_disconnectDebounceTimer = new QTimer(this);
    m_disconnectDebounceTimer->setSingleShot(true);
    connect(m_disconnectDebounceTimer, &QTimer::timeout, this, &AppController::onDisconnectDebounceTimeout);

    checkDaemonStatus();

    connect(this, &AppController::targetAddressesChanged,
            this, &AppController::selectedDevicesChanged);
    connect(&m_bluetooth, &BluetoothManager::targetConnectionChanged,
            this, &AppController::onTargetConnectionChanged);
    connect(&m_bluetooth, &BluetoothManager::targetConnectionChanged,
            this, &AppController::connectedTargetNameChanged);
    connect(&m_bluetooth, &BluetoothManager::targetConnectionChanged,
            this, &AppController::selectedDevicesChanged);
    connect(&m_bluetooth, &BluetoothManager::devicesChanged,
            this, &AppController::onDevicesChanged);
    connect(&m_bluetooth, &BluetoothManager::devicesChanged,
            this, &AppController::selectedDevicesChanged);
    // Switching Bluetooth on is a new chance to reach the car: re-arm the paging budget.
    // The first "powered" emission is only the initial state read at startup, not a switch,
    // so it must not be reported as one.
    connect(&m_bluetooth, &BluetoothManager::bluetoothPoweredChanged, this, [this](bool powered) {
        if (powered) {
            if (!m_bluetoothPowerBaseline) {
                m_bluetoothPowerBaseline = true;
                return;
            }
            resetAutoConnectBudget(true, tr("Bluetooth switched on"));
        } else {
            m_bluetoothPowerBaseline = false;
        }
        updatePollInterval();
    });
    connect(&m_systemMonitor, &SystemMonitor::batteryChanged,
            this, &AppController::onBatteryChanged);
    connect(&m_systemMonitor, &SystemMonitor::roamingChanged,
            this, &AppController::onRoamingChanged);
    connect(&m_systemMonitor, &SystemMonitor::displayWokeUp, this, [this]() {
        bool handlingAutomation = m_isDaemon || !m_isDaemonActive;
        if (!handlingAutomation)
            return;
        // A screen wake is not evidence that the car is near - charging, notifications and a
        // glance at the clock all wake the display. Paging on every wake is what made the app
        // page the absent car every 10-30s, so this is an opt-in advanced option (pageOnWake
        // in the settings file, off by default). The state re-read below always runs.
        if (m_pageOnWake) {
            maybeRearmAutoConnect(tr("screen wake"));
        }
        if (m_autoEnableBluetooth && !m_bluetooth.isBluetoothPowered()) {
            m_bluetooth.ensureBluetoothPowered();
        }
        m_bluetooth.refreshDevices();
        m_hotspot.checkStatus();
        m_lastHotspotCheck = QDateTime::currentMSecsSinceEpoch();
        if (m_bluetooth.isBluetoothPowered() && !m_bluetooth.isTargetConnected()) {
            attemptCarConnect();
        } else if (m_autoToggle && m_bluetooth.isTargetConnected() && !m_hotspot.isHotspotActive() && !m_manualOverrideOff) {
            onTargetConnectionChanged(true);
        }
        updatePollInterval();
    });
    connect(&m_hotspot, &HotspotManager::hotspotActiveChanged, this, [this]() {
        updatePollInterval();
    });
    // Keep the exported diagnostic snapshot in step with the real ConnMan state; without
    // this the backgroundStatus() answer stays on the last notification the service sent.
    connect(&m_hotspot, &HotspotManager::statusMessageChanged, this, [this]() {
        if (m_ipc)
            m_ipc->setStatus(m_hotspot.statusMessage());
    });
    connect(&m_hotspot, &HotspotManager::hotspotStartFailed, this, [this](const QString &reason) {
        appendLog(QString("[%1] WARNING: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), reason));
        sendNotification(tr("Hotspot problem"), reason);
    });

    loadSettings();

    // OpenRepos build: make sure the user service really runs when the user asked for
    // it (e.g. the unit was disabled while the setting was on). The Harbour build has no
    // unit at all - monitoring lives with this process - so the switch is only a setting.
    if (!m_isDaemon && isServiceBuild() && m_autostartService && !m_isDaemonActive) {
        controlUserService(true);
    }

    // Automation timers are active in the daemon process, or in the main process when no external daemon is active
    bool isHandlingAutomation = m_isDaemon || !m_isDaemonActive;

    // Proactively ensure Bluetooth / car connection at boot (3s and 10s after service start, once BlueZ is ready)
    if (isHandlingAutomation) {
        if (m_autoEnableBluetooth) {
            QTimer::singleShot(2000, this, [this]() {
                if (m_autoEnableBluetooth && !m_bluetooth.isBluetoothPowered()) {
                    m_bluetooth.ensureBluetoothPowered();
                }
            });
        }
        for (int delayMs : {3000, 10000}) {
            QTimer::singleShot(delayMs, this, [this]() {
                if (m_autoEnableBluetooth && !m_bluetooth.isBluetoothPowered()) {
                    m_bluetooth.ensureBluetoothPowered();
                }
                m_bluetooth.refreshDevices();
                if (m_bluetooth.isBluetoothPowered() && !m_bluetooth.isTargetConnected()) {
                    attemptCarConnect();
                } else if (m_autoToggle && m_bluetooth.isTargetConnected() && !m_hotspot.isHotspotActive() && !m_manualOverrideOff) {
                    onTargetConnectionChanged(true);
                }
            });
        }
    }

    // Check if target car is already connected at startup (only if handling automation)
    if (isHandlingAutomation && m_bluetooth.isTargetConnected() && !m_hotspot.isHotspotActive()) {
        onTargetConnectionChanged(true);
    }

    // Periodic watchdog timer: keeps monitoring Bluetooth and Hotspot even if system D-Bus
    // signals are missed in background/lockscreen. Runs at 3s only while something is in
    // flight, otherwise at 10s - see pollTick()/updatePollInterval().
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &AppController::pollTick);
    m_pollTimer->start(POLL_BUSY_MS);
    updatePollInterval();

    // Track foreground/background so the GUI idles when it is not visible
    if (QGuiApplication::instance()) {
        QGuiApplication::instance()->installEventFilter(this);
    }

    if (m_isDaemon) {
        appendLog(QString("[%1] Background daemon started").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    } else {
        reloadSharedLog();
        // Log text is developer-facing (the diagnostics page shows it as written), so it
        // states the truth of the flavour instead of claiming a service that does not exist.
        appendLog(isServiceBuild()
                  ? QString("[%1] UI connected to service").arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                  : QString("[%1] UI started, monitoring in this process (no background service in this build)")
                        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    }

    // Register the D-Bus surface. An application may own its own
    // <OrganizationName>.<ApplicationName> and org.sailfishos.coveraction.* names; any
    // other name is rejected by Sailjail. Only signals (Base broadcasts
    // org.sailfishos.coveraction.*) cross the process boundary - method calls between the
    // two processes are filtered, so nothing here exposes automation entry points.
    const QString iface = QStringLiteral("org.sailfishos.coveraction.carhotspot");
    const QString uiName = QStringLiteral("org.sailfishos.coveraction.carhotspot_ui");
    const QString serviceName = iface;

    if (m_isDaemon) {
        QDBusConnection::sessionBus().registerService(serviceName);
        QDBusConnection::sessionBus().connect(uiName, "/", iface,
                                             "manualToggleRequested",
                                             this, SLOT(onManualToggleRequested(bool)));
        QDBusConnection::sessionBus().connect(uiName, "/", iface,
                                             "settingsChanged",
                                             this, SLOT(onRemoteSettingsChanged()));
    } else {
        // The application id name is also the notification remote-action target, and
        // desktop file ExecDBus makes D-Bus activation start the app when the
        // notification is tapped while the UI is closed.
        QDBusConnection::sessionBus().registerService(QStringLiteral("harbour-carhotspot.harbour-carhotspot"));
        QDBusConnection::sessionBus().registerService(uiName);
        QDBusConnection::sessionBus().connect(serviceName, "/", iface,
                                            "logChanged",
                                            this, SLOT(onRemoteLog(QString)));
        QDBusConnection::sessionBus().connect(serviceName, "/", iface,
                                            "daemonStateChanged",
                                            this, SLOT(onRemoteDaemonState(bool)));
    }

    QDBusConnection::sessionBus().registerObject("/", m_ipc,
                                                QDBusConnection::ExportAllSlots);
}

void AppController::pollTick()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (m_isDaemon) {
        // The UI emits a `settingsChanged` signal on every change, so this is only a
        // safety net for edits made while the service was asleep. No sync() here: the
        // service flushes its own writes (activity log, Wi-Fi restore flag) explicitly,
        // and a periodic sync could clobber newer values written by the UI.
        if (now - m_lastSettingsReload >= SETTINGS_RELOAD_MS) {
            m_lastSettingsReload = now;
            loadSettings();
        }

        // Manual hotspot requests arrive through the shared settings file. The file lives in
        // the Sailjail-whitelisted configuration directory, so both processes see it, and it
        // works even when the D-Bus broadcast filter drops a signal (deep sleep, restart of
        // the UI process, race with the service start). The signal is only a fast path.
        QSettings pending(carhotspotSettingsFilePath(), QSettings::IniFormat);
        const int request = pending.value(QStringLiteral("manualHotspotRequest"), -1).toInt();
        const qint64 stamp = pending.value(QStringLiteral("manualHotspotRequestStamp"), 0).toLongLong();
        if (request >= 0 && stamp > m_lastManualRequestStamp) {
            m_lastManualRequestStamp = stamp;
            pending.remove(QStringLiteral("manualHotspotRequest"));
            pending.remove(QStringLiteral("manualHotspotRequestStamp"));
            pending.sync();
            applyHotspotToggleLocally(request == 1);
        }
    } else {
        checkDaemonStatus();
        reloadSharedLog();
    }

    // Short-circuits on m_isDaemon: the service is a QCoreApplication, and
    // QGuiApplication::applicationState() requires a QGuiApplication instance.
    const bool uiVisible = !m_isDaemon
                           && QGuiApplication::applicationState() != Qt::ApplicationHidden;

    // Only the process that owns the automation powers Bluetooth up; otherwise the UI
    // and the service would race each other (the power-on cooldown is per process).
    const bool handlingAutomation = m_isDaemon || !m_isDaemonActive;

    if (handlingAutomation && m_autoEnableBluetooth && !m_bluetooth.isBluetoothPowered()) {
        m_bluetooth.ensureBluetoothPowered();
    }

    // Device and connection state arrives through BlueZ signals (PropertiesChanged for
    // org.bluez.Device1.Connected, InterfacesAdded for a device that appears), which is also
    // what wakes this process when the car connects by itself. The periodic full read is
    // therefore only a safety net for a missed signal, a BlueZ restart, and the picker while
    // the page is visible - it used to run on every tick, 3s around the clock.
    if (handlingAutomation || uiVisible) {
        if (now - m_lastBluetoothHeartbeatMs >= BLUETOOTH_HEARTBEAT_MS) {
            m_lastBluetoothHeartbeatMs = now;
            m_bluetooth.refreshDevices();
        }
    }

    // Tethering state arrives through net.connman.Technology.PropertyChanged, so ConnMan is
    // queried on every tick only while the AP is expected to be up (enable watchdog);
    // otherwise once a minute as a safety net and for the Wi-Fi restoration path.
    if (m_hotspot.startPending() || m_hotspot.isHotspotActive()
            || now - m_lastHotspotCheck >= HOTSPOT_CHECK_MS) {
        m_lastHotspotCheck = now;
        m_hotspot.checkStatus();
    }

    if (handlingAutomation && m_autoToggle && !inServiceHandoff()) {
        if (m_bluetooth.isBluetoothPowered() && !m_bluetooth.isTargetConnected()) {
            // Phone-initiated paging lives in attemptCarConnect(): a bounded number of
            // attempts per power cycle, because an absent car must not be paged around the
            // clock - the page occupies the combo controller, which the Android support
            // stack also needs. A car that connects on its own is reported by BlueZ signals,
            // so a real drive is never delayed.
            attemptCarConnect();
        } else if (m_bluetooth.isTargetConnected() && !m_hotspot.isHotspotActive() && !m_manualOverrideOff) {
            bool timersIdle = (!m_disconnectDebounceTimer || !m_disconnectDebounceTimer->isActive()) &&
                              (!m_delayedStopTimer || !m_delayedStopTimer->isActive()) &&
                              (m_wakeupCookie == 0);
            if (timersIdle && (now - m_lastAutoHotspotAttempt >= 15000)) {
                m_lastAutoHotspotAttempt = now;
                onTargetConnectionChanged(true);
            }
        }
    }

    updatePollInterval();
}

void AppController::updatePollInterval()
{
    if (!m_pollTimer)
        return;

    const bool busy = m_bluetooth.isTargetConnected()
                   || m_hotspot.isHotspotActive()
                   || m_hotspot.startPending()
                   || (m_disconnectDebounceTimer && m_disconnectDebounceTimer->isActive())
                   || (m_delayedStopTimer && m_delayedStopTimer->isActive());

    // The GUI polls fast while visible; both processes idle when nothing is happening.
    const bool uiVisible = !m_isDaemon
                           && QGuiApplication::applicationState() != Qt::ApplicationHidden;

    const int wanted = (busy || uiVisible) ? POLL_BUSY_MS : POLL_IDLE_MS;
    if (m_pollTimer->interval() != wanted) {
        m_pollTimer->setInterval(wanted);
    }
}

void AppController::resetAutoConnectBudget(bool withLog, const QString &reason)
{
    m_autoConnectAttemptsLeft = AUTO_CONNECT_ATTEMPTS_PER_CYCLE;
    m_autoConnectBackoffMs = AUTO_CONNECT_BASE_MS;
    m_lastAutoConnectAttempt = 0;
    m_autoConnectBudgetLogged = false;
    m_autoConnectBudgetSpentMs = 0;
    // Only a signal that the car is really present reaches here (Bluetooth power, a real
    // connect/disconnect, a car selection), so the quiet-window ladder starts over.
    m_autoConnectRearmMs = AUTO_CONNECT_REARM_MS;
    m_autoConnectEmptyBursts = 0;

    // Only the process that owns the automation writes to the shared diagnostics log, so a
    // UI save does not duplicate the line the service would write.
    if (withLog && (m_isDaemon || !m_isDaemonActive) && !reason.isEmpty()) {
        appendLog(QString("[%1] Car paging re-armed (%2)")
                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"), reason));
    }

    updatePollInterval();
}

void AppController::maybeRearmAutoConnect(const QString &reason)
{
    // A screen wake is a weak signal: it fires for charging, notifications and a glance at
    // the clock. On this device it woke the budget every few seconds, so the car was paged
    // every 10-30s instead of 3 times per 10 minutes - the opposite of what the budget is
    // for, because a BR/EDR page holds the combo controller that the Android support stack
    // shares. Only re-arm once the previous burst is old enough, and never skip the
    // back-off that is already running.
    if (m_autoConnectAttemptsLeft > 0) {
        return;  // attempts are still available, nothing to re-arm
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_autoConnectBudgetSpentMs && now - m_autoConnectBudgetSpentMs < m_autoConnectRearmMs) {
        return;
    }

    m_autoConnectAttemptsLeft = AUTO_CONNECT_ATTEMPTS_PER_CYCLE;
    m_autoConnectBudgetLogged = false;
    m_autoConnectBudgetSpentMs = 0;
    m_autoConnectBackoffMs = AUTO_CONNECT_BASE_MS;

    if (m_isDaemon || !m_isDaemonActive) {
        appendLog(QString("[%1] Car paging re-armed (%2, %3 min since the last burst)")
                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"),
                       reason.isEmpty() ? tr("quiet timer") : reason)
                  .arg(m_autoConnectRearmMs / 60000));
    }
}

void AppController::attemptCarConnect()
{
    if (!m_autoToggle || inServiceHandoff()) {
        return;
    }
    if (!m_bluetooth.isBluetoothPowered() || m_bluetooth.isTargetConnected()) {
        return;
    }

    // Never add a BR/EDR page while the access point is up or coming up: one combo controller
    // then has LE scanning, the page and the AP competing for the same firmware slots, which
    // is exactly what makes other Bluetooth consumers (Android stacks, GLE watches) fail.
    // The car is only paged again after the hotspot goes down and the grace period expires.
    if (m_hotspot.isHotspotActive() || m_hotspot.startPending()) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool logging = (m_isDaemon || !m_isDaemonActive);
    const QString carLabel = !connectedTargetName().isEmpty() ? connectedTargetName()
                            : (!m_targetName.isEmpty() ? m_targetName : tr("Car"));

    // Is the car even known to BlueZ? A classic device that is idle does not advertise, so
    // "not in the list" means out of range - and then there is nothing to page. Saying so on
    // the Diagnostics page is what lets a user answer "is this app touching my radio?".
    bool targetKnown = false;
    for (const QVariant &item : m_bluetooth.devices()) {
        if (m_targetAddressesSet.contains(item.toMap().value("address").toString().trimmed().toUpper())) {
            targetKnown = true;
            break;
        }
    }
    if (!targetKnown) {
        if (logging && now - m_lastNoTargetLoggedMs >= 120000) {
            m_lastNoTargetLoggedMs = now;
            appendLog(QString("[%1] %2 is not reported by BlueZ - nothing to page, monitoring passively")
                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss"), carLabel));
        }
        return;
    }

    if (m_autoConnectAttemptsLeft <= 0) {
        // Budget spent. The quiet window before the next burst grows with every burst that
        // found no car: 30 min, then 1 h, then 2 h at most. Paging exists only for kits that
        // never initiate - a car that is really near connects on its own and BlueZ signals us,
        // which is what starts the hotspot - so an absent car must not keep costing the
        // combo controller. Someone who never switches Bluetooth off is still covered: the
        // window is a timer, not a power event.
        if (m_autoConnectBudgetSpentMs && now - m_autoConnectBudgetSpentMs < m_autoConnectRearmMs) {
            return;  // monitor passively for now
        }
        m_autoConnectAttemptsLeft = AUTO_CONNECT_ATTEMPTS_PER_CYCLE;
        m_autoConnectBudgetLogged = false;
        m_autoConnectBackoffMs = AUTO_CONNECT_BASE_MS;
        m_lastAutoConnectAttempt = 0;
    }

    if (now - m_lastAutoConnectAttempt < m_autoConnectBackoffMs) {
        return;
    }
    m_lastAutoConnectAttempt = now;
    m_autoConnectBackoffMs = qMin(m_autoConnectBackoffMs * 2,
                                  static_cast<qint64>(AUTO_CONNECT_BACKOFF_MAX_MS));

    if (logging) {
        appendLog(QString("[%1] Paging %2 (attempt %3/%4 this cycle, radio duty ~10s)")
                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"), carLabel)
                  .arg(AUTO_CONNECT_ATTEMPTS_PER_CYCLE - m_autoConnectAttemptsLeft + 1)
                  .arg(AUTO_CONNECT_ATTEMPTS_PER_CYCLE));
    }

    // Consume the budget only for a page that was really sent - a target BlueZ has not
    // reported yet must not burn the attempts of a car that is merely out of range.
    if (m_bluetooth.connectTargetDevices()) {
        m_autoConnectAttemptsLeft--;
        if (m_autoConnectAttemptsLeft == 0) {
            m_autoConnectBudgetSpentMs = QDateTime::currentMSecsSinceEpoch();
            // This burst found no car: widen the quiet window (see the ladder above).
            m_autoConnectEmptyBursts++;
            if (m_autoConnectEmptyBursts <= 1) {
                m_autoConnectRearmMs = AUTO_CONNECT_REARM_MS;
            } else if (m_autoConnectEmptyBursts == 2) {
                m_autoConnectRearmMs = AUTO_CONNECT_REARM_HOUR_MS;
            } else {
                m_autoConnectRearmMs = AUTO_CONNECT_REARM_MAX_MS;
            }
            if (!m_autoConnectBudgetLogged && logging) {
                m_autoConnectBudgetLogged = true;
                appendLog(QString("[%1] Car paging stopped after %2 attempts - waiting for BlueZ"
                                  " to report the car, next manual attempt in %3 min")
                          .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
                          .arg(AUTO_CONNECT_ATTEMPTS_PER_CYCLE)
                          .arg(m_autoConnectRearmMs / 60000));
            }
        }
    }
}

bool AppController::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::ApplicationStateChange) {
        updatePollInterval();
    }
    return QObject::eventFilter(obj, event);
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
    // Advanced option, deliberately without a toggle in the UI: page the car on a screen wake.
    m_pageOnWake = m_settings.value("pageOnWake", false).toBool();
    m_enableCellularAuto = m_settings.value("enableCellularAuto", true).toBool();
    m_vibrateOnConnect = m_settings.value("vibrateOnConnect", true).toBool();
    // A manual "Hotspot OFF" must survive a process restart, otherwise the automation
    // would switch the hotspot back on against the user's explicit choice.
    m_manualOverrideOff = m_settings.value("manualHotspotOverrideOff", false).toBool();

    m_bluetooth.setTargetDevices(m_targetAddresses);
    m_lastSavedTargets = m_targetAddresses;
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
    m_settings.setValue("manualHotspotOverrideOff", m_manualOverrideOff);
    m_settings.sync();

    // Cross-process notification to the service. Method calls are filtered by Sailjail,
    // signals are not; the service also reloads settings periodically as a safety net.
    notifySettingsChanged();

    // Picking or clearing a car means the car is presumably in range: page straight away.
    // Every other edit (toggles, sliders) only re-arms inside the rate limit - a slider drag
    // saves repeatedly and would otherwise refill the paging budget every few seconds.
    if (m_targetAddresses != m_lastSavedTargets) {
        resetAutoConnectBudget(false);
    } else {
        maybeRearmAutoConnect(tr("a setting changed"));
    }
    m_lastSavedTargets = m_targetAddresses;
}

void AppController::notifySettingsChanged()
{
    if (!m_isDaemon && isServiceBuild() && m_isDaemonActive && m_ipc) {
        emit m_ipc->settingsChanged();
    }
}

void AppController::onRemoteSettingsChanged()
{
    // Same reason as onManualToggleRequested: ignore the signal emitted by this process.
    if (!m_isDaemon)
        return;
    m_settings.sync();
    loadSettings();
    reloadSharedLog();
    // Same rule as in saveSettings(): only an actual car selection deserves an immediate
    // page, any other edit stays inside the rate limit.
    if (m_targetAddresses != m_lastSavedTargets) {
        resetAutoConnectBudget(false);
    } else {
        maybeRearmAutoConnect(tr("a setting changed"));
    }
    m_lastSavedTargets = m_targetAddresses;
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
    // Property setter: always *select* the address, never toggle it off
    // (toggling is only meaningful for the explicit multi-device UI action).
    if (address.trimmed().isEmpty())
        return;

    if (m_allowMultipleDevices) {
        if (!isDeviceSelected(address))
            addDevice(address, address);
    } else {
        selectDevice(address, address);
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
    if (m_autostartService == enabled)
        return;

    m_autostartService = enabled;
    saveSettings();

    if (isServiceBuild() && !m_isDaemon) {
        if (controlUserService(enabled)) {
            requestServiceHandoff(4000);
            appendLog(QString("[%1] Background user service %2")
                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss"),
                           enabled ? QStringLiteral("started") : QStringLiteral("stopped")));
            sendNotification(tr("Car Hotspot"),
                             enabled ? tr("Background service is ACTIVE and monitoring Bluetooth")
                                     : tr("Background service STOPPED"));
        } else {
            appendLog(QString("[%1] Background user service could not be %2")
                      .arg(QDateTime::currentDateTime().toString("hh:mm:ss"),
                           enabled ? QStringLiteral("started") : QStringLiteral("stopped")));
            sendNotification(tr("Car Hotspot"),
                             tr("User service failed to start. Install it manually with: "
                                "systemctl --user enable --now harbour-carhotspot.service"));
        }
    } else {
        // Harbour build: there is no user service to install. Point the user at the
        // OpenRepos build instead of silently doing nothing. The wording is identical to
        // the in-app notice, so it needs exactly one translation.
        appendLog(QString("[%1] Background monitoring preference: %2")
                  .arg(QDateTime::currentDateTime().toString("hh:mm:ss"),
                       enabled ? QStringLiteral("on") : QStringLiteral("off")));
        if (enabled) {
            sendNotification(tr("Background automation needs the OpenRepos version"),
                             tr("This version monitors the car while the app is running (also in background). The OpenRepos version adds a user service that starts on boot and works after the app is closed."));
        }
    }

    emit autostartServiceChanged(m_autostartService);
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
    bool active = m_isDaemon;

    if (!active && isServiceBuild()) {
        // The background service owns org.sailfishos.coveraction.carhotspot; that name is
        // ownable from a sandboxed process and querying the bus is allowed by Base.
        QDBusConnectionInterface *busIface = QDBusConnection::sessionBus().interface();
        if (busIface && busIface->isServiceRegistered("org.sailfishos.coveraction.carhotspot")) {
            active = true;
        }
    }

    if (m_isDaemonActive != active) {
        m_isDaemonActive = active;
        emit isDaemonActiveChanged(m_isDaemonActive);
        if (m_isDaemon && m_ipc) {
            emit m_ipc->daemonStateChanged(active);
        }
    }
}

bool AppController::isServiceBuild() const
{
#ifdef CARHOTSPOT_SERVICE_BUILD
    return true;
#else
    return false;
#endif
}

QString AppController::openReposUrl() const
{
    return QStringLiteral("https://openrepos.net/content/psyhlo/car-hotspot");
}

bool AppController::handlesAutomation() const
{
    return m_isDaemon || !m_isDaemonActive;
}

void AppController::requestServiceHandoff(int milliseconds)
{
    m_serviceHandoffUntilMs = QDateTime::currentMSecsSinceEpoch() + milliseconds;
}

bool AppController::inServiceHandoff() const
{
    return QDateTime::currentMSecsSinceEpoch() < m_serviceHandoffUntilMs;
}

// Enable/disable and (re)start the user service through the systemd user manager on the
// session bus. Sailjail grants this through the "User services" permission (AppLaunch).
// Nothing here spawns a process or runs a shell command.
bool AppController::controlUserService(bool enable)
{
    if (!isServiceBuild())
        return false;

    QDBusInterface systemd(
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager",
        QDBusConnection::sessionBus()
    );

    if (!systemd.isValid()) {
        qWarning() << "AppController: systemd user manager is not reachable";
        return false;
    }

    const QString unit = QStringLiteral("harbour-carhotspot.service");

    auto managerCall = [&unit](const QString &method, const QList<QVariant> &args) {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager",
            method
        );
        msg.setArguments(args);
        // Bounded, so a stuck user manager cannot freeze the UI for the default 25 s.
        QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 5000);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning() << "AppController:" << method << "failed:"
                       << reply.errorName() << reply.errorMessage();
            return false;
        }
        return true;
    };

    bool ok;
    if (enable) {
        ok = managerCall("StartUnit",
                         QList<QVariant>() << unit << QStringLiteral("replace"));
        // Persist it across reboots. Failing here is not fatal: the unit is running for
        // this session and the log tells the user why.
        managerCall("EnableUnit", QList<QVariant>() << unit);
    } else {
        managerCall("DisableUnit", QList<QVariant>() << unit);
        ok = managerCall("StopUnit",
                         QList<QVariant>() << unit << QStringLiteral("replace"));
    }

    requestServiceHandoff(2500);
    QTimer::singleShot(1500, this, &AppController::checkDaemonStatus);
    return ok;
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
    hints.insert("x-nemo-preview-summary", summary);
    hints.insert("x-nemo-preview-body", body);
    hints.insert("x-nemo-icon", "harbour-carhotspot");
    hints.insert("x-nemo-item-count", 1);
    hints.insert("x-nemo-priority", 100);
    hints.insert("x-nemo-user-removable", true);
    hints.insert("x-nemo-timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    // "desktop-entry" is what makes tapping the notification activate the application
    // (desktop file ExecDBus + the exported openApp/activate methods). There is no remote
    // action: this Sailfish OS build has no remote-action support, and inventing a hint
    // format only produces a dead action in the Events View.
    hints.insert("desktop-entry", QStringLiteral("harbour-carhotspot"));
    hints.insert("sound-file", QStringLiteral("message-new-email"));

    QStringList actions;

    QList<QVariant> args;
    args << QString("Car Hotspot");
    args << (uint)0;
    args << QString("harbour-carhotspot");
    args << summary;
    args << body;
    args << actions;
    args << hints;
    args << (int)-1; // -1 keeps the notification in the Events View / notification bar after the banner closes

    msg.setArguments(args);
    QDBusConnection::sessionBus().send(msg);
}

void AppController::openApp()
{
    // Notification taps are delivered by lipstick to harbour-carhotspot.harbour-carhotspot,
    // which is owned by the UI process. The background service has no Wayland connection
    // and cannot raise a window; sandbox D-Bus filtering also prevents it from calling the
    // UI, so it only logs the tap.
    qDebug() << "AppController::openApp() called from notification. Daemon:" << m_isDaemon;
    if (m_isDaemon)
        return;

    emit requestActivateWindow();
}

void AppController::onActivateRequested()
{
    if (!m_isDaemon)
        emit requestActivateWindow();
}

void AppController::onRemoteDaemonState(bool active)
{
    if (m_isDaemon)   // the service emits it, the UI consumes it
        return;
    if (m_isDaemonActive != active) {
        m_isDaemonActive = active;
        emit isDaemonActiveChanged(m_isDaemonActive);
        updatePollInterval();
    }
}

void AppController::onRemoteLog(const QString &log)
{
    if (m_isDaemon || log.isEmpty())
        return;
    m_logStatus = log;
    emit logStatusChanged(m_logStatus);
}

void AppController::onTimedCookie(uint cookie)
{
    if (cookie == 0 || m_wakeupCookie == 0 || cookie != m_wakeupCookie)
        return;

    m_wakeupCookie = 0;
    // The RTC woke the device while the Qt timer was frozen by deep sleep. Run the same
    // guarded shutdown the timer would have run.
    onDelayedStopTimeout();
}

QVariantList AppController::selectedDevices() const
{
    QVariantList result;
    const QVariantList btDevices = m_bluetooth.devices();

    for (const QString &rawAddr : m_targetAddresses) {
        QString addr = rawAddr.trimmed().toUpper();
        if (addr.isEmpty())
            continue;

        QString name = m_targetNames.value(addr, addr);
        bool connected = false;
        bool paired = false;

        for (const QVariant &v : btDevices) {
            QVariantMap m = v.toMap();
            if (m.value("address").toString().trimmed().toUpper() == addr) {
                connected = m.value("connected", false).toBool();
                paired = m.value("paired", false).toBool();
                QString liveName = m.value("name").toString().trimmed();
                if (!liveName.isEmpty()) {
                    name = liveName;
                }
                break;
            }
        }

        QVariantMap item;
        item["address"] = addr;
        item["name"] = name;
        item["connected"] = connected;
        item["paired"] = paired;
        result.append(item);
    }

    return result;
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
    // OpenRepos build with a running service: hand the request over as a signal. A method
    // call would be refused by the sandbox filter, and a blocking D-Bus call could freeze
    // the UI for up to the 25 s bus timeout.
    if (!m_isDaemon && isServiceBuild() && m_isDaemonActive && m_ipc) {
        // Write the request into the shared settings file first (authoritative channel),
        // then notify through a signal so the service reacts immediately instead of on the
        // next poll. A method call would need the sandbox filter to allow it and a blocking
        // call could freeze the UI for the whole bus timeout.
        QSettings pending(carhotspotSettingsFilePath(), QSettings::IniFormat);
        pending.setValue(QStringLiteral("manualHotspotRequest"), active ? 1 : 0);
        pending.setValue(QStringLiteral("manualHotspotRequestStamp"),
                         QDateTime::currentMSecsSinceEpoch());
        pending.sync();
        emit m_ipc->manualToggleRequested(active);
        QTimer::singleShot(1500, this, &AppController::reloadSharedLog);
        return;
    }

    applyHotspotToggleLocally(active);
}

void AppController::onManualToggleRequested(bool active)
{
    // Qt also invokes locally connected slots, so the UI must ignore the signal it emits
    // itself; only the background service acts on it.
    if (!m_isDaemon)
        return;
    applyHotspotToggleLocally(active);
}

bool AppController::applyHotspotToggleLocally(bool active)
{
    // If manual toggle occurs, stop any running grace or debounce timer
    if (m_disconnectDebounceTimer && m_disconnectDebounceTimer->isActive()) {
        m_disconnectDebounceTimer->stop();
    }
    if (m_delayedStopTimer && m_delayedStopTimer->isActive()) {
        m_delayedStopTimer->stop();
    }
    if (m_wakeupCookie > 0) {
        TimedDbus::cancelCookie(m_wakeupCookie);
        m_wakeupCookie = 0;
    }

    m_manualOverrideOff = !active;

    if (active) {
        QString failReason;
        if (!checkSafetyConditions(failReason)) {
            appendLog(QString("[%1] Warning: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), failReason));
            sendNotification(tr("Hotspot blocked"), failReason);
            saveSettings();
            return false;
        }
        ensureCellularConnected();
        triggerFeedback();
    }

    appendLog(QString("[%1] Manual toggle Hotspot: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), active ? "ON" : "OFF"));
    m_hotspot.setHotspotActive(active);
    saveSettings();
    sendNotification(
        tr("Car Hotspot"), 
        active ? tr("Hotspot manually enabled") : tr("Hotspot manually disabled")
    );
    return true;
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

    if (m_ipc) {
        m_ipc->setStatus(m_hotspot.statusMessage());
        m_ipc->setLog(m_logStatus);
        // Only the service broadcasts the log; the UI reads it from the settings file and
        // from this signal, and broadcasting from the UI would only echo to itself.
        if (m_isDaemon) {
            emit m_ipc->logChanged(m_logStatus);
        }
    }
}

QString AppController::firstEntryTimestamp(const QString &log)
{
    // Log lines start with "[hh:mm:ss]"; compare them so that two processes writing the
    // same shared log never overwrite each other's newer entries.
    int start = log.indexOf('[');
    int end = log.indexOf(']');
    if (start < 0 || end <= start + 1)
        return QString();
    return log.mid(start + 1, end - start - 1);
}

void AppController::reloadSharedLog()
{
    m_settings.sync();
    QString diskLog = m_settings.value("activityLog", "").toString();
    if (diskLog.isEmpty() || m_logStatus == diskLog)
        return;

    // Keep the local copy when its newest entry is not older than the one on disk.
    QString local = firstEntryTimestamp(m_logStatus);
    QString disk = firstEntryTimestamp(diskLog);
    if (!local.isEmpty() && !disk.isEmpty() && local >= disk)
        return;

    m_logStatus = diskLog;
    if (m_ipc) {
        m_ipc->setLog(m_logStatus);
    }
    emit logStatusChanged(m_logStatus);
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

    // Sailfish OS plays haptic/sound feedback through NGF (Non-Graphical Feedback).
    // On Sailfish OS 5.x ngf-runner provides it on the SYSTEM bus (verified on a
    // Jolla Phone running 5.2.0.20); older releases expose it on the session bus.
    // Writing the vibrator sysfs nodes (/sys/class/leds/vibrator/*,
    // /sys/class/timed_output/vibrator/*) is not possible from a Sailjail-sandboxed
    // Harbour app, so NGF is the only working path.
    auto playOn = [](const QDBusConnection &bus) -> bool {
        if (!bus.isConnected())
            return false;

        QDBusConnectionInterface *iface = bus.interface();
        if (!iface || !iface->isServiceRegistered("com.nokia.NonGraphicFeedback1.Backend"))
            return false;

        QDBusInterface ngf("com.nokia.NonGraphicFeedback1.Backend",
                           "/",
                           "com.nokia.NonGraphicFeedback1.Backend",
                           bus);
        if (!ngf.isValid())
            return false;

        ngf.asyncCall("Play", QString("vibra"), QVariantMap());
        return true;
    };

    if (!playOn(QDBusConnection::systemBus()))
        playOn(QDBusConnection::sessionBus());
}

void AppController::ensureCellularConnected()
{
    if (!m_enableCellularAuto)
        return;

    // 1. Request cellular connection via Connectiond (Sailfish OS session service running as privileged)
    QDBusMessage connMsg = QDBusMessage::createMethodCall(
        "com.jolla.Connectiond",
        "/Connectiond",
        "com.jolla.Connectiond",
        "connectToType"
    );
    connMsg << QStringLiteral("cellular");
    QDBusConnection::sessionBus().asyncCall(connMsg);

    // 2. Enable cellular technology via ConnMan
    QDBusInterface cellTech(
        "net.connman",
        "/net/connman/technology/cellular",
        "net.connman.Technology",
        QDBusConnection::systemBus()
    );

    if (cellTech.isValid()) {
        cellTech.asyncCall("SetProperty", QString("Powered"), QVariant::fromValue(QDBusVariant(true)));
    }
}

void AppController::onTargetConnectionChanged(bool connected)
{
    // If background daemon is currently active, it handles connection automation.
    // When no separate daemon is active (e.g. Harbour / Sailjail mode), this process handles automation.
    if (!m_isDaemon && m_isDaemonActive) {
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString activeName = connectedTargetName();
    // A real connect/disconnect event proves the car is in range: stop backing off, and
    // let the watchdog run fast while the (dis)connection sequence is being handled.
    resetAutoConnectBudget(false);
    updatePollInterval();
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
        if ((m_delayedStopTimer && m_delayedStopTimer->isActive()) || m_wakeupCookie > 0) {
            if (m_delayedStopTimer) m_delayedStopTimer->stop();
            if (m_wakeupCookie > 0) {
                TimedDbus::cancelCookie(m_wakeupCookie);
                m_wakeupCookie = 0;
            }
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
        // Always debounce brief BlueZ/handshake drops (7s) before notifying or tearing down!
        appendLog(QString("[%1] %2 link dropped. Filtering brief drop (7s)...").arg(timestamp, carLabel));
        m_disconnectDebounceTimer->start(7000);
        updatePollInterval();
    }
}

void AppController::restartBluetooth()
{
    appendLog(QString("[%1] Deep Bluetooth subsystem restart triggered...").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    m_bluetooth.restartBluetoothSubsystem();
}

void AppController::onDisconnectDebounceTimeout()
{
    if (!m_isDaemon && m_isDaemonActive) {
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString carLabel = !m_lastConnectedCarLabel.isEmpty() ? m_lastConnectedCarLabel : (!m_targetName.isEmpty() ? m_targetName : "Car");

    // Safety guard: if car reconnected during debounce, abort stop sequence
    if (m_bluetooth.isTargetConnected()) {
        appendLog(QString("[%1] %2 is currently connected. Disconnect sequence cancelled.").arg(timestamp, carLabel));
        return;
    }

    m_manualOverrideOff = false;
    appendLog(QString("[%1] %2 disconnect confirmed.").arg(timestamp, carLabel));

    if (m_autoToggle && m_hotspot.isHotspotActive()) {
        if (m_stopDelayMinutes > 0) {
            appendLog(QString("[%1] Starting turn-off grace timer (%2 min)...").arg(timestamp).arg(m_stopDelayMinutes));
            sendNotification(tr("Car Hotspot"), tr("%1 disconnected. Hotspot will turn OFF in %2 min").arg(carLabel).arg(m_stopDelayMinutes));
            m_delayedStopTimer->start(m_stopDelayMinutes * 60 * 1000);
            updatePollInterval();
            if (m_wakeupCookie > 0) {
                TimedDbus::cancelCookie(m_wakeupCookie);
            }
            m_wakeupCookie = TimedDbus::scheduleWakeup(m_stopDelayMinutes * 60);
        } else {
            appendLog(QString("[%1] Auto-disabling Hotspot immediately...").arg(timestamp));
            m_hotspot.setHotspotActive(false);
            sendNotification(tr("Car Hotspot"), tr("%1 disconnected: Wi-Fi Hotspot turned OFF").arg(carLabel));
        }
    } else {
        // Hotspot was not active, but car genuinely disconnected after 7s debounce
        sendNotification(tr("Car Hotspot"), tr("%1 disconnected").arg(carLabel));
    }
}

void AppController::onDelayedStopTimeout()
{
    if (!m_isDaemon && m_isDaemonActive) {
        return;
    }

    if (m_wakeupCookie > 0) {
        TimedDbus::cancelCookie(m_wakeupCookie);
        m_wakeupCookie = 0;
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
    if (!m_isDaemon && m_isDaemonActive) {
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
    if (!m_isDaemon && m_isDaemonActive) {
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
            // BlueZ reports Alias/Name, and BluetoothManager falls back to the address
            // itself when the adapter knows no friendly name. Adopting that fallback would
            // replace a user label like "Car Bluetooth" with the MAC address and persist it.
            if (!currentName.isEmpty() && currentName.compare(addr, Qt::CaseInsensitive) != 0
                    && m_targetNames.value(addr) != currentName) {
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
