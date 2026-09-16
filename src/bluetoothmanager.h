#ifndef BLUETOOTHMANAGER_H
#define BLUETOOTHMANAGER_H

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusObjectPath>

class BluetoothManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(bool isTargetConnected READ isTargetConnected NOTIFY targetConnectionChanged)

public:
    explicit BluetoothManager(QObject *parent = nullptr);

    QVariantList devices() const { return m_devices; }
    bool isTargetConnected() const { return m_isTargetConnected; }

public slots:
    void refreshDevices();
    void setTargetDevice(const QString &address);

signals:
    void devicesChanged();
    void targetConnectionChanged(bool connected);
    void deviceConnected(const QString &name, const QString &address);
    void deviceDisconnected(const QString &name, const QString &address);

private slots:
    void onPropertiesChanged(const QString &interface, const QVariantMap &changedProperties, const QStringList &invalidatedProperties);
    void onInterfacesAdded(const QDBusObjectPath &objectPath, const QMap<QString, QVariantMap> &interfacesAndProperties);
    void onInterfacesRemoved(const QDBusObjectPath &objectPath, const QStringList &interfaces);

private:
    void updateTargetStatus();
    QVariantList m_devices;
    QString m_targetAddress;
    bool m_isTargetConnected = false;
};

#endif // BLUETOOTHMANAGER_H
