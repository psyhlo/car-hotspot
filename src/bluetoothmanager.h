#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>
#include <QSet>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>

class QDBusPendingCallWatcher;

class BluetoothManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(bool isTargetConnected READ isTargetConnected NOTIFY targetConnectionChanged)
    Q_PROPERTY(QString connectedTargetAddress READ connectedTargetAddress NOTIFY targetConnectionChanged)
    Q_PROPERTY(QString connectedTargetName READ connectedTargetName NOTIFY targetConnectionChanged)
    Q_PROPERTY(bool isBluetoothPowered READ isBluetoothPowered NOTIFY bluetoothPoweredChanged)
    Q_PROPERTY(bool isRestartingBluetooth READ isRestartingBluetooth NOTIFY restartingBluetoothChanged)
    Q_PROPERTY(QString adapterPath READ adapterPath NOTIFY adapterPathChanged)

public:
    explicit BluetoothManager(QObject *parent = nullptr);

    QVariantList devices() const { return m_devices; }
    bool isTargetConnected() const { return m_isTargetConnected; }
    QString connectedTargetAddress() const { return m_connectedTargetAddress; }
    QString connectedTargetName() const { return m_connectedTargetName; }
    bool isBluetoothPowered();
    bool isRestartingBluetooth() const { return m_isRestartingBluetooth; }
    QString adapterPath() const { return m_adapterPath; }

public slots:
    void refreshDevices();
    void setTargetDevice(const QString &address);
    void setTargetDevices(const QStringList &addresses);
    void ensureBluetoothPowered();
    // Returns true when a Connect() was actually sent for a target device.
    bool connectTargetDevices();
    void restartBluetoothSubsystem();

signals:
    void devicesChanged();
    void targetConnectionChanged(bool connected);
    void bluetoothPoweredChanged(bool powered);
    void restartingBluetoothChanged(bool restarting);
    void adapterPathChanged(const QString &path);

private slots:
    // PropertiesChanged arrives for every BlueZ object, so the handler needs the object path
    // from the raw message to know which device changed - that is also why this slot takes a
    // QDBusMessage instead of the three separate arguments.
    void onDevicePropertiesChanged(const QDBusMessage &message);
    void onInterfacesAdded(const QDBusObjectPath &objectPath, const QMap<QString, QVariantMap> &interfacesAndProperties);
    void onInterfacesRemoved(const QDBusObjectPath &objectPath, const QStringList &interfaces);
    void onRefreshFinished(QDBusPendingCallWatcher *watcher);

private:
    // One BlueZ read per burst instead of one per signal: a (dis)connection emits dozens
    // of PropertiesChanged/InterfacesAdded events, and a single GetManagedObjects reply
    // already contains the whole object tree. 3 s cap instead of Qt's 25 s default.
    static const int REFRESH_MIN_MS = 1200;
    static const int REFRESH_TIMEOUT_MS = 3000;
    static const int CONNECT_DEBOUNCE_MS = 8000;
    static const int POWER_READ_TIMEOUT_MS = 2000;

    void setAdapterPoweredDbus(bool powered);
    bool applyDeviceProperties(const QString &path, const QVariantMap &changed);
    void parseManagedObjects(const QDBusMessage &reply);
    bool isConnectableDevice(const QVariantMap &device) const;
    void logSkippedLeDevice(const QString &address, const QString &name);
    void updateTargetStatus();
    QVariantList m_devices;
    QStringList m_targetAddresses;
    QSet<QString> m_targetAddressesSet;
    bool m_isTargetConnected = false;
    QString m_connectedTargetAddress;
    QString m_connectedTargetName;
    QString m_adapterPath;
    bool m_isRestartingBluetooth = false;
    bool m_lastKnownPowered = false;
    qint64 m_lastPowerOnAttempt = 0;
    qint64 m_lastConnectAttempt = 0;
    int m_reconnectAttemptsLeft = 0;
    bool m_refreshInFlight = false;
    bool m_refreshQueued = false;
    bool m_refreshDeferred = false;
    qint64 m_lastRefreshMs = 0;
    qint64 m_lastRefreshErrorMs = 0;
    QSet<QString> m_loggedLeSkips;
};

#endif // BLUETOOTHMANAGER_H
