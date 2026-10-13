#include "ballistic_flight.h"
#include <algorithm>
#include <cmath>
namespace modular {
namespace {
BallisticVec3 operator+(BallisticVec3 a, BallisticVec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
BallisticVec3 operator*(BallisticVec3 a, double k) { return {a.x*k,a.y*k,a.z*k}; }
double Magnitude(BallisticVec3 a) { return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z); }
// Quadratic drag in a positivity-preserving implicit form. This cannot
// reverse an extreme projectile velocity solely because the simulation step
// is too large for its high Cd/area/mass ratio.
BallisticVec3 Drag(BallisticVec3 v, const BallisticParameters& p, double dt) {
    const double area = 3.141592653589793 * p.diameterM * p.diameterM * 0.25;
    const double k = 0.5 * p.airDensity * p.dragCoefficient * area / p.massKg;
    return v * (1.0 / (1.0 + k * Magnitude(v) * dt));
}
}
BallisticState IntegrateBallistic(const BallisticState& s, const BallisticParameters& p, double dt) {
    if (!std::isfinite(dt) || dt <= 0 || dt > 0.02 || !std::isfinite(p.massKg) ||
        !std::isfinite(p.diameterM) || p.massKg <= 0 || p.diameterM <= 0 ||
        !std::isfinite(p.dragCoefficient) || p.dragCoefficient < 0 ||
        !std::isfinite(p.gravity) || p.gravity < 0 || !std::isfinite(p.airDensity) || p.airDensity < 0 ||
        !std::isfinite(s.position.x) || !std::isfinite(s.position.y) || !std::isfinite(s.position.z) ||
        !std::isfinite(s.velocity.x) || !std::isfinite(s.velocity.y) || !std::isfinite(s.velocity.z))
        return s;
    const BallisticVec3 gravityHalf{0,0,-p.gravity*dt*.5};
    const BallisticVec3 velAfter = Drag(s.velocity + gravityHalf,p,dt) + gravityHalf;
    const BallisticVec3 midpointVel = (s.velocity + velAfter) * 0.5;
    BallisticState next = s;
    next.position = s.position + midpointVel * dt;
    next.velocity = velAfter;
    next.seconds = s.seconds + dt;
    next.meters = s.meters + Magnitude(midpointVel) * dt;
    return next;
}
double BallisticEnergy(const BallisticState& s, double massKg) {
    const double speed = Magnitude(s.velocity);
    return 0.5 * massKg * speed * speed;
}
}
