// J.7 independent compatibility regression: I.10.10 root/child movement
// ownership and J.6 ballistic math are retained in the same merged build.
// This does not replace a live PhysX / Xash gameplay acceptance test.
#include "ballistic_flight.h"
#include "physics_asset_trace_owner.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    using namespace modular;
    using physicsasset::SelectMovementSweepOwners;

    // An articulated child can be queried even if its root actor owns no
    // current collision shape. This is an I.10.10 physics regression guard.
    const auto childOnly = SelectMovementSweepOwners(true, false, false, true);
    assert(childOnly.Any() && !childOnly.root && childOnly.children);
    const auto disabledChild = SelectMovementSweepOwners(true, true, true, false);
    assert(disabledChild.Any() && disabledChild.root && !disabledChild.children);
    assert(!SelectMovementSweepOwners(false, true, true, true).Any());

    // A J.6 ballistic step must produce a downward-bending trajectory while
    // still retaining forward travel for a valid real-time projectile.
    BallisticParameters p;
    p.massKg = 0.008;
    p.dragCoefficient = 0.0;
    p.gravity = 9.80665;
    BallisticState start;
    start.velocity = {360.0, 0.0, 0.0};
    const auto step = IntegrateBallistic(start, p, 0.005);
    assert(std::isfinite(step.position.x) && std::isfinite(step.position.z));
    assert(step.position.x > 0.0 && step.position.z < 0.0);
    assert(step.seconds > start.seconds && step.meters > start.meters);

    // Bullet mass affects impact energy, but not gravitational acceleration.
    const double energy = BallisticEnergy(start, p.massKg);
    assert(std::abs(energy - 518.4) < 1e-6);
    BallisticParameters heavy = p;
    heavy.massKg *= 2.0;
    const auto heavyStep = IntegrateBallistic(start, heavy, 0.005);
    assert(std::abs(heavyStep.position.z - step.position.z) < 1e-10);
    std::cout << "PASS J.7: I.10.10 sweep-owner policy and J.6 projectile physics\n";
}
