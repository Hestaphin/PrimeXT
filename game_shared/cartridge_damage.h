// Shared cartridge damage math for legacy and explicit v4 damage logic.
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
    if (cartridge.schemaVersion >= 4) {
        switch (cartridge.damageLogic) {
        case DamageLogic::Legacy:
            return cartridge.attackLogic == AttackLogic::Hitscan ? cartridge.defaultDamage :
                std::numeric_limits<double>::quiet_NaN();
        case DamageLogic::ConfigurableBase:
            return cartridge.attackLogic == AttackLogic::Ballistic ? cartridge.baseDamage :
                std::numeric_limits<double>::quiet_NaN();
        case DamageLogic::EnergyBased:
            return cartridge.attackLogic == AttackLogic::Ballistic ? EnergyDensityDamage(
                KineticEnergyJ(cartridge.massGrams, velocityMps), cartridge.diameterMm,
                cartridge.damageCoefficient) : std::numeric_limits<double>::quiet_NaN();
        }
        return std::numeric_limits<double>::quiet_NaN();
    }
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
// v4 uses the *same* explicit logic in the editor preview and server impacts.
// A stopped projectile / invalid impact never receives the 5% minimum.
inline double ImpactDamage(DamageLogic logic, double impactEnergyJ,
                           double muzzleEnergyJ, double diameterMm,
                           double coefficient, double baseDamage) {
    if (!std::isfinite(impactEnergyJ) || impactEnergyJ < 0 ||
        !std::isfinite(muzzleEnergyJ) || muzzleEnergyJ <= 0)
        return std::numeric_limits<double>::quiet_NaN();
    if (impactEnergyJ == 0) return 0;
    switch (logic) {
    case DamageLogic::ConfigurableBase:
        return baseDamage * std::clamp(impactEnergyJ / muzzleEnergyJ, .05, 1.0);
    case DamageLogic::EnergyBased:
        return EnergyDensityDamage(impactEnergyJ, diameterMm, coefficient);
    default:
        return std::numeric_limits<double>::quiet_NaN();
    }
}
inline double ImpactDamage(const Cartridge& cartridge, double impactEnergyJ,
                           double muzzleEnergyJ) {
    if (cartridge.schemaVersion < 4)
        return ImpactDamage(impactEnergyJ, muzzleEnergyJ, cartridge.diameterMm,
            cartridge.schemaVersion >= 3 ? cartridge.damageCoefficient : 0, cartridge.damage);
    if (cartridge.attackLogic != AttackLogic::Ballistic)
        return std::numeric_limits<double>::quiet_NaN();
    return ImpactDamage(cartridge.damageLogic, impactEnergyJ, muzzleEnergyJ,
        cartridge.diameterMm, cartridge.damageCoefficient, cartridge.baseDamage);
}
} // namespace modular
