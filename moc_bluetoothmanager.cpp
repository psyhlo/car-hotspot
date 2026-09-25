/****************************************************************************
** Meta object code from reading C++ file 'bluetoothmanager.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.6.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "src/bluetoothmanager.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'bluetoothmanager.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.6.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_BluetoothManager_t {
    QByteArrayData data[29];
    char stringdata0[461];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_BluetoothManager_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_BluetoothManager_t qt_meta_stringdata_BluetoothManager = {
    {
QT_MOC_LITERAL(0, 0, 16), // "BluetoothManager"
QT_MOC_LITERAL(1, 17, 14), // "devicesChanged"
QT_MOC_LITERAL(2, 32, 0), // ""
QT_MOC_LITERAL(3, 33, 23), // "targetConnectionChanged"
QT_MOC_LITERAL(4, 57, 9), // "connected"
QT_MOC_LITERAL(5, 67, 15), // "deviceConnected"
QT_MOC_LITERAL(6, 83, 4), // "name"
QT_MOC_LITERAL(7, 88, 7), // "address"
QT_MOC_LITERAL(8, 96, 18), // "deviceDisconnected"
QT_MOC_LITERAL(9, 115, 14), // "refreshDevices"
QT_MOC_LITERAL(10, 130, 15), // "setTargetDevice"
QT_MOC_LITERAL(11, 146, 16), // "setTargetDevices"
QT_MOC_LITERAL(12, 163, 9), // "addresses"
QT_MOC_LITERAL(13, 173, 22), // "ensureBluetoothPowered"
QT_MOC_LITERAL(14, 196, 19), // "onPropertiesChanged"
QT_MOC_LITERAL(15, 216, 9), // "interface"
QT_MOC_LITERAL(16, 226, 17), // "changedProperties"
QT_MOC_LITERAL(17, 244, 21), // "invalidatedProperties"
QT_MOC_LITERAL(18, 266, 17), // "onInterfacesAdded"
QT_MOC_LITERAL(19, 284, 15), // "QDBusObjectPath"
QT_MOC_LITERAL(20, 300, 10), // "objectPath"
QT_MOC_LITERAL(21, 311, 25), // "QMap<QString,QVariantMap>"
QT_MOC_LITERAL(22, 337, 23), // "interfacesAndProperties"
QT_MOC_LITERAL(23, 361, 19), // "onInterfacesRemoved"
QT_MOC_LITERAL(24, 381, 10), // "interfaces"
QT_MOC_LITERAL(25, 392, 7), // "devices"
QT_MOC_LITERAL(26, 400, 17), // "isTargetConnected"
QT_MOC_LITERAL(27, 418, 22), // "connectedTargetAddress"
QT_MOC_LITERAL(28, 441, 19) // "connectedTargetName"

    },
    "BluetoothManager\0devicesChanged\0\0"
    "targetConnectionChanged\0connected\0"
    "deviceConnected\0name\0address\0"
    "deviceDisconnected\0refreshDevices\0"
    "setTargetDevice\0setTargetDevices\0"
    "addresses\0ensureBluetoothPowered\0"
    "onPropertiesChanged\0interface\0"
    "changedProperties\0invalidatedProperties\0"
    "onInterfacesAdded\0QDBusObjectPath\0"
    "objectPath\0QMap<QString,QVariantMap>\0"
    "interfacesAndProperties\0onInterfacesRemoved\0"
    "interfaces\0devices\0isTargetConnected\0"
    "connectedTargetAddress\0connectedTargetName"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_BluetoothManager[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
      11,   14, // methods
       4,  108, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   69,    2, 0x06 /* Public */,
       3,    1,   70,    2, 0x06 /* Public */,
       5,    2,   73,    2, 0x06 /* Public */,
       8,    2,   78,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       9,    0,   83,    2, 0x0a /* Public */,
      10,    1,   84,    2, 0x0a /* Public */,
      11,    1,   87,    2, 0x0a /* Public */,
      13,    0,   90,    2, 0x0a /* Public */,
      14,    3,   91,    2, 0x08 /* Private */,
      18,    2,   98,    2, 0x08 /* Private */,
      23,    2,  103,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    4,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,    6,    7,
    QMetaType::Void, QMetaType::QString, QMetaType::QString,    6,    7,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    7,
    QMetaType::Void, QMetaType::QStringList,   12,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::QVariantMap, QMetaType::QStringList,   15,   16,   17,
    QMetaType::Void, 0x80000000 | 19, 0x80000000 | 21,   20,   22,
    QMetaType::Void, 0x80000000 | 19, QMetaType::QStringList,   20,   24,

 // properties: name, type, flags
      25, QMetaType::QVariantList, 0x00495001,
      26, QMetaType::Bool, 0x00495001,
      27, QMetaType::QString, 0x00495001,
      28, QMetaType::QString, 0x00495001,

 // properties: notify_signal_id
       0,
       1,
       1,
       1,

       0        // eod
};

void BluetoothManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        BluetoothManager *_t = static_cast<BluetoothManager *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->devicesChanged(); break;
        case 1: _t->targetConnectionChanged((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 2: _t->deviceConnected((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 3: _t->deviceDisconnected((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 4: _t->refreshDevices(); break;
        case 5: _t->setTargetDevice((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 6: _t->setTargetDevices((*reinterpret_cast< const QStringList(*)>(_a[1]))); break;
        case 7: _t->ensureBluetoothPowered(); break;
        case 8: _t->onPropertiesChanged((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QVariantMap(*)>(_a[2])),(*reinterpret_cast< const QStringList(*)>(_a[3]))); break;
        case 9: _t->onInterfacesAdded((*reinterpret_cast< const QDBusObjectPath(*)>(_a[1])),(*reinterpret_cast< const QMap<QString,QVariantMap>(*)>(_a[2]))); break;
        case 10: _t->onInterfacesRemoved((*reinterpret_cast< const QDBusObjectPath(*)>(_a[1])),(*reinterpret_cast< const QStringList(*)>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 9:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QDBusObjectPath >(); break;
            }
            break;
        case 10:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QDBusObjectPath >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (BluetoothManager::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&BluetoothManager::devicesChanged)) {
                *result = 0;
                return;
            }
        }
        {
            typedef void (BluetoothManager::*_t)(bool );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&BluetoothManager::targetConnectionChanged)) {
                *result = 1;
                return;
            }
        }
        {
            typedef void (BluetoothManager::*_t)(const QString & , const QString & );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&BluetoothManager::deviceConnected)) {
                *result = 2;
                return;
            }
        }
        {
            typedef void (BluetoothManager::*_t)(const QString & , const QString & );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&BluetoothManager::deviceDisconnected)) {
                *result = 3;
                return;
            }
        }
    }
#ifndef QT_NO_PROPERTIES
    else if (_c == QMetaObject::ReadProperty) {
        BluetoothManager *_t = static_cast<BluetoothManager *>(_o);
        Q_UNUSED(_t)
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< QVariantList*>(_v) = _t->devices(); break;
        case 1: *reinterpret_cast< bool*>(_v) = _t->isTargetConnected(); break;
        case 2: *reinterpret_cast< QString*>(_v) = _t->connectedTargetAddress(); break;
        case 3: *reinterpret_cast< QString*>(_v) = _t->connectedTargetName(); break;
        default: break;
        }
    } else if (_c == QMetaObject::WriteProperty) {
    } else if (_c == QMetaObject::ResetProperty) {
    }
#endif // QT_NO_PROPERTIES
}

const QMetaObject BluetoothManager::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_BluetoothManager.data,
      qt_meta_data_BluetoothManager,  qt_static_metacall, Q_NULLPTR, Q_NULLPTR}
};


const QMetaObject *BluetoothManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *BluetoothManager::qt_metacast(const char *_clname)
{
    if (!_clname) return Q_NULLPTR;
    if (!strcmp(_clname, qt_meta_stringdata_BluetoothManager.stringdata0))
        return static_cast<void*>(const_cast< BluetoothManager*>(this));
    return QObject::qt_metacast(_clname);
}

int BluetoothManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
#ifndef QT_NO_PROPERTIES
   else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 4;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 4;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 4;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 4;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 4;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void BluetoothManager::devicesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, Q_NULLPTR);
}

// SIGNAL 1
void BluetoothManager::targetConnectionChanged(bool _t1)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void BluetoothManager::deviceConnected(const QString & _t1, const QString & _t2)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void BluetoothManager::deviceDisconnected(const QString & _t1, const QString & _t2)
{
    void *_a[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&_t1)), const_cast<void*>(reinterpret_cast<const void*>(&_t2)) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_END_MOC_NAMESPACE
