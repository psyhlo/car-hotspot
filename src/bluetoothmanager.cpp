#include "bluetoothmanager.h"
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusMetaType>
#include <QProcess>
#include <QDateTime>
#include <QTimer>
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

bool BluetoothManager::isBluetoothPowered()
{
    // 1. Check BlueZ default adapter first (reflects actual hardware/driver state)
    QDBusInterface adapter(
        "org.bluez",
        "/org/bluez/hci0",
        "org.bluez.Adapter1",
        QDBusConnection::systemBus()
    );
    if (adapter.isValid()) {
        QVariant powered = adapter.property("Powered");
        if (powered.isValid() && powered.toBool()) {
            return true;
        }
    }

    // 2. Check ConnMan technology
    QDBusInterface btTech(
        "net.connman",
        "/net/connman/technology/bluetooth",
        "net.connman.Technology",
        QDBusConnection::systemBus()
    );
    if (btTech.isValid()) {
        QDBusReply<QVariantMap> reply = btTech.call("GetProperties");
        if (reply.isValid() && reply.value().value("Powered", false).toBool()) {
            return true;
        }
    }

    return false;
}

void BluetoothManager::ensureBluetoothPowered()
{
    if (isBluetoothPowered()) {
        qDebug() << "BluetoothManager: Bluetooth is already powered ON";
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastPowerOnAttempt < 15000) {
        qDebug() << "BluetoothManager: Bluetooth power-on cooldown active ("
                 << (now - m_lastPowerOnAttempt) << "ms elapsed), skipping duplicate attempt";
        return;
    }
    m_lastPowerOnAttempt = now;

    qDebug() << "BluetoothManager: Bluetooth is OFF. Powering ON via privileged helper...";

    // Power on asynchronously via helper (unblocks rfkill and enables ConnMan technology)
    // Avoid direct adapter.setProperty("Powered", true) to prevent desynchronization with ConnMan and rfkill.
    QProcess::startDetached("sudo", QStringList() << "/usr/bin/harbour-carhotspot-helper" << "bluetooth-on");
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
    QStringList list;
    if (!address.trimmed().isEmpty()) {
        list << address.trimmed();
    }
    setTargetDevices(list);
}

void BluetoothManager::setTargetDevices(const QStringList &addresses)
{
    QStringList cleanList;
    QSet<QString> cleanSet;
    for (const QString &addr : addresses) {
        QString trimmed = addr.trimmed().toUpper();
        if (!trimmed.isEmpty() && !cleanSet.contains(trimmed)) {
            cleanList << trimmed;
            cleanSet.insert(trimmed);
        }
    }

    if (m_targetAddresses == cleanList)
        return;

    m_targetAddresses = cleanList;
    m_targetAddressesSet = cleanSet;
    updateTargetStatus();
}

void BluetoothManager::updateTargetStatus()
{
    bool foundConnected = false;
    QString activeAddress;
    QString activeName;

    if (!m_targetAddressesSet.isEmpty()) {
        for (const QVariant &item : m_devices) {
            QVariantMap map = item.toMap();
            if (!map.value("connected").toBool())
                continue;

            QString devAddr = map.value("address").toString().trimmed().toUpper();
            if (m_targetAddressesSet.contains(devAddr)) {
                foundConnected = true;
                activeAddress = devAddr;
                activeName = map.value("name").toString();
                break;
            }
        }
    }

    bool statusChanged = (m_isTargetConnected != foundConnected) ||
                         (m_connectedTargetAddress != activeAddress) ||
                         (m_connectedTargetName != activeName);

    m_isTargetConnected = foundConnected;
    m_connectedTargetAddress = activeAddress;
    m_connectedTargetName = activeName;

    if (statusChanged) {
        emit targetConnectionChanged(m_isTargetConnected);
    }
}

void BluetoothManager::connectTargetDevices()
{
    if (!isBluetoothPowered()) {
        qDebug() << "BluetoothManager: Cannot connect to target devices, Bluetooth is OFF";
        return;
    }

    if (m_targetAddressesSet.isEmpty()) {
        return;
    }

    if (m_isTargetConnected) {
        qDebug() << "BluetoothManager: Target device already connected";
        m_reconnectAttemptsLeft = 0;
        return;
    }

    refreshDevices();

    bool attempted = false;
    for (const QVariant &item : m_devices) {
        QVariantMap map = item.toMap();
        QString addr = map.value("address").toString().trimmed().toUpper();
        if (m_targetAddressesSet.contains(addr)) {
            QString path = map.value("path").toString();
            if (!path.isEmpty()) {
                qDebug() << "BluetoothManager: Initiating proactive connection to target car:" << addr << "path:" << path;
                QDBusInterface dev(
                    "org.bluez",
                    path,
                    "org.bluez.Device1",
                    QDBusConnection::systemBus()
                );
                if (dev.isValid()) {
                    dev.asyncCall("Connect");
                    attempted = true;
                }
            }
        }
    }

    if (attempted && m_reconnectAttemptsLeft > 0) {
        m_reconnectAttemptsLeft--;
        if (m_reconnectAttemptsLeft > 0) {
            // Retry once more after 10s if still not connected
            QTimer::singleShot(10000, this, [this]() {
                if (!m_isTargetConnected && isBluetoothPowered()) {
                    connectTargetDevices();
                }
            });
        }
    }
}

void BluetoothManager::onPropertiesChanged(const QString &interface, const QVariantMap &changedProperties, const QStringList &invalidatedProperties)
{
    Q_UNUSED(invalidatedProperties);

    if (interface == "org.bluez.Adapter1") {
        if (changedProperties.contains("Powered")) {
            bool powered = changedProperties.value("Powered").toBool();
            qDebug() << "BluetoothManager: Adapter1 Powered changed to:" << powered;
            emit bluetoothPoweredChanged(powered);
            refreshDevices();
            if (powered) {
                // Adapter transitioned to Powered ON. BlueZ needs ~2.5s to register profiles.
                // Then attempt to proactively connect to paired car device(s).
                m_reconnectAttemptsLeft = 2;
                QTimer::singleShot(2500, this, &BluetoothManager::connectTargetDevices);
            }
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
