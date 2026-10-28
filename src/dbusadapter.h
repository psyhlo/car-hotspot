#ifndef DBUSADAPTER_H
#define DBUSADAPTER_H

#include <QObject>
#include <QString>

/*
 * Narrow D-Bus surface of Car Hotspot.
 *
 * Inside a Sailjail sandbox an application may own only
 * <OrganizationName>.<ApplicationName> and org.sailfishos.coveraction.*
 * (see sailjail README "Implicit D-Bus user service ownership" and
 * Base.permission), and the Base policy grants *broadcast* between
 * org.sailfishos.coveraction.* names but not method calls. So the UI process and
 * the background service process talk to each other with signals only, and the
 * only method entry points that are exposed are the ones lipstick needs to bring
 * the app to the foreground from a notification.
 */
class CarHotspotDBusAdapter : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.sailfishos.coveraction.carhotspot")

public:
    explicit CarHotspotDBusAdapter(bool isDaemon, QObject *parent = nullptr);

    bool isDaemon() const { return m_isDaemon; }

    // Process running in the background (started by systemd).
    QString status() const { return m_status; }
    QString activityLog() const { return m_log; }

public slots:
    // Called by lipstick through the notification remote action.
    void openApp();
    void activate();

    // Read-only diagnostic snapshot for external callers.
    QString backgroundStatus() const { return status(); }

signals:
    void requestActivate();

    // UI -> service (UI owns org.sailfishos.coveraction.carhotspot_ui).
    void manualToggleRequested(bool active);
    void settingsChanged();

    // service -> UI (service owns org.sailfishos.coveraction.carhotspot).
    void logChanged(const QString &log);
    void daemonStateChanged(bool active);

    // Internal helpers used by AppController to keep the exported payload fresh.
    void statusUpdated(const QString &status);

public:
    void setStatus(const QString &status) { m_status = status; }
    void setLog(const QString &log) { m_log = log; }

private:
    bool m_isDaemon = false;
    QString m_status;
    QString m_log;
};

#endif // DBUSADAPTER_H
