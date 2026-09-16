/****************************************************************************
** Meta object code from reading C++ file 'appcontroller.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.6.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "src/appcontroller.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'appcontroller.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.6.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_AppController_t {
    QByteArrayData data[26];
    char stringdata0[320];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_AppController_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_AppController_t qt_meta_stringdata_AppController = {
    {
QT_MOC_LITERAL(0, 0, 13), // "AppController"
QT_MOC_LITERAL(1, 14, 20), // "targetAddressChanged"
QT_MOC_LITERAL(2, 35, 0), // ""
QT_MOC_LITERAL(3, 36, 7), // "address"
QT_MOC_LITERAL(4, 44, 17), // "targetNameChanged"
QT_MOC_LITERAL(5, 62, 4), // "name"
QT_MOC_LITERAL(6, 67, 17), // "autoToggleChanged"
QT_MOC_LITERAL(7, 85, 7), // "enabled"
QT_MOC_LITERAL(8, 93, 16), // "logStatusChanged"
QT_MOC_LITERAL(9, 110, 3), // "log"
QT_MOC_LITERAL(10, 114, 12), // "selectDevice"
QT_MOC_LITERAL(11, 127, 19), // "toggleHotspotManual"
QT_MOC_LITERAL(12, 147, 6), // "active"
QT_MOC_LITERAL(13, 154, 9), // "appendLog"
QT_MOC_LITERAL(14, 164, 4), // "text"
QT_MOC_LITERAL(15, 169, 25), // "onTargetConnectionChanged"
QT_MOC_LITERAL(16, 195, 9), // "connected"
QT_MOC_LITERAL(17, 205, 16), // "onDevicesChanged"
QT_MOC_LITERAL(18, 222, 9), // "bluetooth"
QT_MOC_LITERAL(19, 232, 17), // "BluetoothManager*"
QT_MOC_LITERAL(20, 250, 7), // "hotspot"
QT_MOC_LITERAL(21, 258, 15), // "HotspotManager*"
QT_MOC_LITERAL(22, 274, 13), // "targetAddress"
QT_MOC_LITERAL(23, 288, 10), // "targetName"
QT_MOC_LITERAL(24, 299, 10), // "autoToggle"
QT_MOC_LITERAL(25, 310, 9) // "logStatus"

    },
    "AppController\0targetAddressChanged\0\0"
    "address\0targetNameChanged\0name\0"
    "autoToggleChanged\0enabled\0logStatusChanged\0"
    "log\0selectDevice\0toggleHotspotManual\0"
    "active\0appendLog\0text\0onTargetConnectionChanged\0"
    "connected\0onDevicesChanged\0bluetooth\0"
    "BluetoothManager*\0hotspot\0HotspotManager*\0"
    "targetAddress\0targetName\0autoToggle\0"
    "logStatus"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_AppController[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       9,   14, // methods
       6,   86, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   59,    2, 0x06 /* Public */,
       4,    1,   62,    2, 0x06 /* Public */,
       6,    1,   65,    2, 0x06 /* Public */,
       8,    1,   68,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      10,    2,   71,    2, 0x0a /* Public */,
      11,    1,   76,    2, 0x0a /* Public */,
      13,    1,   79,    2, 0x0a /* Public */,
      15,    1,   82,    2, 0x08 /* Private */,
      17,    0,   85,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString,    3,
    QMetaType::Void, QMetaType::QString,    5,
    QMetaType::Void, QMetaType::Bool,    7,
    QMetaType::Void, QMetaType::QString,    9,

 // slots: parameters
    QMetaType::Void, QMetaType::QString, QMetaType::QString,    3,    5,
    QMetaType::Void, QMetaType::Bool,   12,
    QMetaType::Void, QMetaType::QString,   14,
    QMetaType::Void, QMetaType::Bool,   16,
    QMetaType::Void,

 // properties: name, type, flags
      18, 0x80000000 | 19, 0x00095409,
      20, 0x80000000 | 21, 0x00095409,
      22, QMetaType::QString, 0x00495103,
      23, QMetaType::QString, 0x00495001,
      24, QMetaType::Bool, 0x00495103,
      25, QMetaType::QString, 0x00495001,

 // properties: notify_signal_id
       0,
       0,
       0,
       1,
       2,
       3,

       0        // eod
};

void AppController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        AppController *_t = static_cast<AppController *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->targetAddressChanged((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 1: _t->targetNameChanged((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 2: _t->autoToggleChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 3: _t->logStatusChanged((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 4: _t->selectDevice((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 5: _t->toggleHotspotManual((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 6: _t->appendLog((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 7: _t->onTargetConnectionChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 8: _t->onDevicesChanged(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (AppController::*_t)(const QString & );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&AppController::targetAddressChanged)) {
                *result = 0;
                return;
            }
        }
        {
            typedef void (AppController::*_t)(const QString & );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&AppController::targetNameChanged)) {
                *result = 1;
                return;
            }
        }
        {
            typedef void (AppController::*_t)(bool );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&AppController::autoToggleChanged)) {
                *result = 2;
                return;
            }
        }
        {
            typedef void (AppController::*_t)(const QString & );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&AppController::logStatusChanged)) {
                *result = 3;
                return;
            }
        }
    } else if (_c == QMetaObject::RegisterPropertyMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 0:
            *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< BluetoothManager* >(); break;
        case 1:
            *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< HotspotManager* >(); break;
        }
    }

#ifndef QT_NO_PROPERTIES
    else if (_c == QMetaObject::ReadProperty) {
        AppController *_t = static_cast<AppController *>(_o);
        Q_UNUSED(_t)
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< BluetoothManager**>(_v) = _t->bluetooth(); break;
        case 1: *reinterpret_cast< HotspotManager**>(_v) = _t->hotspot(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->targetAddress(); break;
        case 3: *reinterpret_cast< QString*>(_v) = _t->targetName(); break;
        case 4: *reinterpret_cast< bool*>(_v) = _t->autoToggle(); break;
        case 5: *reinterpret_cast< QString*>(_v) = _t->logStatus(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
        AppController *_t = static_cast<AppController *>(_o);
        Q_UNUSED(_t)
        void *_v = _a[0];
        switch (_id) {
        case 2: _t->setTargetAddress(*reinterpret_cast< QString*>(_v)); break;
        case 4: _t->setAutoToggle(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    } else if (_c == QMetaObject::ResetProperty) {
    }
#endif // QT_NO_PROPERTIES
}

const QMetaObject AppController::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_AppController.data,
      qt_meta_data_AppController,  qt_static_metacall, Q_NULLPTR, Q_NULLPTR}
};


const QMetaObject *AppController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AppController::qt_metacast(const char *_clname)
{
    if (!_clname) return Q_NULLPTR;
    if (!strcmp(_clname, qt_meta_stringdata_AppController.stringdata0))
        return static_cast<void*>(const_cast< AppController*>(this));
    return QObject::qt_metacast(_clname);
}

int AppController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 9;
    }
#ifndef QT_NO_PROPERTIES
   else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 6;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 6;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 6;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 6;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 6;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void AppController::targetAddressChanged(const QString & _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void AppController::targetNameChanged(const QString & _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void AppController::autoToggleChanged(bool _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void AppController::logStatusChanged(const QString & _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_END_MOC_NAMESPACE
