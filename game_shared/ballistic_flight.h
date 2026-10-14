// Dependency-free physical projectile integration; coordinates in metres and seconds.
#pragma once
namespace modular {
struct BallisticVec3 { double x = 0, y = 0, z = 0; };
struct BallisticState {
    BallisticVec3 position, velocity;
    double seconds = 0, meters = 0;
};
struct BallisticParameters {
    double massKg = 0.008, diameterM = 0.009, dragCoefficient = 0.30;
    double gravity = 9.80665, airDensity = 1.225;
};
// Time-centred gravity and stable quadratic drag. The opt-in displacement
// integrates the drag substep analytically instead of using a trapezoid;
// legacy callers retain their original math. Sweep previous to new position.
BallisticState IntegrateBallistic(const BallisticState& state,
    const BallisticParameters& params, double dt, bool exactDragDisplacement=false);
double BallisticEnergy(const BallisticState& state, double massKg);
}
