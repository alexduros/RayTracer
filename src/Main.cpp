#include <QApplication>
#include <Window.h>
#include <QDir>
#include <QPixmap>
#include <QSplashScreen>
#include <string>
#include <iostream>

#include "QTUtils.h"

using namespace std;

int main (int argc, char **argv)
{
    QApplication raymini (argc, argv);
    setBoubekQTStyle (raymini);
    Window * window = new Window ();
    window->setWindowTitle ("RayMini: A minimal image synthesizer based on raytracing.");
    window->showMaximized ();
    window->show();
    raymini.setStyle("fusion");
    raymini.connect (&raymini, SIGNAL (lastWindowClosed()), &raymini, SLOT (quit()));

    return raymini.exec ();
}

