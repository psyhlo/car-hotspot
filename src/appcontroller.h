#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QSettings>

#include "carhotspotsettings.h"
#include <QTimer>
#include <QSet>
#include "bluetoothmanager.h"
#include "dbusadapter.h"
#include "hotspotmanager.h"
#include "systemmonitor.h"

class QEvent;

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(BluetoothManager* bluetooth READ bluetooth CONSTANT)
    Q_PROPERTY(HotspotManager* hotspot READ hotspot CONSTANT)
    Q_PROPERTY(SystemMonitor* systemMonitor READ systemMonitor CONSTANT)
    // True in the OpenRepos build that ships the background user service
    // (qmake CONFIG+=service). False in the Jolla Harbour build, where monitoring
    // lives with the application process itself.
    Q_PROPERTY(bool serviceBuild READ isServiceBuild CONSTANT)
    Q_PROPERTY(QString openReposUrl READ openReposUrl CONSTANT)
    Q_PROPERTY(bool allowMultipleDevices READ allowMultipleDevices WRITE setAllowMultipleDevices NOTIFY allowMultipleDevicesChanged)
    Q_PROPERTY(QStringList targetAddresses READ targetAddresses NOTIFY targetAddressesChanged)
    Q_PROPERTY(QVariantList selectedDevices READ selectedDevices NOTIFY selectedDevicesChanged)
    Q_PROPERTY(int selectedDevicesCount READ selectedDevicesCount NOTIFY selectedDevicesCountChanged)
    Q_PROPERTY(QString connectedTargetName READ connectedTargetName NOTIFY connectedTargetNameChanged)
    Q_PROPERTY(QString targetAddress READ targetAddress WRITE setTargetAddress NOTIFY targetAddressChanged)
    Q_PROPERTY(QString targetName READ targetName NOTIFY targetNameChanged)
    Q_PROPERTY(bool autoToggle READ autoToggle WRITE setAutoToggle NOTIFY autoToggleChanged)
    Q_PROPERTY(bool autostartService READ autostartService WRITE setAutostartService NOTIFY autostartServiceChanged)
    Q_PROPERTY(bool isDaemonActive READ isDaemonActive NOTIFY isDaemonActiveChanged)
    Q_PROPERTY(bool showNotifications READ showNotifications WRITE setShowNotifications NOTIFY showNotificationsChanged)
    Q_PROPERTY(bool blockInRoaming READ blockInRoaming WRITE setBlockInRoaming NOTIFY blockInRoamingChanged)
    Q_PROPERTY(int minBatteryLevel READ minBatteryLevel WRITE setMinBatteryLevel NOTIFY minBatteryLevelChanged)
    Q_PROPERTY(int stopDelayMinutes READ stopDelayMinutes WRITE setStopDelayMinutes NOTIFY stopDelayMinutesChanged)
    Q_PROPERTY(bool autoEnableBluetooth READ autoEnableBluetooth WRITE setAutoEnableBluetooth NOTIFY autoEnableBluetoothChanged)
    Q_PROPERTY(bool enableCellularAuto READ enableCellularAuto WRITE setEnableCellularAuto NOTIFY enableCellularAutoChanged)
    Q_PROPERTY(bool vibrateOnConnect READ vibrateOnConnect WRITE setVibrateOnConnect NOTIFY vibrateOnConnectChanged)
    Q_PROPERTY(QString logStatus READ logStatus NOTIFY logStatusChanged)

