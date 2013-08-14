/****************************************************************************
** Meta object code from reading C++ file 'Window.h'
**
** Created: Wed Aug 14 11:17:23 2013
**      by: The Qt Meta Object Compiler version 67 (Qt 5.0.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../Window.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Window.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.0.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
struct qt_meta_stringdata_Window_t {
    QByteArrayData data[21];
    char stringdata[216];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    offsetof(qt_meta_stringdata_Window_t, stringdata) + ofs \
        - idx * sizeof(QByteArrayData) \
    )
static const qt_meta_stringdata_Window_t qt_meta_stringdata_Window = {
    {
QT_MOC_LITERAL(0, 0, 6),
QT_MOC_LITERAL(1, 7, 14),
QT_MOC_LITERAL(2, 22, 0),
QT_MOC_LITERAL(3, 23, 10),
QT_MOC_LITERAL(4, 34, 13),
QT_MOC_LITERAL(5, 48, 4),
QT_MOC_LITERAL(6, 53, 19),
QT_MOC_LITERAL(7, 73, 2),
QT_MOC_LITERAL(8, 76, 15),
QT_MOC_LITERAL(9, 92, 2),
QT_MOC_LITERAL(10, 95, 15),
QT_MOC_LITERAL(11, 111, 14),
QT_MOC_LITERAL(12, 126, 3),
QT_MOC_LITERAL(13, 130, 9),
QT_MOC_LITERAL(14, 140, 6),
QT_MOC_LITERAL(15, 147, 13),
QT_MOC_LITERAL(16, 161, 14),
QT_MOC_LITERAL(17, 176, 5),
QT_MOC_LITERAL(18, 182, 8),
QT_MOC_LITERAL(19, 191, 12),
QT_MOC_LITERAL(20, 204, 10)
    },
    "Window\0renderRayImage\0\0setBGColor\0"
    "setShadowMode\0mode\0setAmbientOcclusion\0"
    "ao\0setAntiAliasing\0aa\0setAliasingMode\0"
    "setRayPerLight\0rpl\0setNumDir\0numdir\0"
    "exportGLImage\0exportRayImage\0about\0"
    "calculAO\0getRayTracer\0RayTracer*\0"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_Window[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   79,    2, 0x0a,
       3,    0,   80,    2, 0x0a,
       4,    1,   81,    2, 0x0a,
       6,    1,   84,    2, 0x0a,
       8,    1,   87,    2, 0x0a,
      10,    1,   90,    2, 0x0a,
      11,    1,   93,    2, 0x0a,
      13,    1,   96,    2, 0x0a,
      15,    0,   99,    2, 0x0a,
      16,    0,  100,    2, 0x0a,
      17,    0,  101,    2, 0x0a,
      18,    0,  102,    2, 0x0a,
      19,    0,  103,    2, 0x0a,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    5,
    QMetaType::Void, QMetaType::Bool,    7,
    QMetaType::Void, QMetaType::Bool,    9,
    QMetaType::Void, QMetaType::Int,    5,
    QMetaType::Void, QMetaType::Int,   12,
    QMetaType::Void, QMetaType::Int,   14,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    0x80000000 | 20,

       0        // eod
};

void Window::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Window *_t = static_cast<Window *>(_o);
        switch (_id) {
        case 0: _t->renderRayImage(); break;
        case 1: _t->setBGColor(); break;
        case 2: _t->setShadowMode((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 3: _t->setAmbientOcclusion((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 4: _t->setAntiAliasing((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 5: _t->setAliasingMode((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 6: _t->setRayPerLight((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 7: _t->setNumDir((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 8: _t->exportGLImage(); break;
        case 9: _t->exportRayImage(); break;
        case 10: _t->about(); break;
        case 11: _t->calculAO(); break;
        case 12: { RayTracer* _r = _t->getRayTracer();
            if (_a[0]) *reinterpret_cast< RayTracer**>(_a[0]) = _r; }  break;
        default: ;
        }
    }
}

const QMetaObject Window::staticMetaObject = {
    { &QMainWindow::staticMetaObject, qt_meta_stringdata_Window.data,
      qt_meta_data_Window,  qt_static_metacall, 0, 0}
};


const QMetaObject *Window::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Window::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_Window.stringdata))
        return static_cast<void*>(const_cast< Window*>(this));
    return QMainWindow::qt_metacast(_clname);
}

int Window::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 13;
    }
    return _id;
}
QT_END_MOC_NAMESPACE
