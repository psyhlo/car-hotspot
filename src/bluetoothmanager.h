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

class BluetoothManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(bool isTargetConnected READ isTargetConnected NOTIFY targetConnectionChanged)
    Q_PROPERTY(QString connectedTargetAddress READ connectedTargetAddress NOTIFY targetConnectionChanged)
    Q_PROPERTY(QString connectedTargetName READ connectedTargetName NOTIFY targetConnectionChanged)
    Q_PROPERTY(bool isBluetoothPowered READ isBluetoothPowered NOTIFY bluetoothPoweredChanged)

public:
    explicit BluetoothManager(QObject *parent = nullptr);

    QVariantList devices() const { return m_devices; }
    bool isTargetConnected() const { return m_isTargetConnected; }
    QString connectedTargetAddress() const { return m_connectedTargetAddress; }
    QString connectedTargetName() const { return m_connectedTargetName; }
    bool isBluetoothPowered();

public slots:
    void refreshDevices();
    void setTargetDevice(const QString &address);
    void setTargetDevices(const QStringList &addresses);
    void ensureBluetoothPowered();
    void connectTargetDevices();

signals:
    void devicesChanged();
    void targetConnectionChanged(bool connected);
    void bluetoothPoweredChanged(bool powered);

private slots:
    void onPropertiesChanged(const QString &interface, const QVariantMap &changedProperties, const QStringList &invalidatedProperties);
    void onInterfacesAdded(const QDBusObjectPath &objectPath, const QMap<QString, QVariantMap> &interfacesAndProperties);
    void onInterfacesRemoved(const QDBusObjectPath &objectPath, const QStringList &interfaces);

private:
    void updateTargetStatus();
    QVariantList m_devices;
    QStringList m_targetAddresses;
    QSet<QString> m_targetAddressesSet;
    bool m_isTargetConnected = false;
    QString m_connectedTargetAddress;
    QString m_connectedTargetName;
    qint64 m_lastPowerOnAttempt = 0;
    int m_reconnectAttemptsLeft = 0;
};

#endif // BLUETOOTHMANAGER_H
