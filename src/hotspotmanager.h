#ifndef HOTSPOTMANAGER_H
#define HOTSPOTMANAGER_H

#include <QObject>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusVariant>

class HotspotManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isHotspotActive READ isHotspotActive NOTIFY hotspotActiveChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool wifiWasPoweredBefore READ wifiWasPoweredBefore NOTIFY wifiWasPoweredBeforeChanged)

public:
    explicit HotspotManager(QObject *parent = nullptr);

    bool isHotspotActive() const { return m_isHotspotActive; }
    QString statusMessage() const { return m_statusMessage; }
    bool wifiWasPoweredBefore() const { return m_wifiWasPowered; }

public slots:
    void setHotspotActive(bool active);
    void checkStatus();
    void updateHotspotState(bool active);
    bool queryWifiPoweredState();

signals:
    void hotspotActiveChanged(bool active);
    void statusMessageChanged(const QString &msg);
    void hotspotToggleRequested(bool active);
    void restoreWifiRequested(bool powered);
    void wifiWasPoweredBeforeChanged(bool powered);

private slots:
    void onPropertyChanged(const QString &name, const QDBusVariant &value);

private:
    void restoreWifiStateIfNeeded();

    bool m_isHotspotActive = false;
    bool m_wifiWasPowered = false;
    bool m_wifiStateRestored = false;
    QString m_statusMessage = "Ready";
};

#endif // HOTSPOTMANAGER_H
