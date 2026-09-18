#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QCommandLineParser>
#include <QRegularExpression>
#include <QTimer>
#include <iostream>
#include "atlas.h"
#include "clock.h"

int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    app.setApplicationName("Meridian");
    app.setDesktopFileName("org.meridian.preview");
    QCommandLineParser parser;parser.setApplicationDescription("Meridian — an offline vintage solar atlas preview");parser.addHelpOption();
    parser.addOption({"fullscreen","Fill the current display (Escape to close)."});
    parser.addOption({"at","Preview an ISO-8601 instant with Z or explicit UTC offset.","instant"});
    parser.addOption({"snapshot","Save a PNG and exit; use QT_QPA_PLATFORM=offscreen for headless rendering.","file"});
    parser.addOption({"size","Window or snapshot size, e.g. 5120x1440. Defaults to fit display.","WxH"});
    parser.process(app);
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
    QQmlApplicationEngine engine;
    QObject::connect(&engine,&QQmlEngine::warnings,[](const QList<QQmlError> &errors){for(const auto &error:errors)std::cerr<<error.toString().toStdString()<<'\n';});
    engine.rootContext()->setContextProperty("clockModel",&clock);
    engine.rootContext()->setContextProperty("appCaptureMode",parser.isSet("snapshot"));
    engine.rootContext()->setContextProperty("appSaverMode",false);
    engine.load(QUrl("qrc:/qml/Main.qml"));
    if(engine.rootObjects().isEmpty())return 1;
    auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if(!window)return 1;
    window->resize(size);
    if(parser.isSet("fullscreen") && !parser.isSet("snapshot"))window->showFullScreen();
    if(parser.isSet("snapshot"))QTimer::singleShot(600,&app,[&]{auto image=window->grabWindow();if(image.isNull() || !image.save(parser.value("snapshot"))){qCritical("Snapshot failed");app.exit(1);}else app.quit();});
    return app.exec();
}
