#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H

#include <QObject>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

class SystemMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int batteryChargePercentage READ batteryChargePercentage NOTIFY batteryChanged)
    Q_PROPERTY(bool isBatteryCharging READ isBatteryCharging NOTIFY batteryChanged)
    Q_PROPERTY(bool isRoaming READ isRoaming NOTIFY roamingChanged)

public:
    explicit SystemMonitor(QObject *parent = nullptr);

    int batteryChargePercentage() const { return m_batteryPercentage; }
    bool isBatteryCharging() const { return m_isCharging; }
    bool isRoaming() const { return m_isRoaming; }

public slots:
    void refreshStatus();

signals:
    void batteryChanged(int percentage, bool charging);
    void roamingChanged(bool roaming);

private slots:
    void onOfonoPropertyChanged(const QString &name, const QVariant &value);
    void onUpowerPropertyChanged(const QString &interface, const QVariantMap &changedProperties, const QStringList &invalidatedProperties);

private:
    void queryBattery();
    void queryRoaming();

    int m_batteryPercentage = 100;
    bool m_isCharging = false;
    bool m_isRoaming = false;
};

#endif // SYSTEMMONITOR_H
