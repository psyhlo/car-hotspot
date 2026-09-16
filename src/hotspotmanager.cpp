#include "hotspotmanager.h"
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QDebug>

HotspotManager::HotspotManager(QObject *parent)
    : QObject(parent)
{
    // ConnMan technology interface for WiFi
    QDBusConnection::systemBus().connect(
        "net.connman",
        "/net/connman/technology/wifi",
        "net.connman.Technology",
        "PropertyChanged",
        this,
        SLOT(onPropertyChanged(QString, QDBusVariant))
    );

    checkStatus();
}

void HotspotManager::checkStatus()
{
    QDBusInterface wifiTech(
        "net.connman",
        "/net/connman/technology/wifi",
        "net.connman.Technology",
        QDBusConnection::systemBus()
    );

    if (!wifiTech.isValid()) {
        m_statusMessage = "ConnMan WiFi technology unavailable";
        emit statusMessageChanged(m_statusMessage);
        return;
    }

    QDBusReply<QVariantMap> reply = wifiTech.call("GetProperties");
    if (reply.isValid()) {
        QVariantMap props = reply.value();
        bool tethering = props.value("Tethering", false).toBool();
        if (m_isHotspotActive != tethering) {
            m_isHotspotActive = tethering;
            emit hotspotActiveChanged(m_isHotspotActive);
        }
        m_statusMessage = m_isHotspotActive ? "Hotspot is ON" : "Hotspot is OFF";
        emit statusMessageChanged(m_statusMessage);
    }
}

void HotspotManager::setHotspotActive(bool active)
{
    QDBusInterface wifiTech(
        "net.connman",
        "/net/connman/technology/wifi",
        "net.connman.Technology",
        QDBusConnection::systemBus()
    );

    if (!wifiTech.isValid()) {
        m_statusMessage = "Error: ConnMan service unreachable";
        emit statusMessageChanged(m_statusMessage);
        return;
    }

    m_statusMessage = active ? "Enabling Hotspot..." : "Disabling Hotspot...";
    emit statusMessageChanged(m_statusMessage);

    // Call SetProperty("Tethering", QDBusVariant(bool))
    QDBusMessage msg = wifiTech.call(
        "SetProperty",
        "Tethering",
        QVariant::fromValue(QDBusVariant(active))
    );

    if (msg.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "Failed to set Tethering:" << msg.errorMessage();
        m_statusMessage = QString("Failed: %1").arg(msg.errorMessage());
        emit statusMessageChanged(m_statusMessage);
    } else {
        m_isHotspotActive = active;
        emit hotspotActiveChanged(m_isHotspotActive);
        m_statusMessage = active ? "Hotspot enabled" : "Hotspot disabled";
        emit statusMessageChanged(m_statusMessage);
    }
}

void HotspotManager::onPropertyChanged(const QString &name, const QDBusVariant &value)
{
    if (name == "Tethering") {
        bool tethering = value.variant().toBool();
        if (m_isHotspotActive != tethering) {
            m_isHotspotActive = tethering;
            emit hotspotActiveChanged(m_isHotspotActive);
            m_statusMessage = m_isHotspotActive ? "Hotspot turned ON" : "Hotspot turned OFF";
            emit statusMessageChanged(m_statusMessage);
        }
    }
}
