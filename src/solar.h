#pragma once
#include <QDateTime>
#include <cmath>

namespace Solar {
constexpr double pi = 3.14159265358979323846;
inline double rad(double v) { return v*pi/180.0; }
inline double deg(double v) { return v*180.0/pi; }
inline double wrap(double v) { return v-360.0*std::floor((v+180.0)/360.0); }
struct Position { double latitude; double longitude; double equationMinutes; };
inline Position position(const QDateTime &instant) {
    const auto utc = instant.toUTC();
    double jd = utc.toMSecsSinceEpoch()/86400000.0 + 2440587.5;
    double t = (jd-2451545.0)/36525.0;
    double l = std::fmod(280.46646+t*(36000.76983+0.0003032*t),360.0);
    double m = rad(357.52911+t*(35999.05029-0.0001537*t));
    double e = 0.016708634-t*(0.000042037+0.0000001267*t);
    double c = std::sin(m)*(1.914602-t*(0.004817+0.000014*t))
        +std::sin(2*m)*(0.019993-0.000101*t)+std::sin(3*m)*0.000289;
    double omega = rad(125.04-1934.136*t);
    double lambda = rad(l+c-0.00569-0.00478*std::sin(omega));
    double eps = rad(23+(26+(21.448-t*(46.815+t*(0.00059-t*0.001813)))/60.0)/60.0
        +0.00256*std::cos(omega));
    double dec = deg(std::asin(std::sin(eps)*std::sin(lambda)));
    double y = std::pow(std::tan(eps/2),2);
    double eq = 4*deg(y*std::sin(2*rad(l))-2*e*std::sin(m)
        +4*e*y*std::sin(m)*std::cos(2*rad(l))-0.5*y*y*std::sin(4*rad(l))
        -1.25*e*e*std::sin(2*m));
    double minutes = utc.time().msecsSinceStartOfDay()/60000.0;
    return {dec,wrap((720-minutes-eq)/4),eq};
}
inline double cosineZenith(double latitude, double longitude, Position sun) {
    return std::sin(rad(latitude))*std::sin(rad(sun.latitude))
        +std::cos(rad(latitude))*std::cos(rad(sun.latitude))*std::cos(rad(longitude-sun.longitude));
}
}
