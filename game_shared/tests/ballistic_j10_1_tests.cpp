// J.10.1 deterministic regression tests, executable without engine/PhysX.
#include "ballistic_path_cache.h"
#include "ballistic_timing.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

static bool Near(double a,double b,double e=1e-8) { return std::abs(a-b)<e; }
int main() {
    using namespace modular;
    BallisticParameters p{};
    p.gravity=0; p.airDensity=0;
    BallisticState muzzle{};
    muzzle.velocity={385,0,0};
    const double maxRange=16.0/39.37007874015748;
    std::vector<BallisticState> path;
    assert(PrecomputeBallisticCurve(path,muzzle,p,maxRange));
    // The first 10ms flight would travel 151.6 game units without clipping.
    assert(path.size()==2);
    assert(Near(path.back().meters,maxRange));
    assert(path.back().position.x<=maxRange+1e-10);
    assert(path.back().seconds<0.01);
    auto after=SampleBallisticCurve(path,0.02);
    assert(after.meters<=maxRange && after.position.x<40.0/39.37007874015748);
    auto middle=SampleBallisticCurve(path,path.back().seconds/2.0);
    assert(Near(middle.meters,maxRange/2.0));
    // A terminal lifetime sample must also be clipped, not overshoot 10s.
    BallisticState late{}; late.seconds=9.998; late.velocity={100,0,0};
    assert(PrecomputeBallisticCurve(path,late,p,1000.0));
    assert(Near(path.back().seconds,10.0));

    // Two different shots in one manager frame must not share age.
    double olderShotLast=1.0,newShotLast=1.02;
    assert(Near(ShotTimeBudget(1.02,olderShotLast),0.02));
    assert(Near(ShotTimeBudget(1.02,newShotLast),0.0));
    assert(Near(ShotTimeBudget(1.5,newShotLast),0.1));
    assert(Near(ShotTimeBudget(1.5,newShotLast+0.1),0.1)); // backlog persists

    // Mid-material position, range and age advance only as frames pay time.
    BallisticTransit t{};
    t.entry.velocity={360,0,0};
    t.exitVelocity={300,0,0};
    t.exitPosition={0.30,0,0};
    t.distanceMeters=0.30;
    t.durationSeconds=2*t.distanceMeters/(360+300);
    t.active=true;
    BallisticState inside=t.entry;
    const double firstBudget=0.0003;
    double used=AdvanceTransit(inside,t,firstBudget,10,10);
    assert(Near(used,firstBudget));
    assert(t.active && inside.seconds< t.durationSeconds);
    assert(inside.position.x>0 && inside.position.x<0.30);
    assert(inside.meters>0 && inside.meters<0.30);
    used=AdvanceTransit(inside,t,0.1,10,10);
    assert(!t.active && used>0);
    assert(Near(inside.position.x,0.30));
    assert(Near(inside.seconds,2*0.30/(360+300)));
    // A short maximum range expires *inside* the wall, never at its far end.
    t={}; t.entry.velocity={360,0,0}; t.exitVelocity={300,0,0};
    t.exitPosition={0.30,0,0}; t.distanceMeters=0.30;
    t.durationSeconds=2*t.distanceMeters/(360+300); t.active=true;
    inside=t.entry;
    used=AdvanceTransit(inside,t,0.01,0.10,10);
    assert(t.active && used<t.durationSeconds);
    assert(inside.meters<=0.10+1e-9);
    assert(inside.position.x<=0.10+1e-8);

    // Actual downhill acceleration is physically valid despite kinetic energy
    // exceeding muzzle energy (a failure in J.9's restore validator).
    BallisticParameters gravity{};
    gravity.airDensity=0; gravity.gravity=9.80665;
    BallisticState falling{};
    falling.velocity={0,0,-100};
    const double initialEnergy=BallisticEnergy(falling,gravity.massKg);
    for (int i=0;i<10;++i) falling=IntegrateBallistic(falling,gravity,0.01);
    assert(BallisticEnergy(falling,gravity.massKg)>initialEnergy);
    assert(PlausibleBallisticEnergy(falling,gravity,initialEnergy));
    BallisticState impossible=falling;
    impossible.velocity={0,0,-5000};
    assert(!PlausibleBallisticEnergy(impossible,gravity,initialEnergy));
    std::cout<<"PASS J.10.1: exact range/lifetime, per-shot birth, material transit, gravity restore\n";
}
