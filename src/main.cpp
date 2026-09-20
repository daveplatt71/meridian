#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCoreApplication>
#include <QProcess>
#include <QQuickWindow>
#include <QScreen>
#include <QCommandLineParser>
#include <QRegularExpression>
#include <QTimer>
#include <iostream>
#include "atlas.h"
#include "clock.h"
#ifdef OMARIDIAN_WITH_LAYER_SHELL
#include "layer_shell.h"
#endif

namespace {

class ScreensaverDismissal final : public QObject {
    Q_OBJECT
public slots:
    void dismiss() {
        // The stock Omarchy lock/dismiss path identifies the screensaver by
        // this stable string. The launcher also keeps it in argv so pkill -f
        // reaches direct Omaridian processes, not only terminal wrappers.
        QProcess::startDetached(QStringLiteral("pkill"), {
            QStringLiteral("-f"), QStringLiteral("[o]rg.omarchy.screensaver")});
        QCoreApplication::quit();
    }
};

bool hasArgument(int argc, char **argv, const char *wanted) {
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QLatin1String(wanted)) return true;
    }
    return false;
}

}

#include "main.moc"

int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    const bool requestedSaver = hasArgument(argc, argv, "--screensaver");
    app.setApplicationName(requestedSaver ? "org.omarchy.screensaver" : "Omaridian");
    app.setDesktopFileName(requestedSaver ? "org.omarchy.screensaver" : "org.omaridian.preview");
    QCommandLineParser parser;parser.setApplicationDescription("Omaridian — an offline vintage solar atlas preview");parser.addHelpOption();
    parser.addOption({"fullscreen","Fill the current display (Escape to close)."});
    parser.addOption({"screensaver","Open the live atlas as a dismissible fullscreen screensaver."});
    parser.addOption({"wallpaper","Render the live atlas as a fullscreen wallpaper preview (not a desktop wallpaper)."});
    parser.addOption({"wallpaper-layer","Run the experimental one-output solid-color Wayland layer-shell proof."});
    QCommandLineOption appIdOption("app-id", "Set the Wayland application id (screensaver integration uses org.omarchy.screensaver).", "id");
    appIdOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(appIdOption);
    QCommandLineOption adapterOption("omarchy-adapter", "Mark a direct Omarchy screensaver launch.");
    adapterOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(adapterOption);
    parser.addOption({"monitor","Select the named monitor for a direct screensaver window.","name"});
    parser.addOption({"at","Preview an ISO-8601 instant with Z or explicit UTC offset.","instant"});
    parser.addOption({"snapshot","Save a PNG and exit; use QT_QPA_PLATFORM=offscreen for headless rendering.","file"});
    parser.addOption({"size","Window or snapshot size, e.g. 5120x1440. Defaults to fit display.","WxH"});
    parser.process(app);
    if (parser.isSet("app-id")) {
        const QString appId = parser.value("app-id");
        if (!parser.isSet("screensaver") || appId != QLatin1String("org.omarchy.screensaver")) {
            qCritical("--app-id is only supported as org.omarchy.screensaver with --screensaver");
            return 2;
        }
        app.setApplicationName(appId);
        app.setDesktopFileName(appId);
    }
    if (parser.isSet("monitor") && !parser.isSet("screensaver")) {
        qCritical("--monitor is only supported with --screensaver");
        return 2;
    }
    const bool layerMode = parser.isSet("wallpaper-layer");
    if(layerMode) {
        if(parser.isSet("screensaver") || parser.isSet("wallpaper") || parser.isSet("fullscreen")) {
            qCritical("--wallpaper-layer cannot be combined with --fullscreen, --screensaver, or --wallpaper");
            return 2;
        }
#ifdef OMARIDIAN_WITH_LAYER_SHELL
        // The layer mode is entered after the shared clock and AtlasMap type
        // are initialized below.
#else
        qCritical("--wallpaper-layer is unavailable in this build; configure with -DOMARIDIAN_WITH_LAYER_SHELL=ON");
        return 2;
#endif
    }
    if(parser.isSet("screensaver") && parser.isSet("wallpaper")) {qCritical("--screensaver and --wallpaper are mutually exclusive");return 2;}
    QDateTime fixed;
    if(parser.isSet("at")) {
        QString text=parser.value("at");fixed=QDateTime::fromString(text,Qt::ISODate);
        if(!fixed.isValid() || !QRegularExpression("(Z|[+-]\\d{2}:\\d{2})$").match(text).hasMatch()) {qCritical("--at requires a valid ISO instant with Z or UTC offset");return 2;}
        fixed=fixed.toUTC();
    }
    QSize size;
    if(parser.isSet("size")) {
        auto match=QRegularExpression("^(\\d+)x(\\d+)$").match(parser.value("size"));
        size=QSize(match.captured(1).toInt(),match.captured(2).toInt());
        if(!match.hasMatch() || size.width()<640 || size.height()<360 || size.width()>10000 || size.height()>10000){qCritical("--size must be between 640x360 and 10000x10000");return 2;}
    } else {auto available=app.primaryScreen()->availableGeometry().size();size=QSize(std::min(1600,int(available.width()*.9)),std::min(800,int(available.height()*.85)));}
    qmlRegisterType<AtlasMap>("VintageAtlas",1,0,"AtlasMap");
    Clock clock(fixed);
#ifdef OMARIDIAN_WITH_LAYER_SHELL
    if(layerMode)return runLayerShellProof(app,clock);
#endif
    QQmlApplicationEngine engine;
    QObject::connect(&engine,&QQmlEngine::warnings,[](const QList<QQmlError> &errors){for(const auto &error:errors)std::cerr<<error.toString().toStdString()<<'\n';});
    engine.rootContext()->setContextProperty("clockModel",&clock);
    engine.rootContext()->setContextProperty("appCaptureMode",parser.isSet("snapshot"));
    engine.rootContext()->setContextProperty("appSaverMode",parser.isSet("screensaver"));
    engine.rootContext()->setContextProperty("appWallpaperMode",parser.isSet("wallpaper"));
    engine.rootContext()->setContextProperty("appWindowVisible", false);
    engine.load(QUrl("qrc:/qml/Main.qml"));
    if(engine.rootObjects().isEmpty())return 1;
    auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if(!window)return 1;
    if (parser.isSet("monitor")) {
        const QString requestedMonitor = parser.value("monitor");
        QScreen *selected = nullptr;
        for (QScreen *screen : app.screens()) {
            if (screen->name() == requestedMonitor) {
                selected = screen;
                break;
            }
        }
        if (!selected) {
            qCritical().noquote() << "Monitor not found:" << requestedMonitor;
            return 3;
        }
        window->setScreen(selected);
    }
    window->resize(size);
    if (parser.isSet("screensaver")) {
        auto *scene = window->findChild<QObject *>(QStringLiteral("omaridianScene"));
        static ScreensaverDismissal dismissal;
        if (scene) QObject::connect(scene, SIGNAL(closeRequested()), &dismissal, SLOT(dismiss()), Qt::UniqueConnection);
    }
    window->setProperty("visible", true);
    if((parser.isSet("fullscreen") || parser.isSet("screensaver") || parser.isSet("wallpaper")) && !parser.isSet("snapshot"))window->showFullScreen();
    if(parser.isSet("snapshot"))QTimer::singleShot(600,&app,[&]{auto image=window->grabWindow();if(image.isNull() || !image.save(parser.value("snapshot"))){qCritical("Snapshot failed");app.exit(1);}else app.quit();});
    return app.exec();
}