public:
    explicit AppController(bool isDaemon = false, QObject *parent = nullptr);

    BluetoothManager* bluetooth() { return &m_bluetooth; }
    HotspotManager* hotspot() { return &m_hotspot; }
    SystemMonitor* systemMonitor() { return &m_systemMonitor; }

    // Q_PROPERTY accessors are kept non-static: moc only guarantees
    // non-static READ members.
    bool isServiceBuild() const;
    QString openReposUrl() const;

    // True when this process drives the automation: the background service, or the
    // application process while no service is running.
    Q_INVOKABLE bool handlesAutomation() const;

    bool allowMultipleDevices() const { return m_allowMultipleDevices; }
    void setAllowMultipleDevices(bool enabled);

    QStringList targetAddresses() const { return m_targetAddresses; }
    QVariantList selectedDevices() const;
    int selectedDevicesCount() const { return m_targetAddresses.size(); }
    QString connectedTargetName() const;

    Q_INVOKABLE bool isDeviceSelected(const QString &address) const;
    Q_INVOKABLE void toggleDeviceSelection(const QString &address, const QString &name);
    Q_INVOKABLE void addDevice(const QString &address, const QString &name);
    Q_INVOKABLE void removeDevice(const QString &address);
    Q_INVOKABLE void clearDevices();
    Q_INVOKABLE QString getDeviceName(const QString &address) const;

    QString targetAddress() const { return m_targetAddress; }
    void setTargetAddress(const QString &address);

    QString targetName() const { return m_targetName; }
    bool autoToggle() const { return m_autoToggle; }
    void setAutoToggle(bool enabled);

    bool autostartService() const { return m_autostartService; }
    void setAutostartService(bool enabled);

    bool isDaemonActive() const { return m_isDaemonActive; }

    bool showNotifications() const { return m_showNotifications; }
    void setShowNotifications(bool enabled);

    bool blockInRoaming() const { return m_blockInRoaming; }
    void setBlockInRoaming(bool enabled);

    int minBatteryLevel() const { return m_minBatteryLevel; }
    void setMinBatteryLevel(int level);

    int stopDelayMinutes() const { return m_stopDelayMinutes; }
    void setStopDelayMinutes(int minutes);

    bool autoEnableBluetooth() const { return m_autoEnableBluetooth; }
    void setAutoEnableBluetooth(bool enabled);

    bool enableCellularAuto() const { return m_enableCellularAuto; }
    void setEnableCellularAuto(bool enabled);

    bool vibrateOnConnect() const { return m_vibrateOnConnect; }
    void setVibrateOnConnect(bool enabled);

    QString logStatus() const { return m_logStatus; }

public slots:
    void selectDevice(const QString &address, const QString &name);
    // Routed: applied locally when this process owns the automation, otherwise
    // forwarded to the background service as a D-Bus signal. Sailjail's Base policy
    // broadcasts between org.sailfishos.coveraction.* names but does not allow method
    // calls between them, so signals are the only sanctioned cross-process channel.
    void toggleHotspotManual(bool active);
    void appendLog(const QString &text);
    void reloadSharedLog();
    void sendNotification(const QString &summary, const QString &body);
    void triggerFeedback();
    void ensureCellularConnected();
    void checkDaemonStatus();
    void openApp();
    void restartBluetooth();

signals:
    void requestActivateWindow();
    void allowMultipleDevicesChanged(bool enabled);
    void targetAddressesChanged();
    void selectedDevicesChanged();
    void selectedDevicesCountChanged();
    void connectedTargetNameChanged();
    void targetAddressChanged(const QString &address);
    void targetNameChanged(const QString &name);
    void autoToggleChanged(bool enabled);
    void autostartServiceChanged(bool enabled);
    void isDaemonActiveChanged(bool active);
    void showNotificationsChanged(bool enabled);
    void blockInRoamingChanged(bool enabled);
    void minBatteryLevelChanged(int level);
    void stopDelayMinutesChanged(int minutes);
    void autoEnableBluetoothChanged(bool enabled);
    void enableCellularAutoChanged(bool enabled);
    void vibrateOnConnectChanged(bool enabled);
    void logStatusChanged(const QString &log);

private slots:
    void onTargetConnectionChanged(bool connected);
    void onDevicesChanged();
    void onBatteryChanged(int percentage, bool charging);
    void onRoamingChanged(bool roaming);
    void onDelayedStopTimeout();
    void onDisconnectDebounceTimeout();
    void onActivateRequested();
    void onManualToggleRequested(bool active);
    void onRemoteSettingsChanged();
    void onRemoteLog(const QString &log);
    void onRemoteDaemonState(bool active);
    // timed cookie callback: the grace period elapsed while the device was asleep.
    void onTimedCookie(uint cookie);

