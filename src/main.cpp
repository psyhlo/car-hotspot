#include <sailfishapp.h>
#include <QtQuick>
#include <QGuiApplication>
#include <QCoreApplication>
#include "appcontroller.h"

int main(int argc, char *argv[])
{
    bool isDaemon = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--daemon" || QString(argv[i]) == "-d") {
            isDaemon = true;
            break;
        }
    }

    if (isDaemon) {
        // Headless background daemon mode
        QCoreApplication app(argc, argv);
        AppController controller;
        return app.exec();
    }

    // Normal GUI mode
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));

    // Install translator according to system locale
    QScopedPointer<QTranslator> translator(new QTranslator);
    QString localeName = QLocale::system().name(); // e.g. "bg_BG" or "bg"
    QString langCode = localeName.split('_').first(); // e.g. "bg"

    // Look in standard app translation directories
    QStringList translationDirs;
    translationDirs << "/usr/share/harbour-carhotspot/translations"
                    << "/usr/share/translations"
                    << SailfishApp::pathTo("translations").toLocalFile();

    bool loaded = false;
    for (const QString &dir : translationDirs) {
        if (translator->load(QString("harbour-carhotspot-%1").arg(localeName), dir) ||
            translator->load(QString("harbour-carhotspot-%1").arg(langCode), dir)) {
            app->installTranslator(translator.data());
            loaded = true;
            break;
        }
    }

    QScopedPointer<QQuickView> view(SailfishApp::createView());

    AppController controller;
    view->rootContext()->setContextProperty("appVersion", "0.1.28");
    view->rootContext()->setContextProperty("appController", &controller);
    view->rootContext()->setContextProperty("bluetoothManager", controller.bluetooth());
    view->rootContext()->setContextProperty("hotspotManager", controller.hotspot());
    view->rootContext()->setContextProperty("systemMonitor", controller.systemMonitor());

    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    return app->exec();
}
