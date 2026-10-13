#include "ballistic_path_cache.h"
#include "ballistic_penetration.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
int main() {
    using namespace modular;
    BallisticParameters physics{};
    BallisticState start{}; start.velocity={360,0,0};
    std::vector<BallisticState> path;
    assert(PrecomputeBallisticCurve(path,start,physics,300.0));
    assert(path.size()>5 && path.size()<=1002);
    // Cached samples must match the integrator exactly at sample boundaries.
    BallisticState live=start;
    for (int i=1;i<5;++i) {
        live=IntegrateBallistic(live,physics,0.01);
        const auto cached=SampleBallisticCurve(path,0.01*i);
        assert(std::abs(cached.position.x-live.position.x)<1.e-9);
        assert(std::abs(cached.position.z-live.position.z)<1.e-9);
    }
    const auto half=SampleBallisticCurve(path,0.015);
    assert(half.seconds>0.014 && half.seconds<0.016);
    assert(!PrecomputeBallisticCurve(path,start,physics,0.0));
    assert(path.empty());
    PenetrationInput in{0.008,360.0,0.01,4000.0,1.0,1.0,50.0};
    const auto wood=ComputePenetration(in);
    assert(wood.penetrated && wood.exitSpeedMps<360 && wood.exitSpeedMps>300);
    const double loss=0.5*in.massKg*(in.speedMps*in.speedMps-wood.exitSpeedMps*wood.exitSpeedMps);
    assert(std::abs(loss-wood.energyLostJoules)<1.e-7);
    in.thicknessM=1.0;
    assert(!ComputePenetration(in).penetrated);
    in.thicknessM=0.01; in.minimumSpeedMps=400;
    assert(!ComputePenetration(in).penetrated);
    in.minimumSpeedMps=50; in.angleCosine=0.01;
    assert(!ComputePenetration(in).penetrated);
    in.angleCosine=1;in.massKg=-1;
    assert(!ComputePenetration(in).penetrated);
    std::cout<<"PASS J.8: cached trajectory/interpolation, invalid trajectory, material energy, thickness, speed and incidence tests\n";
}