private:
    void loadSettings();
    void saveSettings();
    void notifySettingsChanged();
    // Start/enable and stop/disable the user service through
    // org.freedesktop.systemd1 (Sailjail permission "User services" = AppLaunch).
    // No shell command, no subprocess - that would be a sandbox escape.
    bool controlUserService(bool enable);
    void requestServiceHandoff(int milliseconds);
    bool inServiceHandoff() const;
    bool applyHotspotToggleLocally(bool active);
    static QString firstEntryTimestamp(const QString &log);
    bool checkSafetyConditions(QString &reason);
    void pollTick();
    void updatePollInterval();
    // Phone-initiated paging, in one place: a bounded number of attempts per cycle, never
    // while the access point is up or coming up. A cycle ends on a Bluetooth power change, a
    // display wake, a real connect/disconnect or a change made in the app, and also on a
    // 10-minute timer so that a radio which is never switched off still gets the car.
    void attemptCarConnect();
    void resetAutoConnectBudget(bool withLog, const QString &reason = QString());
    // Rate-limited re-arm for low-confidence triggers (a screen wake, an unrelated setting
    // change). Never skips the current back-off, so frequent triggers cannot become
    // frequent paging.
    void maybeRearmAutoConnect(const QString &reason = QString());
    bool eventFilter(QObject *obj, QEvent *event) override;

    // Watchdog cadence: fast only while something is actually in flight, so an idle
    // daemon wakes the CPU three times less often (D-Bus calls + flash reads otherwise
    // run every 3s around the clock).
    static const int POLL_BUSY_MS = 3000;
    static const int POLL_IDLE_MS = 10000;
    static const int SETTINGS_RELOAD_MS = 30000;
    static const int HOTSPOT_CHECK_MS = 60000;
    static const int AUTO_CONNECT_BASE_MS = 15000;
    static const int AUTO_CONNECT_BACKOFF_MAX_MS = 60000;
    // Paging is a last resort, not a monitoring strategy: a car kit that is switched on
    // connects to the phone itself, and BlueZ then reports org.bluez.Device1.Connected over
    // D-Bus, which wakes the service and starts the hotspot. Paging only exists for kits that
    // never initiate, and every page holds the combo controller for ~10s that the Android
    // support stack shares. So a burst is short and the window between bursts grows until the
    // radio is left alone.
    static const int AUTO_CONNECT_ATTEMPTS_PER_CYCLE = 2;
    static const int AUTO_CONNECT_REARM_MS = 1800000;      // 30 min, first quiet window
    static const int AUTO_CONNECT_REARM_HOUR_MS = 3600000; // 1 h, second
    static const int AUTO_CONNECT_REARM_MAX_MS = 7200000;  // 2 h ceiling
    // BlueZ state arrives through PropertiesChanged/InterfacesAdded; the periodic full read is
    // a safety net, not the detection mechanism.
    static const int BLUETOOTH_HEARTBEAT_MS = 60000;

    bool m_isDaemon = false;

    BluetoothManager m_bluetooth;
    HotspotManager m_hotspot;
    SystemMonitor m_systemMonitor;
    CarHotspotDBusAdapter *m_ipc = nullptr;
    QSettings m_settings;
    QTimer *m_delayedStopTimer = nullptr;
    QTimer *m_disconnectDebounceTimer = nullptr;
    QTimer *m_pollTimer = nullptr;
    uint m_wakeupCookie = 0;
    qint64 m_lastFeedbackTime = 0;
    qint64 m_lastAutoConnectAttempt = 0;
    qint64 m_lastAutoHotspotAttempt = 0;
    qint64 m_lastHotspotCheck = 0;
    qint64 m_lastManualRequestStamp = 0;
    qint64 m_lastSettingsReload = 0;
    qint64 m_autoConnectBackoffMs = AUTO_CONNECT_BASE_MS;
    int m_autoConnectAttemptsLeft = AUTO_CONNECT_ATTEMPTS_PER_CYCLE;
    bool m_autoConnectBudgetLogged = false;
    qint64 m_autoConnectBudgetSpentMs = 0;
    qint64 m_lastNoTargetLoggedMs = 0;
    qint64 m_lastBluetoothHeartbeatMs = 0;
    qint64 m_autoConnectRearmMs = AUTO_CONNECT_REARM_MS;
    int m_autoConnectEmptyBursts = 0;
    // Advanced option (settings file, no UI on purpose): page the car when the screen wakes.
    // Off by default because a screen wake is not evidence that the car is near.
    bool m_pageOnWake = false;
    bool m_bluetoothPowerBaseline = false;
    qint64 m_serviceHandoffUntilMs = 0;
    bool m_manualOverrideOff = false;

    bool m_allowMultipleDevices = false;
    QStringList m_targetAddresses;
    QStringList m_lastSavedTargets;
    QSet<QString> m_targetAddressesSet;
    QMap<QString, QString> m_targetNames;
    QString m_lastConnectedCarLabel;

    QString m_targetAddress;
    QString m_targetName;
    bool m_autoToggle = true;
    bool m_autostartService = true;
    bool m_isDaemonActive = false;
    bool m_showNotifications = true;
    bool m_blockInRoaming = true;
    int m_minBatteryLevel = 20;
    int m_stopDelayMinutes = 2;
    bool m_autoEnableBluetooth = false;
    bool m_enableCellularAuto = true;
    bool m_vibrateOnConnect = true;
    QString m_logStatus;
};

#endif // APPCONTROLLER_H

