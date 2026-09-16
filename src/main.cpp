#include <sailfishapp.h>
#include <QtQuick>
#include <QGuiApplication>
#include "appcontroller.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    QScopedPointer<QQuickView> view(SailfishApp::createView());

    AppController controller;
    view->rootContext()->setContextProperty("appController", &controller);
    view->rootContext()->setContextProperty("bluetoothManager", controller.bluetooth());
    view->rootContext()->setContextProperty("hotspotManager", controller.hotspot());

    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    return app->exec();
}
