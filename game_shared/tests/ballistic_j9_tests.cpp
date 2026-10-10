// J.9 portable regression tests. PhysX/gameplay contact tests require a running game.
#include "ballistic_exit_geometry.h"
#include "ballistic_penetration.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
int main() {
    using namespace modular;
    assert(ValidBallisticExit(96,8,0,1));
    assert(ValidBallisticExit(96,8,0.5f,0.7f));
    assert(!ValidBallisticExit(96,0.2f,0,1));
    assert(!ValidBallisticExit(96,96,0,1));
    assert(!ValidBallisticExit(96,2,0,-1)); // wrong-facing backface
    assert(!ValidBallisticExit(96,2,1.0f,1)); // different ray or part
    assert(!ValidBallisticExit(96,2,0,0));
    assert(!ValidBallisticExit(96,std::numeric_limits<float>::quiet_NaN(),0,1));
    assert(!ValidBallisticExit(96,2,0,std::numeric_limits<float>::infinity()));
    assert(!ValidBallisticExit(0,2,0,1));
    // Living material: 9mm sample, finite thickness, speed and energy reduction.
    PenetrationInput tissue{0.008,360.0,0.08,1500.0,1.0,1.0,80.0};
    const auto afterTissue=ComputePenetration(tissue);
    assert(afterTissue.penetrated && afterTissue.exitSpeedMps>0 && afterTissue.exitSpeedMps<360);
    assert(afterTissue.energyLostJoules>0);
    // Opaque geometry and glancing angles must stop the projectile.
    tissue.thicknessM=0.40; tissue.resistanceJoulesPerMeter=90000;
    assert(!ComputePenetration(tissue).penetrated);
    tissue.thicknessM=0.08;tissue.resistanceJoulesPerMeter=1500;
    tissue.angleCosine=0.01;
    assert(!ComputePenetration(tissue).penetrated);
    std::cout<<"PASS J.9: bounded same-ray exit geometry and conservative post-hit energy\n";
}
