#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QSettings>
#include <QTimer>
#include "bluetoothmanager.h"
#include "hotspotmanager.h"
#include "systemmonitor.h"

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(BluetoothManager* bluetooth READ bluetooth CONSTANT)
    Q_PROPERTY(HotspotManager* hotspot READ hotspot CONSTANT)
    Q_PROPERTY(SystemMonitor* systemMonitor READ systemMonitor CONSTANT)
    Q_PROPERTY(QString targetAddress READ targetAddress WRITE setTargetAddress NOTIFY targetAddressChanged)
    Q_PROPERTY(QString targetName READ targetName NOTIFY targetNameChanged)
    Q_PROPERTY(bool autoToggle READ autoToggle WRITE setAutoToggle NOTIFY autoToggleChanged)
    Q_PROPERTY(bool autostartService READ autostartService WRITE setAutostartService NOTIFY autostartServiceChanged)
    Q_PROPERTY(bool isDaemonActive READ isDaemonActive NOTIFY isDaemonActiveChanged)
    Q_PROPERTY(bool showNotifications READ showNotifications WRITE setShowNotifications NOTIFY showNotificationsChanged)
    Q_PROPERTY(bool blockInRoaming READ blockInRoaming WRITE setBlockInRoaming NOTIFY blockInRoamingChanged)
    Q_PROPERTY(int minBatteryLevel READ minBatteryLevel WRITE setMinBatteryLevel NOTIFY minBatteryLevelChanged)
    Q_PROPERTY(int stopDelayMinutes READ stopDelayMinutes WRITE setStopDelayMinutes NOTIFY stopDelayMinutesChanged)
    Q_PROPERTY(bool enableCellularAuto READ enableCellularAuto WRITE setEnableCellularAuto NOTIFY enableCellularAutoChanged)
    Q_PROPERTY(bool vibrateOnConnect READ vibrateOnConnect WRITE setVibrateOnConnect NOTIFY vibrateOnConnectChanged)
    Q_PROPERTY(QString logStatus READ logStatus NOTIFY logStatusChanged)

public:
    explicit AppController(bool isDaemon = false, QObject *parent = nullptr);

    BluetoothManager* bluetooth() { return &m_bluetooth; }
    HotspotManager* hotspot() { return &m_hotspot; }
    SystemMonitor* systemMonitor() { return &m_systemMonitor; }

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

    bool enableCellularAuto() const { return m_enableCellularAuto; }
    void setEnableCellularAuto(bool enabled);

    bool vibrateOnConnect() const { return m_vibrateOnConnect; }
    void setVibrateOnConnect(bool enabled);

    QString logStatus() const { return m_logStatus; }

public slots:
    void selectDevice(const QString &address, const QString &name);
    void toggleHotspotManual(bool active);
    void appendLog(const QString &text);
    void reloadSharedLog();
    void sendNotification(const QString &summary, const QString &body);
    void triggerFeedback();
    void ensureCellularConnected();
    void checkDaemonStatus();

signals:
    void targetAddressChanged(const QString &address);
    void targetNameChanged(const QString &name);
    void autoToggleChanged(bool enabled);
    void autostartServiceChanged(bool enabled);
    void isDaemonActiveChanged(bool active);
    void showNotificationsChanged(bool enabled);
    void blockInRoamingChanged(bool enabled);
    void minBatteryLevelChanged(int level);
    void stopDelayMinutesChanged(int minutes);
    void enableCellularAutoChanged(bool enabled);
    void vibrateOnConnectChanged(bool enabled);
    void logStatusChanged(const QString &log);

private slots:
    void onTargetConnectionChanged(bool connected);
    void onDevicesChanged();
    void onBatteryChanged(int percentage, bool charging);
    void onRoamingChanged(bool roaming);
    void onDelayedStopTimeout();

private:
    void loadSettings();
    void saveSettings();
    void syncSystemdService(bool enable);
    bool checkSafetyConditions(QString &reason);

    bool m_isDaemon = false;

    BluetoothManager m_bluetooth;
    HotspotManager m_hotspot;
    SystemMonitor m_systemMonitor;
    QSettings m_settings;
    QTimer *m_delayedStopTimer = nullptr;
    QTimer *m_pollTimer = nullptr;

    QString m_targetAddress;
    QString m_targetName;
    bool m_autoToggle = true;
    bool m_autostartService = true;
    bool m_isDaemonActive = false;
    bool m_showNotifications = true;
    bool m_blockInRoaming = true;
    int m_minBatteryLevel = 20;
    int m_stopDelayMinutes = 2;
    bool m_enableCellularAuto = true;
    bool m_vibrateOnConnect = true;
    QString m_logStatus;
};

#endif // APPCONTROLLER_H

