#include "atlas.h"
#include "projection.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QColor>
#include <algorithm>

AtlasMap::AtlasMap(QQuickItem *parent):QQuickPaintedItem(parent) {
    setAntialiasing(true);
    QFile file(":/assets/land.geojson");
    if (!file.open(QIODevice::ReadOnly)) qFatal("Bundled atlas is missing");
    const QJsonArray features = QJsonDocument::fromJson(file.readAll()).object()["features"].toArray();
    for (const auto &feature : features) {
        const QJsonObject geometry = feature.toObject()["geometry"].toObject();
        const QJsonArray coordinates = geometry["coordinates"].toArray();
        if (geometry["type"] == "Polygon") polygons_.append(coordinates);
        else if (geometry["type"] == "MultiPolygon")
            for (const auto &polygon : coordinates) polygons_.append(polygon.toArray());
    }
    if (polygons_.isEmpty()) qFatal("Bundled atlas contains no polygons");
    rebuildLand();
    QFile countriesFile(":/assets/countries.geojson");
    if (countriesFile.open(QIODevice::ReadOnly)) {
        const auto features=QJsonDocument::fromJson(countriesFile.readAll()).object()["features"].toArray();
        const QColor palette[]={QColor("#d7b45a"),QColor("#c8863b"),QColor("#9ead6a"),QColor("#e1cc8c"),QColor("#c46b43"),QColor("#8da978"),QColor("#e0a24a")};
        for(const auto &feature:features){
            const auto obj=feature.toObject(); const auto geometry=obj["geometry"].toObject(); const auto coords=geometry["coordinates"].toArray();
            const int color=(obj["properties"].toObject()["MAPCOLOR7"].toInt()-1+7)%7;
            if(geometry["type"]=="Polygon") {
                countryPolygons_.append(coords); countryColors_.append(palette[color]);
            } else if(geometry["type"]=="MultiPolygon") {
                for(const auto &polygon:coords) { countryPolygons_.append(polygon.toArray()); countryColors_.append(palette[color]); }
            }
        }
    }
    rebuildCountries();
    QFile reliefFile(":/assets/relief.png");
    if(reliefFile.open(QIODevice::ReadOnly)) { reliefSource_=QImage::fromData(reliefFile.readAll()); relief_=reliefSource_; }
    rebuildRelief();
}

void AtlasMap::rebuildLand() {
    land_.clear(); landClip_ = QPainterPath(); landClip_.setFillRule(Qt::OddEvenFill);
    for (const auto &rings : polygons_) {
        QPainterPath path; path.setFillRule(Qt::OddEvenFill);
        for (const auto &ringValue : rings) {
            const QJsonArray ring = ringValue.toArray(); bool first = true;
            for (const auto &coordinate : ring) {
                const QJsonArray xy = coordinate.toArray(); if (xy.size() < 2) continue;
                const QPointF point((xy[0].toDouble() + 180.0) / 360.0, Projection::projectY(xy[1].toDouble(), equalArea_));
                if (first) { path.moveTo(point); first = false; } else path.lineTo(point);
            }
            if (!first) path.closeSubpath();
        }
        land_.append(path); landClip_.addPath(path);
    }
}

void AtlasMap::rebuildCountries() {
    countries_.clear(); countryClip_ = QPainterPath(); countryClip_.setFillRule(Qt::OddEvenFill);
    for(const auto &rings:countryPolygons_){
        QPainterPath path; path.setFillRule(Qt::OddEvenFill);
        for(const auto &ringValue:rings){
            const auto ring=ringValue.toArray(); bool first=true;
            for(const auto &coordinate:ring){
                const auto xy=coordinate.toArray(); if(xy.size()<2)continue;
                QPointF point((xy[0].toDouble()+180.0)/360.0,Projection::projectY(xy[1].toDouble(),equalArea_));
                if(first){path.moveTo(point);first=false;}else path.lineTo(point);
            }
            if(!first)path.closeSubpath();
        }
        countries_.append(path); countryClip_.addPath(path);
    }
}

