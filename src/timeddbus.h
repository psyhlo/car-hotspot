#ifndef TIMEDDBUS_H
#define TIMEDDBUS_H

#include <QDateTime>
#include <QList>
#include <QMap>
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

inline uint runInstantCommand(const QString &command) {
    registerTypes();

    TimedEvent ev;
    ev.ticker = static_cast<int>(QDateTime::currentMSecsSinceEpoch() / 1000) + 1;
    ev.attr.txt.insert(QStringLiteral("APPLICATION"), QStringLiteral("harbour-carhotspot"));

    TimedAction act;
    // ActionFlags::Run_Command (0x08) | ActionFlags::When_Due (0x800)
    act.flags = 0x808;
    act.attr.txt.insert(QStringLiteral("COMMAND"), command);
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

inline uint scheduleWakeup(int seconds, const QString &command = QStringLiteral("/bin/true")) {
    registerTypes();

    TimedEvent ev;
    ev.ticker = static_cast<int>(QDateTime::currentMSecsSinceEpoch() / 1000) + seconds;
    ev.attr.txt.insert(QStringLiteral("APPLICATION"), QStringLiteral("harbour-carhotspot"));
    ev.attr.txt.insert(QStringLiteral("TITLE"), QStringLiteral("carhotspot_grace"));
    // EventFlags::Missed_If_Overdue (0x40) | EventFlags::Keep_Alive (0x800)
    ev.flags = 0x840;

    TimedAction act;
    // ActionFlags::Run_Command (0x08) | ActionFlags::Send_Cookie (0x4000)
    act.flags = 0x4008;
    act.attr.txt.insert(QStringLiteral("COMMAND"), command);
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
