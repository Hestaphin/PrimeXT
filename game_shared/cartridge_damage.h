// Format-3 validation math shared by the game and native editor loaders.
// Energy is in joules; the projectile's circular cross-section is in mm^2.
#pragma once
#include "modular_weapons.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace modular {
inline double ProjectileAreaMm2(double diameterMm) {
    if (!std::isfinite(diameterMm) || diameterMm <= 0) return 0;
    const double area = 3.14159265358979323846 * diameterMm * diameterMm * .25;
    return std::isfinite(area) ? area : 0;
}
inline double KineticEnergyJ(double massGrams, double speedMps) {
    if (!std::isfinite(massGrams) || massGrams <= 0 ||
        !std::isfinite(speedMps) || speedMps < 0)
        return std::numeric_limits<double>::quiet_NaN();
    return .5 * (massGrams * .001) * speedMps * speedMps;
}
inline double EnergyDensityDamage(double energyJ, double diameterMm, double coefficient) {
    const double area = ProjectileAreaMm2(diameterMm);
    if (!area || !std::isfinite(energyJ) || energyJ < 0 ||
        !std::isfinite(coefficient) || coefficient <= 0)
        return std::numeric_limits<double>::quiet_NaN();
    return energyJ / area * coefficient;
}
inline double MuzzleDamage(const Cartridge& cartridge, double velocityMps) {
    return cartridge.schemaVersion >= 3 ? EnergyDensityDamage(
        KineticEnergyJ(cartridge.massGrams, velocityMps), cartridge.diameterMm,
        cartridge.damageCoefficient) : cartridge.damage;
}
inline double ImpactDamage(double energyJ, double initialEnergyJ, double diameterMm,
                           double coefficient, double legacyDamage) {
    if (coefficient > 0) return EnergyDensityDamage(energyJ, diameterMm, coefficient);
    // Formats 1/2 retain their authored damage and historical five-percent floor.
    return legacyDamage * std::clamp(energyJ / std::max(.00001, initialEnergyJ), .05, 1.0);
}
} // namespace modular
