#include "Window.h"

#include <vector>
#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>

#include <QDockWidget>
#include <QGroupBox>
#include <QButtonGroup>
#include <QMenuBar>
#include <QApplication>
#include <QLayout>
#include <QLabel>
#include <QProgressBar>
#include <QCheckBox>
#include <QRadioButton>
#include <QColorDialog>
#include <QLCDNumber>
#include <QPixmap>
#include <QFrame>
#include <QSplitter>
#include <QMenu>
#include <QScrollArea>
#include <QFont>
#include <QSizePolicy>
#include <QImageReader>
#include <QStatusBar>
#include <QPushButton>
#include <QMessageBox>
#include <QFileDialog>

#include "Scene.h"
#include "RayTracer.h"
#include "GLViewer.h"

using namespace std;


Window::Window () : QMainWindow (NULL) {
    cout << "Entering window" << endl;
    viewer = new GLViewer ();

    cout << "Creates Layout" << endl;
    QGroupBox * renderingGroupBox = new QGroupBox (this);
    QHBoxLayout * renderingLayout = new QHBoxLayout (renderingGroupBox);

    imageLabel = new QLabel;
    imageLabel->setBackgroundRole (QPalette::Base);
    imageLabel->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel->setScaledContents (true);
    imageLabel->setPixmap (QPixmap::fromImage (rayImage));

    cout << "Adding imageLabel" << endl;
    renderingLayout->addWidget (viewer);
    renderingLayout->addWidget (imageLabel);

    setCentralWidget (renderingGroupBox);

    cout << "Creates control widget" << endl;
    QDockWidget * controlDockWidget = new QDockWidget (this);
    initControlWidget ();

    controlDockWidget->setWidget (controlWidget);
    QWidget* titleWidget = new QWidget(this);
    controlDockWidget->setTitleBarWidget(titleWidget);
    controlDockWidget->adjustSize ();
    addDockWidget (Qt::RightDockWidgetArea, controlDockWidget);
    controlDockWidget->setFeatures (QDockWidget::AllDockWidgetFeatures);

    setMinimumWidth (800);
    setMinimumHeight (400);
}

Window::~Window () {

}

void Window::renderRayImage () {
    qglviewer::Camera * cam = viewer->camera ();
    rayTracer = RayTracer::getInstance ();

    qglviewer::Vec p = cam->position ();
    qglviewer::Vec d = cam->viewDirection ();
    qglviewer::Vec u = cam->upVector ();
    qglviewer::Vec r = cam->rightVector ();
    Vec3Df camPos (p[0], p[1], p[2]);
    Vec3Df viewDirection (d[0], d[1], d[2]);
    Vec3Df upVector (u[0], u[1], u[2]);
    Vec3Df rightVector (r[0], r[1], r[2]);
    float fieldOfView = cam->horizontalFieldOfView ();
    float aspectRatio = cam->aspectRatio ();
    unsigned int screenWidth = cam->screenWidth ();
    unsigned int screenHeight = cam->screenHeight ();
    rayImage = rayTracer->render (camPos, viewDirection, upVector, rightVector,
                                  fieldOfView, aspectRatio, screenWidth, screenHeight);
    imageLabel->setPixmap (QPixmap::fromImage (rayImage));

}

void Window::calculAO(){
    //this->rayTracer = RayTracer::getInstance ();
    //getRayTracer()->scene->calculAmbientOcclusion();
}
void Window::setBGColor () {
    QColor c = QColorDialog::getColor (QColor (133, 152, 181), this);
    if (c.isValid () == true) {
        cout << c.red () << endl;
        RayTracer::getInstance ()->setBackgroundColor (Vec3Df (c.red (), c.green (), c.blue ()));
        viewer->setBackgroundColor (c);
        viewer->updateGL ();
    }
}

void Window::setShadowMode(bool mode){
    RayTracer::getInstance()->setShadowMode(mode);
}

void Window::setAmbientOcclusion(bool ao){
    RayTracer::getInstance()->setAmbientOcclusion(ao);
}

void Window::setAntiAliasing(bool aa){
    RayTracer::getInstance()->setAntiAliasing(aa);
}

void Window::setAliasingMode(int mode){
    RayTracer::getInstance()->setAliasingMode(mode);
}

void Window::setRayPerLight(int rpl){
    RayTracer::getInstance()->setRayPerLight(rpl);
}

void Window::setNumDir(int numdir){
    RayTracer::getInstance()->setNumDir(numdir);
}


void Window::exportGLImage () {
    viewer->saveSnapshot (false, false);
}

