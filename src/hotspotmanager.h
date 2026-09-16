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

public:
    explicit HotspotManager(QObject *parent = nullptr);

    bool isHotspotActive() const { return m_isHotspotActive; }
    QString statusMessage() const { return m_statusMessage; }

public slots:
    void setHotspotActive(bool active);
    void checkStatus();

signals:
    void hotspotActiveChanged(bool active);
    void statusMessageChanged(const QString &msg);

private slots:
    void onPropertyChanged(const QString &name, const QDBusVariant &value);

private:
    bool m_isHotspotActive = false;
    QString m_statusMessage = "Ready";
};

#endif // HOTSPOTMANAGER_H
