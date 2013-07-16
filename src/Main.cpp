#include <QApplication>
#include <QDir>
#include <QPixmap>
#include <QSplashScreen>
#include <string>
#include <iostream>

#include "QTUtils.h"
#include "Window.h"

using namespace std;

int main (int argc, char **argv)
{
    cout << "Raymini: Entering main..." << endl;
    QApplication raymini (argc, argv);
    setBoubekQTStyle (raymini);
    cout << "Raymini: Creates main window..." << endl;
    Window * window = new Window ();
//    window->setWindowTitle ("RayMini: A minimal image synthesizer based on raytracing.");
//    window->showMaximized ();
//    window->show();
    raymini.setStyle("fusion");
    raymini.connect (&raymini, SIGNAL (lastWindowClosed()), &raymini, SLOT (quit()));

    return raymini.exec ();
}

