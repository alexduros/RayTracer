TEMPLATE = app
TARGET = raymini
CONFIG += qt \
    opengl \
    warn_on \
    release \
    thread
HEADERS = Window.h \
    GLViewer.h \
    QTUtils.h \
    Vertex.h \
    Triangle.h \
    Mesh.h \
    BoundingBox.h \
    Material.h \
    Object.h \
    Light.h \
    Scene.h \
    RayTracer.h \
    Ray.h \
    KdTree.h \
    Vec3D.h \
    TriangleC.h
SOURCES = Window.cpp \
    GLViewer.cpp \
    QTUtils.cpp \
    Vertex.cpp \
    Triangle.cpp \
    Mesh.cpp \
    BoundingBox.cpp \
    Material.cpp \
    Object.cpp \
    Light.cpp \
    Scene.cpp \
    RayTracer.cpp \
    Ray.cpp \
    Main.cpp \
    KdTree.cpp \
    TriangleC.cpp
DESTDIR = .
QT_VERSION = $$[QT_VERSION]
QMAKE_FLAGS += -F/Library/Frameworks
LIBS += -framework QGLViewer

contains( QT_VERSION, "^4\\..*" ):QT *= xml \
    opengl
else:CONFIG *= thread


