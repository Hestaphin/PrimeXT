// I.10.5: pure, SDK-independent physical-part cache/expiry policy tests.
// Rendering/packet integration is checked separately by the source wiring test.
#include "physics_part_net.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

int main()
{
    bool active[4] = { false, false, false, false };
    assert(!partnet::AnyActivePartPose(active, 4));
    assert(!partnet::AnyActivePartPose(nullptr, 4));
    assert(!partnet::AnyActivePartPose(active, 0));

    // The owner can remain at the same cached animation frame, but a moving
    // part must request new render bones for every interpolated frame.
    const float received = 10.0f;
    active[2] = true;
    for (float now : {10.0f, 10.01f, 10.025f, 10.05f, 10.50f}) {
        assert(partnet::AnyActivePartPose(active, 4));
        assert(partnet::PartPoseFresh(now, received));
    }
    assert(partnet::PartPoseFresh(11.0f, received));
    assert(!partnet::PartPoseFresh(11.001f, received));
    // A lost clear eventually removes ownership of the bone, allowing the
    // next evaluation to return to the normal animation-cache path.
    if (!partnet::PartPoseFresh(11.001f, received)) active[2] = false;
    assert(!partnet::AnyActivePartPose(active, 4));

    // No time reversal, NaN, or infinities may leave an override latched.
    assert(!partnet::PartPoseFresh(9.99f, received));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    assert(!partnet::PartPoseFresh(nan, received));
    assert(!partnet::PartPoseFresh(10.0f, nan));
    assert(!partnet::PartPoseFresh(inf, received));
    assert(!partnet::PartPoseFresh(10.0f, inf));

    // One active override is sufficient, even when other bones are clear.
    active[3] = true;
    assert(partnet::AnyActivePartPose(active, 4));
    active[3] = false;
    assert(!partnet::AnyActivePartPose(active, 4));
    std::cout << "I.10.5 physical part pose cache and expiry policy PASS\n";
}
