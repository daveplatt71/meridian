#pragma once
#include <QQuickPaintedItem>
#include <QDateTime>
#include <QPainterPath>
#include <QImage>
#include <QJsonArray>
#include "solar.h"

class AtlasMap : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QDateTime utc READ utc WRITE setUtc NOTIFY utcChanged)
    Q_PROPERTY(bool equalArea READ equalArea WRITE setEqualArea NOTIFY equalAreaChanged)
    Q_PROPERTY(double hourBandHeight READ hourBandHeight WRITE setHourBandHeight NOTIFY hourBandHeightChanged)
public:
    explicit AtlasMap(QQuickItem *parent=nullptr);
    QDateTime utc() const {return utc_;}
    void setUtc(QDateTime value);
    bool equalArea() const { return equalArea_; }
    void setEqualArea(bool value);
    double hourBandHeight() const { return hourBandHeight_; }
    void setHourBandHeight(double value);
    void paint(QPainter *painter) override;
signals:
    void utcChanged();
    void equalAreaChanged();
    void hourBandHeightChanged();
private:
    QDateTime utc_;
    bool equalArea_ = false;
    double hourBandHeight_ = 28.0;
    QList<QJsonArray> polygons_;
    QList<QPainterPath> land_;
    QList<QPainterPath> countries_;
    QList<QColor> countryColors_;
    QList<QJsonArray> countryPolygons_;
    QPainterPath landClip_;
    QPainterPath countryClip_;
    QImage shade_;
    QImage relief_;
    QImage reliefSource_;
    qint64 shadeMinute_ = -1;
    void rebuildLand();
    void rebuildCountries();
    void rebuildRelief();
    void updateShade();
};
