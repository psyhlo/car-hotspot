#include "bluetoothmanager.h"
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusVariant>
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

void BluetoothManager::setAdapterPoweredDbus(bool powered)
{
    // 1. Official Sailfish OS session service for Bluetooth power control (permitted by Bluetooth.permission)
    QDBusInterface jollaBt(
        "com.jolla.Bluetooth",
        "/BluetoothSessionControl",
        "com.jolla.Bluetooth",
        QDBusConnection::sessionBus()
    );
    if (jollaBt.isValid()) {
        jollaBt.asyncCall(powered ? QStringLiteral("startSession") : QStringLiteral("endSession"));
    } else {
        // Activate com.jolla.Bluetooth via QDBusMessage if not yet running
        QDBusMessage sessionMsg = QDBusMessage::createMethodCall(
            "com.jolla.Bluetooth",
            "/BluetoothSessionControl",
            "com.jolla.Bluetooth",
            powered ? QStringLiteral("startSession") : QStringLiteral("endSession")
        );
        QDBusConnection::sessionBus().asyncCall(sessionMsg);
    }

    // 2. Set ConnMan Bluetooth technology Powered state
    QDBusInterface btTech(
        "net.connman",
        "/net/connman/technology/bluetooth",
        "net.connman.Technology",
        QDBusConnection::systemBus()
    );
    if (btTech.isValid()) {
        btTech.asyncCall("SetProperty", QString("Powered"), QVariant::fromValue(QDBusVariant(powered)));
    }

    // 3. Set BlueZ Adapter1 Powered property directly via org.freedesktop.DBus.Properties
    QString path = m_adapterPath.isEmpty() ? QStringLiteral("/org/bluez/hci0") : m_adapterPath;
    QDBusMessage propMsg = QDBusMessage::createMethodCall(
        "org.bluez",
        path,
        "org.freedesktop.DBus.Properties",
        "Set"
    );
    propMsg << QStringLiteral("org.bluez.Adapter1")
            << QStringLiteral("Powered")
            << QVariant::fromValue(QDBusVariant(powered));
    QDBusConnection::systemBus().asyncCall(propMsg);
}

