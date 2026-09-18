#pragma once
#include <QObject>
#include <QTimer>
#include "solar.h"

class Clock : public QObject {
    Q_OBJECT
    Q_PROPERTY(QDateTime utc READ utc NOTIFY changed)
    Q_PROPERTY(QString utcText READ utcText NOTIFY changed)
    Q_PROPERTY(QString localText READ localText NOTIFY changed)
    Q_PROPERTY(QString dateText READ dateText NOTIFY changed)
    Q_PROPERTY(QString declinationText READ declinationText NOTIFY changed)
    Q_PROPERTY(QString solarLongitudeText READ solarLongitudeText NOTIFY changed)
    Q_PROPERTY(QString modeText READ modeText CONSTANT)
public:
    explicit Clock(QDateTime fixed = {}, QObject *parent = nullptr):QObject(parent),fixed_(fixed.isValid()),now_(fixed_?fixed:QDateTime::currentDateTimeUtc()) {
        timer_.setInterval(1000);
        connect(&timer_,&QTimer::timeout,this,[this]{now_=QDateTime::currentDateTimeUtc();emit changed();});
        if(!fixed_) timer_.start();
    }
    QDateTime utc() const {return now_;}
    QString utcText() const {return now_.toUTC().toString("HH:mm:ss");}
    QString localText() const {return now_.toLocalTime().toString("HH:mm");}
    QString dateText() const {return now_.toUTC().toString("dd MMMM yyyy").toUpper();}
    QString declinationText() const {double v=Solar::position(now_).latitude;return QString::number(std::abs(v),'f',1)+QStringLiteral("° ")+(v<0?"S":"N");}
    QString solarLongitudeText() const {double v=Solar::position(now_).longitude;return QString::number(std::abs(v),'f',1)+QStringLiteral("° ")+(v<0?"W":"E");}
    QString modeText() const {return fixed_?"HISTORICAL PREVIEW":"LIVE SOLAR TIME";}
signals:
    void changed();
private:
    bool fixed_;
    QDateTime now_;
    QTimer timer_;
};
