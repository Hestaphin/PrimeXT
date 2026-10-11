// J.8 portable penetration energy-budget helpers (SI units).
#pragma once
namespace modular {
struct PenetrationInput {
    double massKg = 0;
    double speedMps = 0;
    double thicknessM = 0;
    double resistanceJoulesPerMeter = 0;
    double angleCosine = 1;
    double projectileFactor = 1;
    double minimumSpeedMps = 0;
};
struct PenetrationResult {
    bool penetrated = false;
    double exitSpeedMps = 0;
    double energyLostJoules = 0;
};
// A conservative gameplay model: failure consumes the projectile; success
// conserves remaining kinetic energy. Geometry must independently verify exit.
PenetrationResult ComputePenetration(const PenetrationInput& input);
}
