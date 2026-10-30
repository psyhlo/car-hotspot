#include "bluetoothmanager.h"
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusVariant>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusPendingCallWatcher>
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
        SLOT(onDevicePropertiesChanged(QDBusMessage))
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
    // A property read is quick, but on a wedged bluetoothd Qt would wait its full 25 s.
    adapter.setTimeout(POWER_READ_TIMEOUT_MS);
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
        adapter1.setTimeout(POWER_READ_TIMEOUT_MS);
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
    btTech.setTimeout(POWER_READ_TIMEOUT_MS);
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

// GetManagedObjects is this app's only read path to BlueZ. It used to be a blocking call
// with Qt's 25 s default timeout, which froze the caller's thread whenever bluetoothd was
// busy or mid-restart, and it ran once per BlueZ signal - a (dis)connection emits dozens.
// Reads are now asynchronous, capped at 3 s and coalesced into one request per burst,
// because a single reply already describes the whole object tree.
void BluetoothManager::refreshDevices()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (m_refreshInFlight) {
        m_refreshQueued = true;
        return;
    }

    if (now - m_lastRefreshMs < REFRESH_MIN_MS) {
        if (!m_refreshDeferred) {
            m_refreshDeferred = true;
            QTimer::singleShot(int(REFRESH_MIN_MS - (now - m_lastRefreshMs)), this, [this]() {
                m_refreshDeferred = false;
                refreshDevices();
            });
        }
        return;
    }

    m_lastRefreshMs = now;
    m_refreshInFlight = true;

    QDBusMessage msg = QDBusMessage::createMethodCall(
        "org.bluez",
        "/",
        "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects"
    );

    QDBusPendingCall call = QDBusConnection::systemBus().asyncCall(msg, REFRESH_TIMEOUT_MS);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &BluetoothManager::onRefreshFinished);
}

void BluetoothManager::onRefreshFinished(QDBusPendingCallWatcher *watcher)
{
    m_refreshInFlight = false;

    if (watcher->isError()) {
        // Timeouts during a bluetoothd restart or a deep-sleep wakeup are normal; report
        // one at most every 10 s so a wedged adapter cannot flood the diagnostics log.
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastRefreshErrorMs >= 10000) {
            m_lastRefreshErrorMs = now;
            qWarning() << "BluetoothManager: GetManagedObjects failed:" << watcher->error().message();
        }
    } else {
        parseManagedObjects(watcher->reply());
    }

    watcher->deleteLater();

    // Something changed while the reply was on the wire - re-read once so the cache cannot
    // stay stale (a device that appeared during the call must still become visible).
    if (m_refreshQueued) {
        m_refreshQueued = false;
        m_lastRefreshMs = 0;
        refreshDevices();
    }
}

