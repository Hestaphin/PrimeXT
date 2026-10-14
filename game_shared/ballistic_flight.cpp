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
BallisticVec3 Drag(BallisticVec3 v, const BallisticParameters& p, double dt, double& amount) {
    const double area = 3.141592653589793 * p.diameterM * p.diameterM * 0.25;
    const double k = 0.5 * p.airDensity * p.dragCoefficient * area / p.massKg;
    amount = k * Magnitude(v) * dt;
    return v * (1.0 / (1.0 + amount));
}
}
BallisticState IntegrateBallistic(const BallisticState& s, const BallisticParameters& p,
    double dt, bool exactDragDisplacement) {
    if (!std::isfinite(dt) || dt <= 0 || dt > 0.02 || !std::isfinite(p.massKg) ||
        !std::isfinite(p.diameterM) || p.massKg <= 0 || p.diameterM <= 0 ||
        !std::isfinite(p.dragCoefficient) || p.dragCoefficient < 0 ||
        !std::isfinite(p.gravity) || p.gravity < 0 || !std::isfinite(p.airDensity) || p.airDensity < 0 ||
        !std::isfinite(s.position.x) || !std::isfinite(s.position.y) || !std::isfinite(s.position.z) ||
        !std::isfinite(s.velocity.x) || !std::isfinite(s.velocity.y) || !std::isfinite(s.velocity.z))
        return s;
    const BallisticVec3 gravityHalf{0,0,-p.gravity*dt*.5};
    double dragAmount=0;
    const BallisticVec3 kicked=s.velocity + gravityHalf;
    const BallisticVec3 velAfter = Drag(kicked,p,dt,dragAmount) + gravityHalf;
    BallisticVec3 midpointVel = (s.velocity + velAfter) * 0.5;
    if (exactDragDisplacement) {
        // For dv/dt=-k*|v|*v, displacement is v*dt*log(1+q)/q.
        // log1p retains precision as q approaches zero. Gravity remains
        // centred by the same half kicks, including the exact vacuum case.
        const double average=dragAmount>0 ? std::log1p(dragAmount)/dragAmount : 1.0;
        midpointVel=kicked*average;
    }
    BallisticState next = s;
    next.position = s.position + midpointVel * dt;
    next.velocity = velAfter;
    next.seconds = s.seconds + dt;
    double stepDistance = Magnitude(midpointVel) * dt;
    // Net displacement is not arc length. At a near-vertical apex the
    // gravity kick can reverse velocity within this step, yielding zero net
    // displacement although the projectile travelled up and back down.
    // Only in reversal steps, integrate speed along the endpoint velocities;
    // leave the fast/common ballistic path byte-for-byte equivalent.
    const double endpointDot = s.velocity.x * velAfter.x +
        s.velocity.y * velAfter.y + s.velocity.z * velAfter.z;
    if (endpointDot < 0.0) {
        const BallisticVec3 change{velAfter.x - s.velocity.x,
            velAfter.y - s.velocity.y, velAfter.z - s.velocity.z};
        // Eight-panel Simpson integration; an exactly symmetric vacuum apex
        // lands on panel 4 and gives the exact positive travelled distance.
        double speedSum = Magnitude(s.velocity) + Magnitude(velAfter);
        for (int i = 1; i < 8; ++i)
            speedSum += (i % 2 ? 4.0 : 2.0) *
                Magnitude(s.velocity + change * (double(i) / 8.0));
        stepDistance = dt * speedSum / 24.0;
    }
    next.meters = s.meters + stepDistance;
    return next;
}
double BallisticEnergy(const BallisticState& s, double massKg) {
    const double speed = Magnitude(s.velocity);
    return 0.5 * massKg * speed * speed;
}
}
