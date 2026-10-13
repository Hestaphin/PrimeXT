#include "physics_part_impact.h"
#include <cassert>
#include <cmath>
#include <limits>

int main()
{
    using namespace physicsasset;
    assert(PhysicsPartRayHitWins(4.0f, 5.0f));
    assert(!PhysicsPartRayHitWins(5.0f, 5.0f));
    assert(!PhysicsPartRayHitWins(4.9999f, 5.0f));
    assert(!PhysicsPartRayHitWins(6.0f, 5.0f)); // blocked by nearer wall
    assert(!PhysicsPartRayHitWins(-1.0f, 5.0f));
    assert(!PhysicsPartRayHitWins(std::numeric_limits<float>::quiet_NaN(), 5.0f));
    assert(!PhysicsPartMayReact(false, false, true, true, 10.0f)); // Animated
    assert(PhysicsPartMayReact(false, true, true, true, 10.0f)); // Driven -> Free
    assert(PhysicsPartMayReact(true, false, true, true, 10.0f)); // already Free
    assert(!PhysicsPartMayReact(false, true, false, true, 10.0f)); // no hinge
    assert(!PhysicsPartMayReact(false, true, true, false, 10.0f)); // invalid geometry
    assert(!PhysicsPartMayReact(false, true, true, true, 0.0f));
    assert(!PhysicsPartMayReact(false, true, true, true, std::numeric_limits<float>::infinity()));
    assert(PhysicsPartBlastWeight(0.0f, 100.0f) == 1.0f);
    assert(std::abs(PhysicsPartBlastWeight(50.0f, 100.0f) - 0.5f) < 1e-6f);
    assert(PhysicsPartBlastWeight(100.0f, 100.0f) == 0.0f);
    assert(PhysicsPartBlastWeight(101.0f, 100.0f) == 0.0f);
    assert(PhysicsPartBlastWeight(0.0f, -1.0f) == 0.0f);
    assert(PhysicsPartBlastWeight(std::numeric_limits<float>::quiet_NaN(), 5.0f) == 0.0f);
    // Normalization means two identical eligible parts split exactly one event.
    float a = PhysicsPartBlastWeight(10.0f, 100.0f);
    float b = PhysicsPartBlastWeight(10.0f, 100.0f);
    assert(std::abs((300.0f * a / (a + b)) + (300.0f * b / (a + b)) - 300.0f) < 1e-4f);
}
