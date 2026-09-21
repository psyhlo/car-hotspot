#include "bluetoothmanager.h"
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusMetaType>
#include <QDebug>

// Type definition for BlueZ GetManagedObjects
typedef QMap<QString, QVariantMap> InterfaceMap;
typedef QMap<QDBusObjectPath, InterfaceMap> ManagedObjectList;
Q_DECLARE_METATYPE(InterfaceMap)
Q_DECLARE_METATYPE(ManagedObjectList)

BluetoothManager::BluetoothManager(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<InterfaceMap>();
    qDBusRegisterMetaType<ManagedObjectList>();

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
    QDBusMessage msg = QDBusMessage::createMethodCall(
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects"
    );

    QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "BluetoothManager: GetManagedObjects call failed:" << reply.errorMessage();
        return;
    }

    if (reply.arguments().isEmpty()) {
        qWarning() << "BluetoothManager: GetManagedObjects returned empty reply";
        return;
    }

    const QDBusArgument &arg = reply.arguments().at(0).value<QDBusArgument>();
    QVariantList list;

    // Parse array of dict of {object_path, dict of {string, dict of {string, variant}}}
    if (arg.currentType() == QDBusArgument::MapType) {
        arg.beginMap();
        while (!arg.atEnd()) {
            arg.beginMapEntry();
            QDBusObjectPath path;
            arg >> path;
            
            QMap<QString, QVariantMap> interfaces;
            arg >> interfaces;
            
            if (interfaces.contains("org.bluez.Device1")) {
                const QVariantMap props = interfaces.value("org.bluez.Device1");
                QVariantMap item;
                item["path"] = path.path();
                item["address"] = props.value("Address").toString();
                item["name"] = props.value("Alias", props.value("Name", props.value("Address"))).toString();
                item["connected"] = props.value("Connected", false).toBool();
                item["paired"] = props.value("Paired", false).toBool();
                list.append(item);
                qDebug() << "Found Bluetooth device:" << item["name"] << item["address"];
            }
            arg.endMapEntry();
        }
        arg.endMap();
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

    if (interface == "org.bluez.Adapter1") {
        if (changedProperties.contains("Powered")) {
            refreshDevices();
        }
        return;
    }

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
