#include "hotspotmanager.h"
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#include <QProcess>
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
    // Direct QDBusInterface to ConnMan WiFi Technology
    QDBusInterface wifiTech(
        "net.connman",
        "/net/connman/technology/wifi",
        "net.connman.Technology",
        QDBusConnection::systemBus()
    );

    if (wifiTech.isValid()) {
        QDBusReply<QVariantMap> reply = wifiTech.call("GetProperties");
        if (reply.isValid()) {
            bool tethering = reply.value().value("Tethering", false).toBool();
            if (m_isHotspotActive != tethering) {
                m_isHotspotActive = tethering;
                emit hotspotActiveChanged(m_isHotspotActive);
            }
            m_statusMessage = m_isHotspotActive ? "Hotspot is ON" : "Hotspot is OFF";
            emit statusMessageChanged(m_statusMessage);
            return;
        }
    }

    // Fallback: query via dbus-send
    QProcess p;
    p.start("dbus-send", QStringList() 
        << "--system" 
        << "--print-reply" 
        << "--dest=net.connman" 
        << "/net/connman/technology/wifi" 
        << "net.connman.Technology.GetProperties"
    );

    if (p.waitForFinished(2000)) {
        QString out = p.readAllStandardOutput();
        int tIdx = out.indexOf("\"Tethering\"");
        if (tIdx != -1) {
            QString snippet = out.mid(tIdx, 80);
            bool tethering = snippet.contains("boolean true", Qt::CaseInsensitive);
            if (m_isHotspotActive != tethering) {
                m_isHotspotActive = tethering;
                emit hotspotActiveChanged(m_isHotspotActive);
            }
            m_statusMessage = m_isHotspotActive ? "Hotspot is ON" : "Hotspot is OFF";
            emit statusMessageChanged(m_statusMessage);
        }
    }
}

void HotspotManager::updateHotspotState(bool active)
{
    if (m_isHotspotActive != active) {
        m_isHotspotActive = active;
        emit hotspotActiveChanged(m_isHotspotActive);
        m_statusMessage = m_isHotspotActive ? "Hotspot is ON" : "Hotspot is OFF";
        emit statusMessageChanged(m_statusMessage);
    }
}

void HotspotManager::setHotspotActive(bool active)
{
    m_statusMessage = active ? "Enabling Hotspot..." : "Disabling Hotspot...";
    emit statusMessageChanged(m_statusMessage);

    // 1. Always send D-Bus call to com.jolla.Connectiond (works in background, locked screen, daemon)
    QDBusInterface connDaemon(
        "com.jolla.Connectiond",
        "/Connectiond",
        "com.jolla.Connectiond",
        QDBusConnection::sessionBus()
    );

    if (connDaemon.isValid()) {
        if (active) {
            connDaemon.asyncCall("startTethering", QString("wifi"));
        } else {
            // stopTethering expects (QString type, bool force)
            connDaemon.asyncCall("stopTethering", QString("wifi"), true);
        }
    }

    // 2. Also trigger QML declarative ConnectionAgent if GUI is alive
    emit hotspotToggleRequested(active);

    // 3. Verify status after brief delay
    checkStatus();
}

void HotspotManager::onPropertyChanged(const QString &name, const QDBusVariant &value)
{
    if (name == "Tethering") {
        bool tethering = value.variant().toBool();
        if (m_isHotspotActive != tethering) {
            m_isHotspotActive = tethering;
            emit hotspotActiveChanged(m_isHotspotActive);
            m_statusMessage = m_isHotspotActive ? "Hotspot is ON" : "Hotspot is OFF";
            emit statusMessageChanged(m_statusMessage);
        }
    }
}