void Window::exportRayImage () {
    QString filename = QFileDialog::getSaveFileName (this,
                                                     "Save ray-traced image",
                                                     ".",
                                                     "*.jpg *.bmp *.png");
    if (!filename.isNull () && !filename.isEmpty ()) {
        rayImage.save (filename);
    }
}

void Window::changeOFFFile () {
    QString filename = QFileDialog::getOpenFileName(this,
                                                    "Open OFF File",
                                                    ".",
                                                    "*.off");
    if (!filename.isNull () && !filename.isEmpty ()) {
        cout << "using " << filename.toStdString() << endl;
        Scene * scene = Scene::getInstance();
        scene->destroyInstance();
        scene->getInstance();
        scene->setOFFFilename(filename);
        // viewer->initializeGL();
    }
}

void Window::about () {
    QMessageBox::about (this,
                        "About This Program",
                        "<b>RayMini</b> <br> by <i>Tamy Boubekeur</i>.");
}

void Window::initControlWidget () {
    controlWidget = new QGroupBox ();
    QVBoxLayout * layout = new QVBoxLayout (controlWidget);

    // Preview Group Box
    QGroupBox * previewGroupBox = new QGroupBox ("Preview", controlWidget);
    QVBoxLayout * previewLayout = new QVBoxLayout (previewGroupBox);

    // Scene * scene = Scene::getInstance();
    // QLabel * meshLabelTitle = new QLabel("OFF File : " + scene->getOFFFilename(), controlWidget);
    // QPushButton * changeOFFButton  = new QPushButton ("Choose model", previewGroupBox);
    // connect (changeOFFButton, SIGNAL (clicked ()) , this, SLOT (changeOFFFile ()));
    // previewLayout->addWidget (meshLabelTitle);
    // previewLayout->addWidget (changeOFFButton);

    QCheckBox * wireframeCheckBox = new QCheckBox ("Wireframe", previewGroupBox);
    connect (wireframeCheckBox, SIGNAL (toggled (bool)), viewer, SLOT (setWireframe (bool)));
    previewLayout->addWidget (wireframeCheckBox);

    QButtonGroup * modeButtonGroup = new QButtonGroup (previewGroupBox);
    modeButtonGroup->setExclusive (true);
    QRadioButton * flatButton = new QRadioButton ("Flat", previewGroupBox);
    QRadioButton * smoothButton = new QRadioButton ("Smooth", previewGroupBox);
    modeButtonGroup->addButton (flatButton, static_cast<int>(GLViewer::Flat));
    modeButtonGroup->addButton (smoothButton, static_cast<int>(GLViewer::Smooth));
    connect (modeButtonGroup, SIGNAL (buttonClicked (int)), viewer, SLOT (setRenderingMode (int)));
    previewLayout->addWidget (flatButton);
    previewLayout->addWidget (smoothButton);

    QPushButton * snapshotButton  = new QPushButton ("Save preview", previewGroupBox);
    connect (snapshotButton, SIGNAL (clicked ()) , this, SLOT (exportGLImage ()));
    previewLayout->addWidget (snapshotButton);

    layout->addWidget (previewGroupBox);

    // QGroupBox * rayGroupBox = new QGroupBox ("Ray Tracing", controlWidget);
    // QVBoxLayout * rayLayout = new QVBoxLayout (rayGroupBox);
    // QPushButton * rayButton = new QPushButton ("Render", rayGroupBox);
    // rayLayout->addWidget (rayButton);
    // connect (rayButton, SIGNAL (clicked ()), this, SLOT (renderRayImage ()));

    // KD-Trees Group Box
    QGroupBox * treeGroupBox = new QGroupBox ("Data Structure", controlWidget);
    QVBoxLayout * treeLayout = new QVBoxLayout (treeGroupBox);

    QLabel * kdTreeLabel = new QLabel(QString("KD-tree level:"), treeGroupBox);
    treeLayout->addWidget(kdTreeLabel);
    QSlider * kdTreeSlider = new QSlider(Qt::Horizontal, treeGroupBox);
    kdTreeSlider->setRange(0, 30);
    kdTreeSlider->setSliderPosition(1);
    kdTreeSlider->setValue(1);
    connect(kdTreeSlider, SIGNAL(valueChanged (int)), viewer, SLOT (setKDTreeDepth (int)));
    treeLayout->addWidget(kdTreeSlider);

    layout->addWidget (treeGroupBox);

//    QCheckBox * shadowMode = new QCheckBox ("Soft Shadow", rayGroupBox);
//    QLabel *rayLabel = new QLabel("Ray Per Light", rayGroupBox);

//    //Soft Shadows
//    QSpinBox * rayPerLight = new QSpinBox(rayGroupBox);
//    rayPerLight->setValue(10);
//    rayPerLight->setMinimum(10);
//    rayPerLight->setMaximum(100);
//    connect(rayPerLight, SIGNAL (valueChanged(int)), this, SLOT (setRayPerLight(int)));

//    rayLabel->setVisible(false);
//    rayPerLight->setVisible(false);

//    connect (shadowMode, SIGNAL (toggled (bool)), this, SLOT (setShadowMode(bool)));
//    connect (shadowMode, SIGNAL (toggled (bool)), rayPerLight, SLOT (setVisible(bool)));
//    connect (shadowMode, SIGNAL (toggled (bool)), rayLabel, SLOT (setVisible(bool)));

//    rayLayout->addWidget (shadowMode);
//    rayLayout->addWidget (rayLabel);
//    rayLayout->addWidget (rayPerLight);
//    //

//    QCheckBox * ambientOcclusion = new QCheckBox ("AmbientOcclusion", rayGroupBox);
//    connect (ambientOcclusion, SIGNAL (toggled (bool)), this, SLOT (setAmbientOcclusion(bool)));
//    rayLayout->addWidget (ambientOcclusion);

//    //AntiAliasing
//    QCheckBox * antialias = new QCheckBox ("AntiAliasing", rayGroupBox);
//    QButtonGroup *mode = new QButtonGroup(rayGroupBox);
//    mode->setExclusive(true);

//    QRadioButton *modeRegular = new QRadioButton("Regular", rayGroupBox);
//    QRadioButton *modeStoch = new QRadioButton("Stochastic", rayGroupBox);

//    modeRegular->setVisible(false);
//    modeStoch->setVisible(false);

//    connect(mode, SIGNAL(buttonPressed(int)), this, SLOT(setAliasingMode(int)));

//    mode->addButton(modeRegular,0);
//    mode->addButton(modeStoch, 1);

//    QLabel *numLabel = new QLabel("Directions", rayGroupBox);
//    numLabel->setVisible(false);

//    QSpinBox * numDir = new QSpinBox(rayGroupBox);
//    numDir->setVisible(false);
//    numDir->setValue(10);
//    numDir->setMinimum(10);
//    numDir->setMaximum(100);

//    connect(numDir, SIGNAL (valueChanged(int)), this, SLOT (setNumDir(int)));
//    connect(antialias, SIGNAL(toggled(bool)), numDir, SLOT (setVisible(bool)));
//    connect(antialias, SIGNAL(toggled(bool)), numLabel, SLOT (setVisible(bool)));
//    connect (antialias, SIGNAL (toggled (bool)), this, SLOT (setAntiAliasing(bool)));
//    connect (antialias, SIGNAL (toggled (bool)), modeRegular, SLOT (setVisible(bool)));
//    connect (antialias, SIGNAL (toggled (bool)), modeStoch, SLOT (setVisible(bool)));


//    rayLayout->addWidget (antialias);
//    rayLayout->addWidget (modeRegular);
//    rayLayout->addWidget (modeStoch);
//    rayLayout->addWidget (numLabel);
//    rayLayout->addWidget (numDir);

//    //
//    QPushButton * saveButton  = new QPushButton ("Save", rayGroupBox);
//    connect (saveButton, SIGNAL (clicked ()) , this, SLOT (exportRayImage ()));
//    rayLayout->addWidget (saveButton);

//    layout->addWidget (rayGroupBox);

//    QGroupBox * globalGroupBox = new QGroupBox ("Global Settings", controlWidget);
//    QVBoxLayout * globalLayout = new QVBoxLayout (globalGroupBox);

//    QPushButton * bgColorButton  = new QPushButton ("Background Color", globalGroupBox);
//    connect (bgColorButton, SIGNAL (clicked()) , this, SLOT (setBGColor()));
//    globalLayout->addWidget (bgColorButton);

//    QPushButton * aboutButton  = new QPushButton ("About", globalGroupBox);
//    connect (aboutButton, SIGNAL (clicked()) , this, SLOT (about()));
//    globalLayout->addWidget (aboutButton);

//    QPushButton * quitButton  = new QPushButton ("Quit", globalGroupBox);
//    connect (quitButton, SIGNAL (clicked()) , qApp, SLOT (closeAllWindows()));
//    globalLayout->addWidget (quitButton);

//    layout->addWidget (globalGroupBox);
    layout->addStretch (0);
}
