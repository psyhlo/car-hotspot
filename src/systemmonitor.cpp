#include "systemmonitor.h"
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDebug>

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent)
{
    // Connect to oFono NetworkRegistration property changes (Roaming)
    QDBusConnection::systemBus().connect(
        "org.ofono",
        QString(), // any modem path (e.g. /ril_0)
        "org.ofono.NetworkRegistration",
        "PropertyChanged",
        this,
        SLOT(onOfonoPropertyChanged(QString, QVariant))
    );

    // Connect to Sailfish OS MCE battery and charger signals (permitted in Sailjail Base.permission)
    QDBusConnection::systemBus().connect(
        "com.nokia.mce",
        "/com/nokia/mce/signal",
        "com.nokia.mce.signal",
        "battery_level_ind",
        this,
        SLOT(onMceBatteryLevelChanged(int))
    );

    QDBusConnection::systemBus().connect(
        "com.nokia.mce",
        "/com/nokia/mce/signal",
        "com.nokia.mce.signal",
        "charger_state_ind",
        this,
        SLOT(onMceChargerStateChanged(QString))
    );

    // Connect to UPower DisplayDevice property changes (fallback)
    QDBusConnection::systemBus().connect(
        "org.freedesktop.UPower",
        "/org/freedesktop/UPower/devices/DisplayDevice",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        this,
        SLOT(onUpowerPropertyChanged(QString, QVariantMap, QStringList))
    );

    refreshStatus();
}

void SystemMonitor::refreshStatus()
{
    queryBattery();
    queryRoaming();
}

void SystemMonitor::queryBattery()
{
    // 1. Primary: Sailfish OS MCE via D-Bus (works inside Sailjail Base.permission)
    QDBusInterface mce(
        "com.nokia.mce",
        "/com/nokia/mce/request",
        "com.nokia.mce.request",
        QDBusConnection::systemBus()
    );

    if (mce.isValid()) {
        QDBusReply<int> levelReply = mce.call("get_battery_level");
        QDBusReply<QString> chargerReply = mce.call("get_charger_state");
        if (levelReply.isValid() && levelReply.value() >= 0 && levelReply.value() <= 100) {
            m_batteryPercentage = levelReply.value();
            if (chargerReply.isValid()) {
                m_isCharging = (chargerReply.value() == QLatin1String("on"));
            }
            emit batteryChanged(m_batteryPercentage, m_isCharging);
            return;
        }
    }

    // 2. Fallback: UPower DisplayDevice via D-Bus
    QDBusInterface upower(
        "org.freedesktop.UPower",
        "/org/freedesktop/UPower/devices/DisplayDevice",
        "org.freedesktop.UPower.Device",
        QDBusConnection::systemBus()
    );

    if (upower.isValid()) {
        QVariant pct = upower.property("Percentage");
        QVariant state = upower.property("State"); // 1: Charging, 4: Fully charged
        if (pct.isValid()) {
            m_batteryPercentage = pct.toInt();
            int st = state.toInt();
            m_isCharging = (st == 1 || st == 4);
            emit batteryChanged(m_batteryPercentage, m_isCharging);
            return;
        }
    }

    // 3. Fallback: Direct Linux sysfs (/sys/class/power_supply/battery)
    QFile capFile("/sys/class/power_supply/battery/capacity");
    if (capFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&capFile);
        int val = in.readLine().trimmed().toInt();
        if (val > 0 && val <= 100) {
            m_batteryPercentage = val;
        }
        capFile.close();
    }

    QFile stFile("/sys/class/power_supply/battery/status");
    if (stFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&stFile);
        QString status = in.readLine().trimmed();
        m_isCharging = (status == "Charging" || status == "Full");
        stFile.close();
    }

    emit batteryChanged(m_batteryPercentage, m_isCharging);
}

void SystemMonitor::queryRoaming()
{
    bool roamingDetected = false;

    // 1. Query oFono Manager for all modems (supports Dual-SIM / Multi-SIM)
    QDBusInterface ofono(
        "org.ofono",
        "/",
        "org.ofono.Manager",
        QDBusConnection::systemBus()
    );

    if (ofono.isValid()) {
        QDBusReply<QVariantList> reply = ofono.call("GetModems");
        if (reply.isValid()) {
            for (const QVariant &item : reply.value()) {
                const QDBusArgument &arg = item.value<QDBusArgument>();
                if (arg.currentType() == QDBusArgument::StructureType) {
                    arg.beginStructure();
                    QDBusObjectPath modemPath;
                    QVariantMap props;
                    arg >> modemPath >> props;
                    arg.endStructure();

                    QDBusInterface netReg(
                        "org.ofono",
                        modemPath.path(),
                        "org.ofono.NetworkRegistration",
                        QDBusConnection::systemBus()
                    );

                    if (netReg.isValid()) {
                        QDBusReply<QVariantMap> netProps = netReg.call("GetProperties");
                        if (netProps.isValid()) {
                            QString status = netProps.value().value("Status").toString();
                            if (status == "roaming") {
                                roamingDetected = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // 2. If not already flagged by oFono, also check ConnMan active cellular services
    if (!roamingDetected) {
        QDBusInterface connman(
            "net.connman",
            "/",
            "net.connman.Manager",
            QDBusConnection::systemBus()
        );

        if (connman.isValid()) {
            QDBusReply<QVariantList> reply = connman.call("GetServices");
            if (reply.isValid()) {
                for (const QVariant &item : reply.value()) {
                    const QDBusArgument &arg = item.value<QDBusArgument>();
                    if (arg.currentType() == QDBusArgument::StructureType) {
                        arg.beginStructure();
                        QDBusObjectPath path;
                        QVariantMap props;
                        arg >> path >> props;
                        arg.endStructure();

                        if (props.value("Type").toString() == "cellular") {
                            if (props.value("Roaming", false).toBool()) {
                                roamingDetected = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if (m_isRoaming != roamingDetected) {
        m_isRoaming = roamingDetected;
        emit roamingChanged(m_isRoaming);
    }
}

void SystemMonitor::onOfonoPropertyChanged(const QString &name, const QVariant &value)
{
    Q_UNUSED(value);
    if (name == "Status") {
        queryRoaming();
    }
}

void SystemMonitor::onUpowerPropertyChanged(const QString &interface, const QVariantMap &changedProperties, const QStringList &invalidatedProperties)
{
    Q_UNUSED(interface);
    Q_UNUSED(invalidatedProperties);
    if (changedProperties.contains("Percentage") || changedProperties.contains("State")) {
        queryBattery();
    }
}

void SystemMonitor::onMceBatteryLevelChanged(int level)
{
    if (level >= 0 && level <= 100 && m_batteryPercentage != level) {
        m_batteryPercentage = level;
        emit batteryChanged(m_batteryPercentage, m_isCharging);
    }
}

void SystemMonitor::onMceChargerStateChanged(const QString &state)
{
    bool charging = (state == QLatin1String("on"));
    if (m_isCharging != charging) {
        m_isCharging = charging;
        emit batteryChanged(m_batteryPercentage, m_isCharging);
    }
}

