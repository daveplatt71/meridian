#pragma once
#include "solar.h"
#include <algorithm>

namespace Projection {
inline double projectY(double latitude, bool equalArea) {
    latitude = std::clamp(latitude, -90.0, 90.0);
    return equalArea ? (1.0 - std::sin(Solar::rad(latitude))) / 2.0
                     : (90.0 - latitude) / 180.0;
}
inline double latitudeAt(double y, bool equalArea) {
    y = std::clamp(y, 0.0, 1.0);
    return equalArea ? Solar::deg(std::asin(1.0 - 2.0 * y)) : 90.0 - 180.0 * y;
}
inline double aspect(bool equalArea) { return equalArea ? Solar::pi : 2.0; }
}
