#include "ballistic_penetration.h"
#include <algorithm>
#include <cmath>
namespace modular {
PenetrationResult ComputePenetration(const PenetrationInput& i) {
    PenetrationResult r;
    if (!std::isfinite(i.massKg) || !std::isfinite(i.speedMps) ||
        !std::isfinite(i.thicknessM) || !std::isfinite(i.resistanceJoulesPerMeter) ||
        !std::isfinite(i.angleCosine) || !std::isfinite(i.projectileFactor) ||
        !std::isfinite(i.minimumSpeedMps) || i.massKg <= 0 ||
        i.speedMps <= 0 || i.thicknessM <= 0 || i.resistanceJoulesPerMeter <= 0 ||
        i.angleCosine <= 0.05 || i.angleCosine > 1.001 || i.projectileFactor <= 0 ||
        i.minimumSpeedMps < 0 || i.speedMps < i.minimumSpeedMps) return r;
    const double energy = 0.5 * i.massKg * i.speedMps * i.speedMps;
    const double lost = i.resistanceJoulesPerMeter * i.thicknessM /
        std::clamp(i.angleCosine, 0.05, 1.0) * i.projectileFactor;
    r.energyLostJoules = std::min(energy, lost);
    if (!std::isfinite(lost) || lost >= energy) return r;
    r.exitSpeedMps = std::sqrt(2.0 * (energy - lost) / i.massKg);
    r.penetrated = std::isfinite(r.exitSpeedMps) && r.exitSpeedMps >= i.minimumSpeedMps;
    if (!r.penetrated) r.exitSpeedMps = 0;
    return r;
}
}
