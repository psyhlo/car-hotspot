#include "bluetoothmanager.h"
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusArgument>
#include <QDebug>

BluetoothManager::BluetoothManager(QObject *parent)
    : QObject(parent)
{
    // Connect to BlueZ ObjectManager signals
    QDBusConnection::systemBus().connect(
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        "InterfacesAdded",
        this,
        SLOT(onInterfacesAdded(QDBusObjectPath, QMap<QString, QVariantMap>))
    );

    QDBusConnection::systemBus().connect(
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        "InterfacesRemoved",
        this,
        SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList))
    );

    // Connect to PropertiesChanged for all BlueZ objects
    QDBusConnection::systemBus().connect(
        "org.bluez",
        QString(),
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        this,
        SLOT(onPropertiesChanged(QString, QVariantMap, QStringList))
    );

    refreshDevices();
}

void BluetoothManager::refreshDevices()
{
    QDBusInterface manager(
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        QDBusConnection::systemBus()
    );

    if (!manager.isValid()) {
        qWarning() << "BluetoothManager: Cannot access org.bluez ObjectManager";
        return;
    }

    QDBusReply<QMap<QDBusObjectPath, QMap<QString, QVariantMap>>> reply = manager.call("GetManagedObjects");
    if (!reply.isValid()) {
        qWarning() << "BluetoothManager: GetManagedObjects failed:" << reply.error().message();
        return;
    }

    QVariantList list;
    const auto objects = reply.value();
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        const auto &interfaces = it.value();
        if (interfaces.contains("org.bluez.Device1")) {
            const QVariantMap props = interfaces.value("org.bluez.Device1");
            QVariantMap item;
            item["path"] = it.key().path();
            item["address"] = props.value("Address").toString();
            item["name"] = props.value("Alias", props.value("Name", props.value("Address"))).toString();
            item["connected"] = props.value("Connected", false).toBool();
            item["paired"] = props.value("Paired", false).toBool();
            list.append(item);
        }
    }

    m_devices = list;
    emit devicesChanged();
    updateTargetStatus();
}

void BluetoothManager::setTargetDevice(const QString &address)
{
    if (m_targetAddress == address)
        return;

    m_targetAddress = address;
    updateTargetStatus();
}

void BluetoothManager::updateTargetStatus()
{
    bool foundConnected = false;
    for (const QVariant &item : m_devices) {
        QVariantMap map = item.toMap();
        if (map.value("address").toString().compare(m_targetAddress, Qt::CaseInsensitive) == 0) {
            foundConnected = map.value("connected").toBool();
            break;
        }
    }

    if (m_isTargetConnected != foundConnected) {
        m_isTargetConnected = foundConnected;
        emit targetConnectionChanged(m_isTargetConnected);
    }
}

void BluetoothManager::onPropertiesChanged(const QString &interface, const QVariantMap &changedProperties, const QStringList &invalidatedProperties)
{
    Q_UNUSED(invalidatedProperties);
    if (interface != "org.bluez.Device1")
        return;

    if (changedProperties.contains("Connected")) {
        refreshDevices();
    }
}

void BluetoothManager::onInterfacesAdded(const QDBusObjectPath &objectPath, const QMap<QString, QVariantMap> &interfacesAndProperties)
{
    Q_UNUSED(objectPath);
    if (interfacesAndProperties.contains("org.bluez.Device1")) {
        refreshDevices();
    }
}

void BluetoothManager::onInterfacesRemoved(const QDBusObjectPath &objectPath, const QStringList &interfaces)
{
    Q_UNUSED(objectPath);
    if (interfaces.contains("org.bluez.Device1")) {
        refreshDevices();
    }
}
