#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QImage>
#include <QPainter>
#include <QTest>
#include <iostream>
#include "atlas.h"
#include "clock.h"
#include "projection.h"

namespace {

struct GeoPoint { double longitude; double latitude; };

QImage renderAtlas(AtlasMap &atlas, bool equalArea) {
    atlas.setEqualArea(equalArea);
    QImage image(1200, 620, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    atlas.paint(&painter);
    return image;
}

QPoint mapPoint(const GeoPoint point, bool equalArea) {
    constexpr int width = 1200;
    constexpr int height = 620;
    constexpr int band = 28;
    return QPoint(qRound((point.longitude + 180.0) * width / 360.0),
                  band + qRound(Projection::projectY(point.latitude, equalArea) * (height - band)));
}

bool isWarmLand(const QImage &image, const GeoPoint point, bool equalArea) {
    const QPoint pixel = mapPoint(point, equalArea);
    return pixel.x() >= 0 && pixel.x() < image.width() && pixel.y() >= 0 && pixel.y() < image.height()
        && QColor(image.pixel(pixel)).red() > QColor(image.pixel(pixel)).blue() + 15;
}

}

int main(int argc,char **argv){
    QGuiApplication app(argc,argv);
    qmlRegisterType<AtlasMap>("VintageAtlas",1,0,"AtlasMap");
    Clock clock(QDateTime::fromString("1969-12-31T23:59:00Z",Qt::ISODate));
    QQmlApplicationEngine engine;
    bool warnings=false;
    QObject::connect(&engine,&QQmlEngine::warnings,[&](const QList<QQmlError> &errors){warnings=true;for(auto error:errors)std::cerr<<error.toString().toStdString()<<'\n';});
    engine.rootContext()->setContextProperty("clockModel",&clock);
    engine.rootContext()->setContextProperty("appCaptureMode",true);
    engine.rootContext()->setContextProperty("appSaverMode",false);
    engine.rootContext()->setContextProperty("appWallpaperMode",false);
    engine.load(QUrl("qrc:/qml/Main.qml"));
    if(engine.rootObjects().isEmpty())return 1;
    auto window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    auto map=window->findChild<AtlasMap*>("atlasMap");
    if(!map)return 2;

    // These points are deliberately away from labels, grid lines, and coastlines.
    // Brazil is a mainland control; Java, Sumatra, and Sulawesi are separate
    // parts of Indonesia's MultiPolygon.
    const GeoPoint mainlandAndIslands[] = {
        {-54.5, -10.2}, {110.4, -7.3}, {101.2, -0.5}, {121.2, -1.5}
    };
    AtlasMap renderedAtlas;
    renderedAtlas.setSize(QSizeF(1200, 620));
    renderedAtlas.setHourBandHeight(28.0);
    for (bool equalArea : {false, true, false}) {
        const QImage image = renderAtlas(renderedAtlas, equalArea);
        for (const auto point : mainlandAndIslands) {
            if (!isWarmLand(image, point, equalArea)) {
                std::cerr << "Known mainland/island coverage missing in "
                          << (equalArea ? "equal-area" : "equirectangular") << " projection\n";
                return 9;
            }
        }
        // Relief must be clipped to land after every projection switch: this
        // open-ocean point must retain the map fill.
        const QPoint oceanPixel = mapPoint({157.0, 5.0}, equalArea);
        if (QColor(image.pixel(oceanPixel)).rgb() != QColor("#b0c3be").rgb()) {
            std::cerr << "Relief tinted open ocean in "
                      << (equalArea ? "equal-area" : "equirectangular") << " projection\n";
            return 10;
        }
    }

    for(auto size:{QSize(5120,1440),QSize(1600,900),QSize(640,360),QSize(2048,576),QSize(1600,900)}){
        window->resize(size);QTest::qWait(50);
        QRectF rect=map->mapRectToScene(map->boundingRect());
        bool wide=size.width()/double(size.height())>2.6;
        double expectedAspect=wide?3.4:2.0;
        double geographicalHeight=map->height()-map->property("hourBandHeight").toDouble();
        if(map->property("equalArea").toBool()!=wide || std::abs(map->width()/geographicalHeight-expectedAspect)>0.001 || rect.left()<0 || rect.right()>size.width()+1 || rect.top()<0 || rect.bottom()>size.height()+1){std::cerr<<"Map outside viewport or wrong projection aspect\n";return 3;}
        if(std::abs(rect.center().x()-size.width()/2.0)>1){std::cerr<<"Map not horizontally centred\n";return 7;}
        double coverage=rect.width()*geographicalHeight/(size.width()*double(size.height()));
        if(coverage<(size.width()==5120?0.75:0.60)){std::cerr<<"Map does not fill enough screen: "<<coverage<<'\n';return 8;}
        std::cout<<size.width()<<'x'<<size.height()<<" geographic coverage: "<<coverage*100<<"%\n";
        auto image=window->grabWindow();
        if(image.isNull()){std::cerr<<"Empty render\n";return 4;}
    }

    QTest::keyClick(window,Qt::Key_Escape);QTest::qWait(20);
    if(window->isVisible()){std::cerr<<"Escape did not close preview\n";return 5;}

    // Load the wallpaper renderer in a separate offscreen engine.  This
    // exercises the mode-specific geometry while keeping the preview engine
    // and its framed layout independent and deterministic.
    QQmlApplicationEngine wallpaperEngine;
    wallpaperEngine.rootContext()->setContextProperty("clockModel",&clock);
    wallpaperEngine.rootContext()->setContextProperty("appCaptureMode",true);
    wallpaperEngine.rootContext()->setContextProperty("appSaverMode",false);
    wallpaperEngine.rootContext()->setContextProperty("appWallpaperMode",true);
    wallpaperEngine.load(QUrl("qrc:/qml/Main.qml"));
    if(wallpaperEngine.rootObjects().isEmpty()){std::cerr<<"Wallpaper mode failed to load\n";return 11;}
    auto wallpaperWindow=qobject_cast<QQuickWindow*>(wallpaperEngine.rootObjects().first());
    auto wallpaperMap=wallpaperWindow->findChild<AtlasMap*>("atlasMap");
    auto frame=wallpaperWindow->findChild<QObject*>("pictureFrame");
    if(!wallpaperWindow || !wallpaperMap || !frame || frame->property("visible").toBool()){
        std::cerr<<"Wallpaper mode did not select the clean map surface\n";return 12;
    }
    wallpaperWindow->resize(2048,576);QTest::qWait(50);
    QRectF wallpaperRect=wallpaperMap->mapRectToScene(wallpaperMap->boundingRect());
    if(!wallpaperMap->property("equalArea").toBool() || wallpaperRect != QRectF(0,0,2048,576)){
        std::cerr<<"Wallpaper map does not fill the offscreen viewport\n";return 13;
    }
    if(wallpaperWindow->grabWindow().isNull()){std::cerr<<"Empty wallpaper render\n";return 14;}

    if(warnings)return 6;
    std::cout<<"Responsive projection switches, map coverage and centering pass; Escape closes preview\n";
}