void AtlasMap::rebuildRelief() {
    if(reliefSource_.isNull()) return;
    const auto source=reliefSource_.convertToFormat(QImage::Format_Grayscale8);
    relief_=QImage(source.size(),QImage::Format_Grayscale8);
    for(int y=0;y<relief_.height();++y){
        auto row=relief_.scanLine(y);
        double lat=Projection::latitudeAt((y+0.5)/relief_.height(),equalArea_);
        int sy=qBound(0,int((90-lat)/180*source.height()),source.height()-1);
        for(int x=0;x<relief_.width();++x) row[x]=source.constScanLine(sy)[qBound(0,int(x*source.width()/double(relief_.width())),source.width()-1)];
    }
}

void AtlasMap::setEqualArea(bool value) {
    if (value == equalArea_) return;
    equalArea_ = value; rebuildLand(); rebuildCountries(); rebuildRelief(); shade_ = QImage(); shadeMinute_ = -1;
    if (utc_.isValid()) updateShade(); update(); emit equalAreaChanged();
}

void AtlasMap::setHourBandHeight(double value) {
    value = std::max(1.0, value); if (qFuzzyCompare(value, hourBandHeight_)) return;
    hourBandHeight_ = value; update(); emit hourBandHeightChanged();
}

void AtlasMap::setUtc(QDateTime value) {
    if (!value.isValid() || value == utc_) return; utc_ = value.toUTC();
    if (shade_.isNull() || utc_.toSecsSinceEpoch() / 60 != shadeMinute_) { updateShade(); update(); }
    emit utcChanged();
}

void AtlasMap::updateShade() {
    shadeMinute_ = utc_.toSecsSinceEpoch() / 60; shade_ = QImage(1440, 720, QImage::Format_ARGB32_Premultiplied);
    const auto sun = Solar::position(utc_);
    for (int y = 0; y < shade_.height(); ++y) {
        auto row = reinterpret_cast<QRgb *>(shade_.scanLine(y));
        const double latitude = Projection::latitudeAt((y + 0.5) / shade_.height(), equalArea_);
        for (int x = 0; x < shade_.width(); ++x) {
            const double longitude = (x + 0.5) * 360.0 / shade_.width() - 180.0;
            const double cosine = Solar::cosineZenith(latitude, longitude, sun);
            const double altitude = Solar::deg(std::asin(std::clamp(cosine, -1.0, 1.0)));
            const double darkness = std::clamp(-altitude / 6.0, 0.0, 1.0);
            row[x] = qPremultiply(qRgba(13, 27, 34, qRound(155 * darkness)));
        }
    }
}

