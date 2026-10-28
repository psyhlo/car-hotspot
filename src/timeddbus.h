#ifndef TIMEDDBUS_H
#define TIMEDDBUS_H

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusMetaType>

struct TimedCred {
    QString token;
    bool accrue = false;
};
Q_DECLARE_METATYPE(TimedCred)
Q_DECLARE_METATYPE(QList<TimedCred>)

inline QDBusArgument &operator<<(QDBusArgument &arg, const TimedCred &c) {
    arg.beginStructure();
    arg << c.token << c.accrue;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument &operator>>(const QDBusArgument &arg, TimedCred &c) {
    arg.beginStructure();
    arg >> c.token >> c.accrue;
    arg.endStructure();
    return arg;
}

struct TimedAttr {
    QMap<QString, QString> txt;
};
Q_DECLARE_METATYPE(TimedAttr)

inline QDBusArgument &operator<<(QDBusArgument &arg, const TimedAttr &a) {
    arg.beginStructure();
    arg << a.txt;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument &operator>>(const QDBusArgument &arg, TimedAttr &a) {
    arg.beginStructure();
    arg >> a.txt;
    arg.endStructure();
    return arg;
}

struct TimedButton {
    TimedAttr attr;
    uint snooze = 0;
};
Q_DECLARE_METATYPE(TimedButton)
Q_DECLARE_METATYPE(QList<TimedButton>)

inline QDBusArgument &operator<<(QDBusArgument &arg, const TimedButton &b) {
    arg.beginStructure();
    arg << b.attr << b.snooze;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument &operator>>(const QDBusArgument &arg, TimedButton &b) {
    arg.beginStructure();
    arg >> b.attr >> b.snooze;
    arg.endStructure();
    return arg;
}

struct TimedAction {
    uint flags = 0;
    TimedAttr attr;
    QList<TimedCred> cred;
};
Q_DECLARE_METATYPE(TimedAction)
Q_DECLARE_METATYPE(QList<TimedAction>)

inline QDBusArgument &operator<<(QDBusArgument &arg, const TimedAction &a) {
    arg.beginStructure();
    arg << a.flags << a.attr << a.cred;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument &operator>>(const QDBusArgument &arg, TimedAction &a) {
    arg.beginStructure();
    arg >> a.flags >> a.attr >> a.cred;
    arg.endStructure();
    return arg;
}

struct TimedRecurrence {
    quint64 mins = 0;
    uint hour = 0;
    uint mday = 0;
    uint wday = 0;
    uint mons = 0;
    uint flags = 0;
};
Q_DECLARE_METATYPE(TimedRecurrence)
Q_DECLARE_METATYPE(QList<TimedRecurrence>)

inline QDBusArgument &operator<<(QDBusArgument &arg, const TimedRecurrence &r) {
    arg.beginStructure();
    arg << r.mins << r.hour << r.mday << r.wday << r.mons << r.flags;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument &operator>>(const QDBusArgument &arg, TimedRecurrence &r) {
    arg.beginStructure();
    arg >> r.mins >> r.hour >> r.mday >> r.wday >> r.mons >> r.flags;
    arg.endStructure();
    return arg;
}

struct TimedEvent {
    int ticker = 0;
    uint year = 0;
    uint month = 0;
    uint day = 0;
    uint hour = 0;
    uint minute = 0;
    QString tz;
    TimedAttr attr;
    uint flags = 0;
    QList<TimedButton> b_attr;
    QList<TimedAction> actions;
    QList<TimedRecurrence> recrs;
    int t_snooze = 0;
    int b_snooze = 0;
    QList<TimedCred> cred_modifier;
};
Q_DECLARE_METATYPE(TimedEvent)

inline QDBusArgument &operator<<(QDBusArgument &arg, const TimedEvent &e) {
    arg.beginStructure();
    arg << e.ticker << e.year << e.month << e.day << e.hour << e.minute
        << e.tz << e.attr << e.flags << e.b_attr << e.actions << e.recrs
        << e.t_snooze << e.b_snooze << e.cred_modifier;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument &operator>>(const QDBusArgument &arg, TimedEvent &e) {
    arg.beginStructure();
    arg >> e.ticker >> e.year >> e.month >> e.day >> e.hour >> e.minute
        >> e.tz >> e.attr >> e.flags >> e.b_attr >> e.actions >> e.recrs
        >> e.t_snooze >> e.b_snooze >> e.cred_modifier;
    arg.endStructure();
    return arg;
}

namespace TimedDbus {

// timed EventFlags (see timed/common.h). Wakeup makes the RTC wake the device
// out of deep sleep; Keep_Alive keeps the event alive across timed restarts;
// Missed_If_Overdue drops the event instead of firing it late.
static const quint32 TIMED_EVENT_FLAG_MISSED_IF_OVERDUE = 0x040;
static const quint32 TIMED_EVENT_FLAG_WAKEUP            = 0x100;
static const quint32 TIMED_EVENT_FLAG_KEEP_ALIVE        = 0x800;

// timed ActionFlags. Send_Cookie makes timed emit its `cookie` D-Bus signal when
// the event fires - that is the only way an unprivileged application gets an
// alarm callback. Run_Command (0x008) is deliberately NOT used anywhere in this
// application: it executes a shell command outside of the Sailjail sandbox, which
// a Harbour application must not do (Base.permission includes disable-exec.inc and
// disable-programs.inc precisely to make that impossible from inside the sandbox).
static const quint32 TIMED_ACTION_FLAG_SEND_COOKIE = 0x4000;

inline void registerTypes() {
    static bool registered = false;
    if (!registered) {
        qDBusRegisterMetaType<TimedCred>();
        qDBusRegisterMetaType<QList<TimedCred>>();
        qDBusRegisterMetaType<TimedAttr>();
        qDBusRegisterMetaType<TimedButton>();
        qDBusRegisterMetaType<QList<TimedButton>>();
        qDBusRegisterMetaType<TimedAction>();
        qDBusRegisterMetaType<QList<TimedAction>>();
        qDBusRegisterMetaType<TimedRecurrence>();
        qDBusRegisterMetaType<QList<TimedRecurrence>>();
        qDBusRegisterMetaType<TimedEvent>();
        registered = true;
    }
}

// Schedules an RTC wakeup that only fires `cookie` on com.nokia.time. No command
// string is involved, so nothing is executed outside the sandbox.
inline uint scheduleWakeup(int seconds) {
    registerTypes();

    TimedEvent ev;
    ev.ticker = static_cast<int>(QDateTime::currentMSecsSinceEpoch() / 1000) + seconds;
    ev.attr.txt.insert(QStringLiteral("APPLICATION"), QStringLiteral("harbour-carhotspot"));
    ev.attr.txt.insert(QStringLiteral("TITLE"), QStringLiteral("carhotspot_grace"));
    ev.flags = TIMED_EVENT_FLAG_MISSED_IF_OVERDUE | TIMED_EVENT_FLAG_WAKEUP
             | TIMED_EVENT_FLAG_KEEP_ALIVE;

    TimedAction act;
    act.flags = TIMED_ACTION_FLAG_SEND_COOKIE;
    ev.actions.append(act);

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("com.nokia.time"),
        QStringLiteral("/com/nokia/time"),
        QStringLiteral("com.nokia.time"),
        QStringLiteral("add_event")
    );
    msg << QVariant::fromValue(ev);

    QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        return reply.arguments().first().toUInt();
    }
    return 0;
}

// timed reports fired cookies as `cookie(uint)` on /com/nokia/time. Without a
// listener a scheduled wakeup is never consumed, so the caller has no way to know
// that the grace period elapsed while the device was asleep.
inline void listenForCookies(QObject *receiver, const char *slot) {
    registerTypes();
    QDBusConnection::systemBus().connect(
        QStringLiteral("com.nokia.time"),
        QStringLiteral("/com/nokia/time"),
        QStringLiteral("com.nokia.time"),
        QStringLiteral("cookie"),
        receiver,
        slot
    );
}

inline void cancelCookie(uint cookie) {
    if (cookie == 0)
        return;

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("com.nokia.time"),
        QStringLiteral("/com/nokia/time"),
        QStringLiteral("com.nokia.time"),
        QStringLiteral("cancel")
    );
    msg << cookie;
    QDBusConnection::systemBus().call(msg, QDBus::NoBlock);
}

} // namespace TimedDbus

#endif // TIMEDDBUS_H
