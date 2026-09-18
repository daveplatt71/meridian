#include "solar.h"
#include "projection.h"
#include <QTimeZone>
#include <iostream>
#include <cstdlib>

void require(bool okay,const char *message){if(!okay){std::cerr<<message<<'\n';std::exit(1);}}
int main(){
    for(bool equalArea:{false,true}) {
        for(double latitude:{-90.0,-80.0,-45.0,0.0,45.0,80.0,90.0}) {
            auto y=Projection::projectY(latitude,equalArea);
            require(y>=0 && y<=1,"Projection stays inside the map");
            require(std::abs(Projection::latitudeAt(y,equalArea)-latitude)<1e-8,"Projection inverse matches forward latitude");
        }
    }
    require(std::abs(Projection::projectY(30,true)-0.25)<1e-10,"Equal-area 30N is one quarter from top");
    require(std::abs(Projection::aspect(true)-Solar::pi)<1e-10,"Equal-area natural aspect");
    auto at=[](const char *iso){return QDateTime::fromString(iso,Qt::ISODate);};
    // Independent astronomical constraints: declination at equinox/solstice.
    auto march=Solar::position(at("2026-03-20T14:46:00Z"));
    auto june=Solar::position(at("2026-06-21T08:24:00Z"));
    auto december=Solar::position(at("2026-12-21T20:50:00Z"));
    require(std::abs(march.latitude)<0.1,"Equinox declination should be near zero");
    require(std::abs(june.latitude-23.44)<0.1,"June solstice declination");
    require(std::abs(december.latitude+23.44)<0.1,"December solstice declination");
    require(Solar::cosineZenith(80,0,june)>0,"Arctic summer daylight");
    require(Solar::cosineZenith(-80,0,june)<0,"Antarctic winter darkness");
    auto a=Solar::position(at("2026-09-16T12:00:00Z"));
    auto b=Solar::position(at("2026-09-16T05:00:00-07:00"));
    require(std::abs(a.longitude-b.longitude)<1e-8,"Timezone-invariant coordinates");
    require(std::abs(Solar::cosineZenith(a.latitude,a.longitude,a)-1)<1e-10,"Subsolar zenith");
    require(Solar::cosineZenith(-a.latitude,Solar::wrap(a.longitude+180),a)<-0.999,"Antisolar midnight");
    require(std::abs(Solar::cosineZenith(30,-180,a)-Solar::cosineZenith(30,180,a))<1e-10,"Date-line seam");
    auto before=Solar::position(at("2024-02-29T23:59:59Z"));
    auto after=Solar::position(at("2024-03-01T00:00:00Z"));
    require(std::abs(Solar::wrap(after.longitude-before.longitude))<0.01,"Leap-day midnight continuity");
    auto noon=Solar::position(at("2000-01-01T12:00:00Z"));
    require(noon.longitude>0 && noon.longitude<2,"J2000 noon longitude sign");
    // Independent NOAA fractional-year approximation (solareqns.PDF), not the
    // Meeus algorithm used by production. Broad tolerances reflect the simpler
    // approximation; this catches sign, time-of-day and seasonal regressions.
    for(int month=1;month<=12;++month){
        QDateTime date(QDate(2026,month,15),QTime(5,30),QTimeZone::UTC);
        double gamma=2*Solar::pi/365*(date.date().dayOfYear()-1+(5.5-12)/24);
        double eq=229.18*(0.000075+0.001868*std::cos(gamma)-0.032077*std::sin(gamma)
            -0.014615*std::cos(2*gamma)-0.040849*std::sin(2*gamma));
        double dec=Solar::deg(0.006918-0.399912*std::cos(gamma)+0.070257*std::sin(gamma)
            -0.006758*std::cos(2*gamma)+0.000907*std::sin(2*gamma)
            -0.002697*std::cos(3*gamma)+0.00148*std::sin(3*gamma));
        auto result=Solar::position(date);
        require(std::abs(result.latitude-dec)<0.8,"Independent seasonal declination check");
        require(std::abs(result.equationMinutes-eq)<0.8,"Independent equation-of-time check");
        require(std::abs(Solar::wrap(result.longitude-(720-330-eq)/4))<0.2,"Independent morning longitude check");
    }
    std::cout<<"Solar geometry checks passed\n";
}