void AtlasMap::paint(QPainter *p) {
    p->setRenderHint(QPainter::Antialiasing); const double w = width(), h = height();
    const double band = std::min(hourBandHeight_, std::max(1.0, h - 1.0));
    const QRectF mapRect(0, band, w, std::max(1.0, h - band)); p->fillRect(mapRect, QColor("#b0c3be"));
    auto point = [mapRect, this](double longitude, double latitude) { return QPointF(mapRect.left() + (longitude + 180.0) * mapRect.width() / 360.0, mapRect.top() + Projection::projectY(latitude, equalArea_) * mapRect.height()); };
    p->save(); p->translate(mapRect.topLeft()); p->scale(mapRect.width(), mapRect.height());
    QPen coast(QColor("#70654f")); coast.setWidthF(0); p->setPen(coast); p->setBrush(QColor("#e0d4ad"));
    if(countries_.isEmpty()) for (const auto &path : land_) p->drawPath(path); else for(int i=0;i<countries_.size();++i){p->setBrush(countryColors_[i]);p->drawPath(countries_[i]);}
    p->restore();
    if(!relief_.isNull()){
        p->save(); p->translate(mapRect.topLeft()); p->scale(mapRect.width(), mapRect.height());
        p->setClipPath(countries_.isEmpty() ? landClip_ : countryClip_); p->setOpacity(0.22); p->drawImage(QRectF(0, 0, 1, 1), relief_); p->restore();
    }
    if(!countries_.isEmpty()){
        p->save(); p->translate(mapRect.topLeft()); p->scale(mapRect.width(), mapRect.height());
        p->setBrush(Qt::NoBrush); p->setPen(QPen(QColor("#665541"), 0)); for(const auto &path:countries_) p->drawPath(path); p->restore();
    }
    p->setPen(QPen(QColor(55, 70, 66, 45), 0.7));
    for (int longitude = -180; longitude <= 180; longitude += 15) p->drawLine(point(longitude, -90), point(longitude, 90));
    for (int latitude = -60; latitude <= 60; latitude += 30) p->drawLine(point(-180, latitude), point(180, latitude));
    p->setPen(QPen(QColor(70, 70, 51, 100), 1, Qt::DashLine));
    for (double latitude : {-66.56, -23.44, 0.0, 23.44, 66.56}) p->drawLine(point(-180, latitude), point(180, latitude));
    struct Label { const char *name; double longitude; double latitude; };
    const Label regions[] = {{"N O R T H   A M E R I C A", -106, 47}, {"S O U T H   A M E R I C A", -62, -19}, {"A F R I C A", 20, 4}, {"E U R O P E", 23, 51}, {"A S I A", 91, 43}, {"A U S T R A L I A", 134, -25}, {"P A C I F I C   O C E A N", -139, -10}, {"A T L A N T I C", -30, 17}, {"I N D I A N   O C E A N", 79, -28}};
    QFont font("Noto Serif"); font.setPixelSize(std::max(9, qRound(w / 95))); p->setFont(font); p->setPen(QColor("#4b5144"));
    const double labelHeight = QFontMetricsF(font).lineSpacing() + 4.0;
    for (const auto &label : regions) { const auto pos = point(label.longitude, label.latitude); p->drawText(QRectF(pos.x() - w * .16, pos.y() - labelHeight / 2.0, w * .32, labelHeight), Qt::AlignCenter, QString::fromLatin1(label.name)); }
    if (!shade_.isNull()) { p->setRenderHint(QPainter::SmoothPixmapTransform); p->drawImage(mapRect, shade_); }
    const auto sun = Solar::position(utc_); const auto sunPoint = point(sun.longitude, sun.latitude);
    p->setPen(QPen(QColor("#ffecb0"), 1.5)); p->setBrush(QColor("#dac28a")); p->drawEllipse(sunPoint, 4, 4); p->setBrush(Qt::NoBrush); p->drawEllipse(sunPoint, 9, 9);
    font.setFamily("Noto Sans"); font.setPixelSize(std::max(1, qRound(std::min(w / 115.0, std::max(1.0, (band - 8.0) / 1.4))))); p->setFont(font); p->setPen(QColor("#ded3b1")); p->fillRect(QRectF(0, 0, w, band), QColor(32, 45, 43, 225));
    for (int hour = 0; hour < 24; ++hour) { const double longitude = Solar::wrap(sun.longitude + (hour - 12) * 15); const double x = point(longitude, 0).x(); const QString text = QString("%1").arg(hour, 2, 10, QChar('0')); for (double offset : {-w, 0.0, w}) { p->drawText(QRectF(x + offset - w / 48.0, 0, w / 24.0, band), Qt::AlignCenter, text); p->drawLine(QPointF(x + offset, band - 4), QPointF(x + offset, band)); } }
    p->setPen(QPen(QColor("#8d7c56"), 2)); p->setBrush(Qt::NoBrush); p->drawRect(QRectF(0, 0, w, h).adjusted(1, 1, -1, -1));
}
