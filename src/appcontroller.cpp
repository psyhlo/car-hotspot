#include "appcontroller.h"
#include <QDateTime>
#include <QDebug>

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_settings("harbour-hotspotincar", "harbour-hotspotincar")
{
    loadSettings();

    connect(&m_bluetooth, &BluetoothManager::targetConnectionChanged,
            this, &AppController::onTargetConnectionChanged);
    connect(&m_bluetooth, &BluetoothManager::devicesChanged,
            this, &AppController::onDevicesChanged);

    appendLog(QString("[%1] Service started").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
}

void AppController::loadSettings()
{
    m_targetAddress = m_settings.value("targetAddress", "").toString();
    m_targetName = m_settings.value("targetName", "").toString();
    m_autoToggle = m_settings.value("autoToggle", true).toBool();

    if (!m_targetAddress.isEmpty()) {
        m_bluetooth.setTargetDevice(m_targetAddress);
    }
}

void AppController::saveSettings()
{
    m_settings.setValue("targetAddress", m_targetAddress);
    m_settings.setValue("targetName", m_targetName);
    m_settings.setValue("autoToggle", m_autoToggle);
    m_settings.sync();
}

void AppController::setTargetAddress(const QString &address)
{
    if (m_targetAddress != address) {
        m_targetAddress = address;
        m_bluetooth.setTargetDevice(address);
        saveSettings();
        emit targetAddressChanged(m_targetAddress);
        appendLog(QString("[%1] Target set to: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), address));
    }
}

void AppController::setAutoToggle(bool enabled)
{
    if (m_autoToggle != enabled) {
        m_autoToggle = enabled;
        saveSettings();
        emit autoToggleChanged(m_autoToggle);
        appendLog(QString("[%1] Auto-toggle: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), enabled ? "ENABLED" : "DISABLED"));
    }
}

void AppController::selectDevice(const QString &address, const QString &name)
{
    m_targetAddress = address;
    m_targetName = name;
    m_bluetooth.setTargetDevice(address);
    saveSettings();
    emit targetAddressChanged(m_targetAddress);
    emit targetNameChanged(m_targetName);
    appendLog(QString("[%1] Selected car: %2 (%3)").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), name, address));
}

void AppController::toggleHotspotManual(bool active)
{
    appendLog(QString("[%1] Manual toggle Hotspot: %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), active ? "ON" : "OFF"));
    m_hotspot.setHotspotActive(active);
}

void AppController::appendLog(const QString &text)
{
    if (!m_logStatus.isEmpty()) {
        m_logStatus = text + "\n" + m_logStatus;
        // keep recent 20 lines
        QStringList lines = m_logStatus.split("\n");
        if (lines.size() > 20) {
            lines = lines.mid(0, 20);
            m_logStatus = lines.join("\n");
        }
    } else {
        m_logStatus = text;
    }
    emit logStatusChanged(m_logStatus);
}

void AppController::onTargetConnectionChanged(bool connected)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    if (connected) {
        appendLog(QString("[%1] Car Bluetooth connected!").arg(timestamp));
        if (m_autoToggle) {
            appendLog(QString("[%1] Auto-enabling Hotspot...").arg(timestamp));
            m_hotspot.setHotspotActive(true);
        }
    } else {
        appendLog(QString("[%1] Car Bluetooth disconnected.").arg(timestamp));
        if (m_autoToggle) {
            appendLog(QString("[%1] Auto-disabling Hotspot...").arg(timestamp));
            m_hotspot.setHotspotActive(false);
        }
    }
}

void AppController::onDevicesChanged()
{
    // If target name is empty or needs refresh from devices
    if (!m_targetAddress.isEmpty()) {
        const auto devs = m_bluetooth.devices();
        for (const QVariant &v : devs) {
            QVariantMap m = v.toMap();
            if (m.value("address").toString().compare(m_targetAddress, Qt::CaseInsensitive) == 0) {
                QString currentName = m.value("name").toString();
                if (m_targetName != currentName) {
                    m_targetName = currentName;
                    saveSettings();
                    emit targetNameChanged(m_targetName);
                }
                break;
            }
        }
    }
}
