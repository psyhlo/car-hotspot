#include <sailfishapp.h>
#include <QtQuick>
#include <QtQml>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QTimer>
#include <QDir>
#include <QImage>
#include <QSet>
#include <cstdio>
#include "appcontroller.h"
#include "carhotspotsettings.h"
#include "hotspotmanager.h"

static void installAppTranslator(QCoreApplication *app, bool isDaemon)
{
    QTranslator *translator = new QTranslator(app);
    QString localeName = QLocale::system().name(); // e.g. "bg_BG" or "bg"
    QString langCode = localeName.split('_').first(); // e.g. "bg"

    // Look in standard app translation directories
    QStringList translationDirs;
    translationDirs << "/usr/share/harbour-carhotspot/translations"
                    << "/usr/share/translations";
    if (!isDaemon) {
        translationDirs << SailfishApp::pathTo("translations").toLocalFile();
    }

    for (const QString &dir : translationDirs) {
        if (translator->load(QString("harbour-carhotspot-%1").arg(localeName), dir) ||
            translator->load(QString("harbour-carhotspot-%1").arg(langCode), dir)) {
            app->installTranslator(translator);
            return;
        }
    }

    delete translator;
}

int main(int argc, char *argv[])
{
    bool isDaemon = false;
    bool selfTest = false;
    for (int i = 1; i < argc; ++i) {
        const QString arg(argv[i]);
        if (arg == "--daemon" || arg == "-d") {
            isDaemon = true;
        } else if (arg == "--selftest") {
            selfTest = true;
        }
    }

    if (isDaemon || selfTest) {
        // Headless background daemon mode
        QCoreApplication app(argc, argv);
        QCoreApplication::setOrganizationName("harbour-carhotspot");
        QCoreApplication::setApplicationName("harbour-carhotspot");
        installAppTranslator(&app, true);

        if (selfTest) {
            // Device self test: run the real tethering code path from inside the sandbox
            // and report what ConnMan actually did. Nothing privileged, no shell:
            //   sailjail -p harbour-carhotspot-daemon.desktop -- /usr/bin/harbour-carhotspot --selftest
            HotspotManager hotspot;
            QTimer::singleShot(800, &hotspot, [&hotspot]() { hotspot.setHotspotActive(true); });
            QTimer::singleShot(12000, &hotspot, [&hotspot]() {
                fprintf(stdout, "SELFTEST tethering=%d status=%s\n",
                        hotspot.isHotspotActive() ? 1 : 0,
                        hotspot.statusMessage().toUtf8().constData());
                fflush(stdout);
                hotspot.setHotspotActive(false);
            });
            QTimer::singleShot(20000, &app, [&hotspot, &app]() {
                fprintf(stdout, "SELFTEST restored tethering=%d status=%s\n",
                        hotspot.isHotspotActive() ? 1 : 0,
                        hotspot.statusMessage().toUtf8().constData());
                fflush(stdout);
                app.quit();
            });
            return app.exec();
        }

        AppController controller(true);
        return app.exec();
    }

    // Normal GUI mode
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    QGuiApplication::setOrganizationName("harbour-carhotspot");
    QGuiApplication::setApplicationName("harbour-carhotspot");

    // Install translator according to system locale
    installAppTranslator(app.data(), false);

    QScopedPointer<QQuickView> view(SailfishApp::createView());

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

#ifndef APP_VERSION
#define APP_VERSION 0.1.0
#endif

    AppController controller;
    QObject::connect(&controller, &AppController::requestActivateWindow, [&view]() {
        if (view) {
            view->raise();
            view->requestActivate();
        }
    });

    view->rootContext()->setContextProperty("appVersion", QStringLiteral(STRINGIFY(APP_VERSION)));
    view->rootContext()->setContextProperty("appController", &controller);
    view->rootContext()->setContextProperty("bluetoothManager", controller.bluetooth());
    view->rootContext()->setContextProperty("hotspotManager", controller.hotspot());
    view->rootContext()->setContextProperty("systemMonitor", controller.systemMonitor());

    view->setSource(SailfishApp::pathToMainQml());
    // A QML syntax error leaves an empty window on screen, which is very hard to notice on
    // a device - report it on stderr instead (visible when the app is started from a shell).
    for (const QQmlError &err : view->errors()) {
        fprintf(stderr, "QML error: %s\n", err.toString().toLocal8Bit().constData());
        fflush(stderr);
    }
    view->show();

    // ---- Development aid for store screenshots ------------------------------------------
    // Nothing here is reachable from the UI, the D-Bus surface or a normal launch. Two entry
    // points:
    //   1. CARHOTSPOT_SHOTDIR=<dir>  - for an instance started from a shell.
    //   2. Settings keys: devShotEnabled=1 while the app starts, then devShotTrigger=<n> and
    //      devShotDir=<dir> written from outside. Only a launcher-started app is really
    //      exposed on screen, and only then does grabWindow() return content instead of a
    //      blank image - which is what store screenshots need.
    // Progress is mirrored into the devShotStatus key, so the run can be checked over ssh
    // without a shell. Steps (devShotStep in harbour-carhotspot.qml): main page top, main
    // page scrolled to the end, diagnostics page top, diagnostics page scrolled to end.
    QSettings *shotCfg = new QSettings(carhotspotSettingsFilePath(), QSettings::IniFormat);
    const QString envShotDir = QString::fromLocal8Bit(qgetenv("CARHOTSPOT_SHOTDIR"));
    const bool watchShotTrigger = shotCfg->value("devShotEnabled", 0).toInt() != 0;
    if (view && (!envShotDir.isEmpty() || watchShotTrigger)) {
        QQuickView *window = view.data();
        QObject *root = window->rootObject();
        // Sailfish redirects QML console.log to the journal; keep it on stdout in this mode.
        qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &msg) {
            fprintf(stdout, "QML: %s\n", msg.toLocal8Bit().constData());
            fflush(stdout);
        });

        const QString sizeEnv = QString::fromLocal8Bit(qgetenv("CARHOTSPOT_SHOTSIZE"));
        const QStringList sizeParts = sizeEnv.split('x');
        if (sizeParts.size() == 2) {
            const int w = sizeParts.at(0).toInt();
            const int h = sizeParts.at(1).toInt();
            if (w > 100 && h > 100) {
                window->resize(w, h);
            }
        }

        auto writeStatus = [shotCfg](const QString &line) {
            const QString previous = shotCfg->value("devShotStatus", QString()).toString();
            shotCfg->setValue("devShotStatus", previous + line + QStringLiteral("\n"));
            shotCfg->sync();
        };

        auto saveShot = [window, shotCfg](const QString &dir, int step) {
            const QImage image = window->grabWindow();
            // A grab with a single colour means the surface was never exposed on screen.
            QSet<QRgb> colors;
            for (int y = 0; y < image.height() && colors.size() < 40; y += 48) {
                for (int x = 0; x < image.width() && colors.size() < 40; x += 48) {
                    colors.insert(image.pixel(x, y));
                }
            }
            const QString path = dir + QStringLiteral("/shot-%1.png").arg(step + 1);
            const bool ok = image.save(path, "PNG");
            const QString line = QStringLiteral("shot-%1 %2x%3 colors=%4 %5")
                                     .arg(step + 1).arg(image.width()).arg(image.height())
                                     .arg(colors.size()).arg(ok ? "ok" : "FAILED");
            fprintf(stdout, "SHOT %s\n", line.toLocal8Bit().constData());
            fflush(stdout);
            const QString previous = shotCfg->value("devShotStatus", QString()).toString();
            shotCfg->setValue("devShotStatus", previous + line + QStringLiteral("\n"));
            shotCfg->sync();
        };

        auto runShots = [window, root, shotCfg, saveShot](const QString &dir) {
            if (dir.isEmpty() || !QDir().mkpath(dir)) {
                fprintf(stderr, "SHOT bad dir\n");
                return;
            }
            int delay = 1600;
            for (int step = 0; step < 4; ++step) {
                QTimer::singleShot(delay, window, [window, root, dir, step, saveShot]() {
                    if (root) {
                        QMetaObject::invokeMethod(root, "devShotStep", Q_ARG(QVariant, step));
                    }
                    QTimer::singleShot(1400, window, [dir, step, saveShot]() {
                        saveShot(dir, step);
                    });
                });
                delay += 2400;
            }
            QTimer::singleShot(delay + 1500, window, [shotCfg]() {
                shotCfg->setValue("devShotTrigger", shotCfg->value("devShotTrigger", 0).toInt());
                shotCfg->sync();
                qApp->quit();
            });
        };

        if (!envShotDir.isEmpty()) {
            runShots(envShotDir);
        } else {
            int *lastTrigger = new int(shotCfg->value("devShotTrigger", 0).toInt());
            QTimer *poll = new QTimer(window);
            QObject::connect(poll, &QTimer::timeout, poll, [shotCfg, lastTrigger, runShots, writeStatus]() {
                const int trigger = shotCfg->value("devShotTrigger", 0).toInt();
                if (trigger != *lastTrigger) {
                    *lastTrigger = trigger;
                    const QString dir = shotCfg->value("devShotDir", QString()).toString();
                    if (dir.isEmpty()) {
                        writeStatus("devShotDir is empty");
                    } else {
                        writeStatus(QStringLiteral("trigger %1 -> %2").arg(trigger).arg(dir));
                        runShots(dir);
                    }
                }
            });
            poll->start(1000);
        }
    } else {
        delete shotCfg;
        shotCfg = nullptr;
    }

    return app->exec();
}
