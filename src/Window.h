#ifndef WINDOW_H
#define WINDOW_H

#include <QMainWindow>
#include <QAction>
#include <QToolBar>
#include <QCheckBox>
#include <QGroupBox>
#include <QComboBox>
#include <QSlider>
#include <QLCDNumber>
#include <QSpinBox>
#include <QImage>
#include <QLabel>

#include <vector>
#include <string>

#include "QTUtils.h"
#include "GLViewer.h"
#include "RayTracer.h"

class Window : public QMainWindow {
    Q_OBJECT

public:
    Window();
    virtual ~Window();

    static void showStatusMessage (const QString & msg);

public slots :
    void renderRayImage ();
    void setBGColor ();
    void setShadowMode(bool mode);
    void setAmbientOcclusion(bool ao);
    void setAntiAliasing(bool aa);
    void setAliasingMode(int mode);
    void setRayPerLight(int rpl);
    void setWiframe(bool m);
    void setNumDir(int numdir);
    void exportGLImage ();
    void exportRayImage ();
    void about ();
    void calculAO ();
    void setRenderingMode(int m);

    RayTracer * getRayTracer(){ return rayTracer; }

public :
    RayTracer * rayTracer;
private :
    void initControlWidget ();

    QActionGroup * actionGroup;
    QGroupBox * controlWidget;
    QString currentDirectory;

    GLViewer * viewer;
    QLabel * imageLabel;
    QImage rayImage;
};

#endif // WINDOW_H


// Some Emacs-Hints -- please don't remove:
//
//  Local Variables:
//  mode:C++
//  tab-width:4
//  End:
