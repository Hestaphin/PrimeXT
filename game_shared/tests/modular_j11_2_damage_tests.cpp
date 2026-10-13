// J.11.2 — Cartridge-owned damage formula tests (no engine dependencies).
#include "cartridge_damage.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using namespace modular;
    Cartridge legacy;
    legacy.schemaVersion = 4;
    legacy.attackLogic = AttackLogic::Hitscan;
    legacy.damageLogic = DamageLogic::Legacy;
    legacy.defaultDamage = 60;
    legacy.legacyBulletType = 2;
    legacy.massGrams = 8;
    legacy.diameterMm = 9;
    assert(MuzzleDamage(legacy, 100) == 60);
    assert(MuzzleDamage(legacy, 400) == 60); // no energy-dependent hitscan damage
    assert(std::isnan(ImpactDamage(legacy, 50, 100))); // not a ballistic cartridge
    Cartridge base = legacy;
    base.attackLogic = AttackLogic::Ballistic;
    base.damageLogic = DamageLogic::ConfigurableBase;
    base.baseDamage = 40;
    assert(MuzzleDamage(base, 360) == 40);
    assert(MuzzleDamage(base, 100) == 40);
    assert(ImpactDamage(base, 518.4, 518.4) == 40);
    assert(ImpactDamage(base, 259.2, 518.4) == 20);
    assert(std::abs(ImpactDamage(base, 0.01*518.4, 518.4)-2)<1e-6);
    assert(ImpactDamage(base, 0, 518.4) == 0); // stopped projectile has no 5% phantom hit
    assert(std::isnan(ImpactDamage(base, 1, 0)));
    assert(std::isnan(ImpactDamage(base, std::numeric_limits<double>::infinity(), 50)));
    Cartridge energy = base;
    energy.damageLogic = DamageLogic::EnergyBased;
    energy.damageCoefficient = 2;
    const double start = MuzzleDamage(energy, 360);
    assert(start > 0 && std::isfinite(start));
    assert(std::abs(MuzzleDamage(energy,720)-start*4)<1e-9);
    assert(std::abs(ImpactDamage(energy,259.2,518.4)-start*0.5)<1e-9);
    assert(ImpactDamage(energy,0,518.4)==0);
    assert(std::isnan(ImpactDamage(energy,1,0)));
    Cartridge invalid = energy;
    invalid.attackLogic = AttackLogic::Hitscan;
    assert(std::isnan(MuzzleDamage(invalid, 360)));
    invalid = base; invalid.damageLogic=DamageLogic::Legacy;
    assert(std::isnan(MuzzleDamage(invalid,360)));
    Cartridge old;
    old.damage=17; old.schemaVersion=2;
    assert(MuzzleDamage(old,360)==17);
    assert(ImpactDamage(5,100,9,0,20)==1); // old 5%-floor formula unchanged
    std::cout << "PASS J.11.2 v4 legacy/base/energy damage, floor and old formats\n";
}
