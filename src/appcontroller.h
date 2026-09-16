#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QSettings>
#include "bluetoothmanager.h"
#include "hotspotmanager.h"

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(BluetoothManager* bluetooth READ bluetooth CONSTANT)
    Q_PROPERTY(HotspotManager* hotspot READ hotspot CONSTANT)
    Q_PROPERTY(QString targetAddress READ targetAddress WRITE setTargetAddress NOTIFY targetAddressChanged)
    Q_PROPERTY(QString targetName READ targetName NOTIFY targetNameChanged)
    Q_PROPERTY(bool autoToggle READ autoToggle WRITE setAutoToggle NOTIFY autoToggleChanged)
    Q_PROPERTY(QString logStatus READ logStatus NOTIFY logStatusChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    BluetoothManager* bluetooth() { return &m_bluetooth; }
    HotspotManager* hotspot() { return &m_hotspot; }

    QString targetAddress() const { return m_targetAddress; }
    void setTargetAddress(const QString &address);

    QString targetName() const { return m_targetName; }
    bool autoToggle() const { return m_autoToggle; }
    void setAutoToggle(bool enabled);

    QString logStatus() const { return m_logStatus; }

public slots:
    void selectDevice(const QString &address, const QString &name);
    void toggleHotspotManual(bool active);
    void appendLog(const QString &text);

signals:
    void targetAddressChanged(const QString &address);
    void targetNameChanged(const QString &name);
    void autoToggleChanged(bool enabled);
    void logStatusChanged(const QString &log);

private slots:
    void onTargetConnectionChanged(bool connected);
    void onDevicesChanged();

private:
    void loadSettings();
    void saveSettings();

    BluetoothManager m_bluetooth;
    HotspotManager m_hotspot;
    QSettings m_settings;

    QString m_targetAddress;
    QString m_targetName;
    bool m_autoToggle = true;
    QString m_logStatus;
};

#endif // APPCONTROLLER_H