bool BluetoothManager::isBluetoothPowered()
{
    QString path = m_adapterPath;
    if (path.isEmpty()) {
        path = "/org/bluez/hci0";
    }

    // 1. Check detected BlueZ adapter first (reflects actual hardware/driver state)
    QDBusInterface adapter(
        "org.bluez",
        path,
        "org.bluez.Adapter1",
        QDBusConnection::systemBus()
    );
    if (adapter.isValid()) {
        QVariant powered = adapter.property("Powered");
        if (powered.isValid()) {
            return powered.toBool();
        }
    }

    // Fallback: if default hci0 failed, try hci1
    if (path == "/org/bluez/hci0") {
        QDBusInterface adapter1(
            "org.bluez",
            "/org/bluez/hci1",
            "org.bluez.Adapter1",
            QDBusConnection::systemBus()
        );
        if (adapter1.isValid()) {
            QVariant powered = adapter1.property("Powered");
            if (powered.isValid()) {
                m_adapterPath = "/org/bluez/hci1";
                return powered.toBool();
            }
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

    qDebug() << "BluetoothManager: Bluetooth is OFF. Powering ON via D-Bus...";
    setAdapterPoweredDbus(true);

    // Re-assert Adapter1 Powered after 900ms in case ConnMan took a moment to initialize the HCI adapter
    QTimer::singleShot(900, this, [this]() {
        if (!isBluetoothPowered()) {
            setAdapterPoweredDbus(true);
        }
    });
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
            
            if (interfaces.contains("org.bluez.Adapter1")) {
                QString foundAdapter = path.path();
                if (m_adapterPath != foundAdapter) {
                    m_adapterPath = foundAdapter;
                    qDebug() << "BluetoothManager: Detected Bluetooth adapter at" << m_adapterPath;
                    emit adapterPathChanged(m_adapterPath);
                }
                const QVariantMap adapterProps = interfaces.value("org.bluez.Adapter1");
                if (adapterProps.contains("Powered")) {
                    bool poweredNow = adapterProps.value("Powered", false).toBool();
                    if (m_lastKnownPowered != poweredNow) {
                        m_lastKnownPowered = poweredNow;
                        emit bluetoothPoweredChanged(poweredNow);
                        if (poweredNow && !m_isTargetConnected) {
                            m_reconnectAttemptsLeft = 2;
                            QTimer::singleShot(2000, this, &BluetoothManager::connectTargetDevices);
                        }
                    }
                }
            }

            if (interfaces.contains("org.bluez.Device1")) {
                const QVariantMap props = interfaces.value("org.bluez.Device1");
                QString addr = props.value("Address").toString().trimmed().toUpper();
                QVariantMap item;
                item["path"] = path.path();
                item["address"] = addr;
                item["name"] = props.value("Alias", props.value("Name", addr)).toString();
                item["connected"] = props.value("Connected", false).toBool();
                item["paired"] = props.value("Paired", false).toBool();
                item["selected"] = m_targetAddressesSet.contains(addr);
                list.append(item);
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

    // Update "selected" property on all cached devices so QML Repeater updates immediately
    for (int i = 0; i < m_devices.size(); ++i) {
        QVariantMap map = m_devices[i].toMap();
        QString addr = map.value("address").toString().trimmed().toUpper();
        map["selected"] = m_targetAddressesSet.contains(addr);
        m_devices[i] = map;
    }
    emit devicesChanged();
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

    bool connChanged = (m_isTargetConnected != foundConnected);
    bool nameChanged = (m_connectedTargetName != activeName || m_connectedTargetAddress != activeAddress);

    m_isTargetConnected = foundConnected;
    m_connectedTargetAddress = activeAddress;
    m_connectedTargetName = activeName;

    if (connChanged || nameChanged) {
        emit targetConnectionChanged(m_isTargetConnected);
    }
}

void BluetoothManager::restartBluetoothSubsystem()
{
    if (m_isRestartingBluetooth) {
        return;
    }

    m_isRestartingBluetooth = true;
    emit restartingBluetoothChanged(true);
    qDebug() << "BluetoothManager: Power-cycling Bluetooth adapter via D-Bus...";

    // Step 1: Power OFF Bluetooth technology and adapter via D-Bus
    setAdapterPoweredDbus(false);

    // Step 2: Power ON after 1500ms
    QTimer::singleShot(1500, this, [this]() {
        setAdapterPoweredDbus(true);
    });

    // Step 3: Refresh devices and complete restart after 3500ms
    QTimer::singleShot(3500, this, [this]() {
        setAdapterPoweredDbus(true);
        refreshDevices();
        emit bluetoothPoweredChanged(isBluetoothPowered());
        m_isRestartingBluetooth = false;
        emit restartingBluetoothChanged(false);
        if (isBluetoothPowered() && !m_isTargetConnected) {
            connectTargetDevices();
        }
    });
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

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastConnectAttempt < 8000) {
        return;
    }

    refreshDevices();
    if (m_isTargetConnected) {
        m_reconnectAttemptsLeft = 0;
        return;
    }

    m_lastConnectAttempt = now;

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
    if (interfacesAndProperties.contains("org.bluez.Adapter1")) {
        m_adapterPath = objectPath.path();
        qDebug() << "BluetoothManager: Adapter added at" << m_adapterPath;
        emit adapterPathChanged(m_adapterPath);
    }
    if (interfacesAndProperties.contains("org.bluez.Device1")) {
        refreshDevices();
    }
}

void BluetoothManager::onInterfacesRemoved(const QDBusObjectPath &objectPath, const QStringList &interfaces)
{
    if (interfaces.contains("org.bluez.Adapter1") && objectPath.path() == m_adapterPath) {
        qDebug() << "BluetoothManager: Adapter removed at" << m_adapterPath;
        m_adapterPath.clear();
        emit adapterPathChanged(m_adapterPath);
    }
    if (interfaces.contains("org.bluez.Device1")) {
        refreshDevices();
    }
}