void BluetoothManager::parseManagedObjects(const QDBusMessage &reply)
{
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
                item["icon"] = props.value("Icon", QString()).toString();
                QStringList allowedModes;
                const QVariant modesVariant = props.value("AllowedModes");
                if (modesVariant.canConvert<QVariantList>()) {
                    const QVariantList modes = modesVariant.toList();
                    for (const QVariant &mode : modes) {
                        allowedModes << mode.toString();
                    }
                }
                item["allowedModes"] = allowedModes;
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

// A BLE-only peripheral answers LE connections only and serves a single central at a time.
// Paging or connecting to one from this process takes it away from whoever already owns it,
// and on Sailfish that owner is usually the Android support stack (com.android.bluetooth
// drives its own HCI on the same combo controller) - which is precisely how a GLE watch
// becomes undiscoverable from the Android app. Only classic-capable devices may be paged.
bool BluetoothManager::isConnectableDevice(const QVariantMap &device) const
{
    const QStringList modes = device.value("allowedModes").toStringList();
    if (modes.size() == 1 && modes.first().compare(QStringLiteral("le"), Qt::CaseInsensitive) == 0) {
        return false;
    }

    // Deliberately narrow: classes that are never a car kit. BlueZ reports "unknown" for
    // devices it cannot classify, so those stay connectable for compatibility.
    const QString icon = device.value("icon").toString().toLower();
    if (icon == QLatin1String("watch") || icon == QLatin1String("le-audio")
            || icon == QLatin1String("gles")) {
        return false;
    }

    return true;
}

void BluetoothManager::logSkippedLeDevice(const QString &address, const QString &name)
{
    if (m_loggedLeSkips.contains(address)) {
        return;
    }
    m_loggedLeSkips.insert(address);
    qDebug() << "BluetoothManager: Not connecting to" << address << name
             << "- LE-only device, a BLE peripheral accepts one central at a time";
}

bool BluetoothManager::connectTargetDevices()
{
    if (!isBluetoothPowered()) {
        qDebug() << "BluetoothManager: Cannot connect to target devices, Bluetooth is OFF";
        return false;
    }

    if (m_targetAddressesSet.isEmpty()) {
        return false;
    }

    if (m_isTargetConnected) {
        qDebug() << "BluetoothManager: Target device already connected";
        m_reconnectAttemptsLeft = 0;
        return false;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastConnectAttempt < CONNECT_DEBOUNCE_MS) {
        return false;
    }
    m_lastConnectAttempt = now;

    // The device cache is kept current by the ObjectManager signals and the periodic
    // refresh, so no blocking read is needed here any more.
    bool attempted = false;
    for (const QVariant &item : m_devices) {
        QVariantMap map = item.toMap();
        QString addr = map.value("address").toString().trimmed().toUpper();
        if (!m_targetAddressesSet.contains(addr)) {
            continue;
        }
        QString path = map.value("path").toString();
        if (path.isEmpty()) {
            continue;
        }
        if (!isConnectableDevice(map)) {
            logSkippedLeDevice(addr, map.value("name").toString());
            continue;
        }

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

    if (!attempted) {
        // No cache entry for the target yet - the car has not been seen by BlueZ. Request a
        // fresh read so a later tick can act on it instead of paging nothing forever.
        refreshDevices();
        return false;
    }

    if (m_reconnectAttemptsLeft > 0) {
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

    return true;
}

void BluetoothManager::onDevicePropertiesChanged(const QDBusMessage &message)
{
    const QList<QVariant> args = message.arguments();
    if (args.size() < 2) {
        return;
    }
    const QString interface = args.at(0).toString();
    const QVariantMap changedProperties = args.at(1).toMap();
    const QStringList invalidatedProperties =
        args.size() > 2 ? args.at(2).toStringList() : QStringList();

    if (interface == QLatin1String("org.bluez.Adapter1")) {
        if (changedProperties.contains("Powered")) {
            const bool powered = changedProperties.value("Powered").toBool();
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

    if (interface != QLatin1String("org.bluez.Device1")) {
        return;
    }

    // The car connecting on its own shows up here, and this payload already carries the new
    // Connected value, so apply it straight to the cache instead of asking BlueZ for the whole
    // tree again. updateTargetStatus() is what starts the hotspot, so the automation now runs
    // on the signal itself instead of on the next watchdog tick.
    if (!invalidatedProperties.isEmpty()) {
        // Properties the signal only invalidates have to be re-read, not guessed at.
        refreshDevices();
        return;
    }

    if (applyDeviceProperties(message.path(), changedProperties)) {
        updateTargetStatus();
        emit devicesChanged();
        return;
    }

    // A device we have never seen, or a change we cannot map: one coalesced read covers it.
    refreshDevices();
}

// Patch the cached device entry from a PropertiesChanged payload. Returns false when the
// object path is not in the cache, which the caller treats as "read the tree once".
bool BluetoothManager::applyDeviceProperties(const QString &path, const QVariantMap &changed)
{
    if (path.isEmpty() || changed.isEmpty()) {
        return false;
    }

    for (int i = 0; i < m_devices.size(); ++i) {
        QVariantMap map = m_devices.at(i).toMap();
        if (map.value("path").toString() != path) {
            continue;
        }

        if (changed.contains("Connected")) {
            map["connected"] = changed.value("Connected").toBool();
        }
        if (changed.contains("Paired")) {
            map["paired"] = changed.value("Paired").toBool();
        }
        if (changed.contains("Icon")) {
            map["icon"] = changed.value("Icon").toString();
        }
        if (changed.contains("AllowedModes")) {
            QStringList modes;
            const QList<QVariant> raw = changed.value("AllowedModes").toList();
            for (const QVariant &mode : raw) {
                modes << mode.toString();
            }
            map["allowedModes"] = modes;
        }
        // The UI shows Alias when there is one, otherwise the address - mirror that here so a
        // renamed device keeps the same label without another round trip.
        if (changed.contains("Alias")) {
            map["name"] = changed.value("Alias").toString();
        } else if (changed.contains("Name")
                   && map.value("name").toString() == map.value("address").toString()) {
            map["name"] = changed.value("Name").toString();
        }

        m_devices[i] = map;
        return true;
    }

    return false;
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
