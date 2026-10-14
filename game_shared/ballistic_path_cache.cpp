#include "ballistic_path_cache.h"
#include <algorithm>
#include <cmath>
namespace modular {
namespace {
bool Finite(BallisticVec3 p) {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
}
BallisticVec3 Mix(BallisticVec3 a,BallisticVec3 b,double t) {
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
}
}
bool PrecomputeBallisticCurve(std::vector<BallisticState>& points,
    const BallisticState& initial,const BallisticParameters& p,double range,
    double dt,int maxSamples,bool exactDragDisplacement) {
    points.clear();
    const double startingEnergy=BallisticEnergy(initial,p.massKg);
    if (!Finite(initial.position) || !Finite(initial.velocity) ||
        !std::isfinite(initial.seconds) || !std::isfinite(initial.meters) ||
        initial.seconds<0 || initial.seconds>10 || initial.meters<0 ||
        !std::isfinite(p.massKg) || p.massKg<=0 ||
        !std::isfinite(p.diameterM) || p.diameterM<=0 ||
        !std::isfinite(p.dragCoefficient) || p.dragCoefficient<0 ||
        !std::isfinite(p.gravity) || p.gravity<0 ||
        !std::isfinite(p.airDensity) || p.airDensity<0 ||
        !std::isfinite(range) || range<=initial.meters ||
        !std::isfinite(dt) || dt<=0 || dt>0.02 ||
        maxSamples<2 || maxSamples>2048 ||
        !std::isfinite(startingEnergy) || startingEnergy<0.1) return false;
    points.reserve(std::min(maxSamples,256));
    points.push_back(initial);
    for (int i=1;i<maxSamples;++i) {
        const auto& previous=points.back();
        const double timeLeft=10.0-previous.seconds;
        if (timeLeft<=1e-10 || range-previous.meters<=1e-10 ||
            BallisticEnergy(previous,p.massKg)<0.1) break;
        const double step=std::min(dt,timeLeft);
        const auto next=IntegrateBallistic(previous,p,step,exactDragDisplacement);
        if (!Finite(next.position) || !Finite(next.velocity) ||
            next.seconds<=previous.seconds || !std::isfinite(next.meters) ||
            next.meters<=previous.meters) { points.clear(); return false; }
        if (next.meters>=range) {
            // Never cache a terminal sample beyond the permitted range. The
            // abbreviated last interval is handled by time-based sampling.
            const double f=std::clamp((range-previous.meters)/
                                      (next.meters-previous.meters),0.0,1.0);
            BallisticState clipped;
            clipped.position=Mix(previous.position,next.position,f);
            clipped.velocity=Mix(previous.velocity,next.velocity,f);
            clipped.seconds=previous.seconds+(next.seconds-previous.seconds)*f;
            clipped.meters=range;
            points.push_back(clipped);
            break;
        }
        points.push_back(next);
        if (next.seconds>=10 || BallisticEnergy(next,p.massKg)<0.1) break;
    }
    return points.size()>1;
}
BallisticState SampleBallisticCurve(const std::vector<BallisticState>& points,
    double elapsed,double dt,bool curvedPosition) {
    if (points.empty()) return {};
    if (!std::isfinite(elapsed) || elapsed<=0 || !std::isfinite(dt) || dt<=0)
        return points.front();
    // Sample by stored timestamps, not by fixed indices: a range/lifetime
    // endpoint can end in the middle of the last nominal integration step.
    const double target=points.front().seconds+elapsed;
    if (target>=points.back().seconds) return points.back();
    const auto found=std::lower_bound(points.begin()+1,points.end(),target,
        [](const BallisticState& point,double time) { return point.seconds<time; });
    const auto& b=*found;
    const auto& a=*(found-1);
    const double t=std::clamp((target-a.seconds)/(b.seconds-a.seconds),0.0,1.0);
    BallisticState s;
    s.position=Mix(a.position,b.position,t);
    if (curvedPosition) {
        // Recover curvature between cached samples without integrating again
        // in the frame loop. Position and derivative match both endpoints;
        // vacuum gravity is a parabola rather than a 20 ms interpolation kink.
        const double span=b.seconds-a.seconds;
        const double t2=t*t,t3=t2*t;
        const double h00=2*t3-3*t2+1,h10=t3-2*t2+t;
        const double h01=-2*t3+3*t2,h11=t3-t2;
        const auto coordinate=[&](double pa,double pb,double va,double vb) {
            return h00*pa+h10*span*va+h01*pb+h11*span*vb;
        };
        s.position={coordinate(a.position.x,b.position.x,a.velocity.x,b.velocity.x),
                    coordinate(a.position.y,b.position.y,a.velocity.y,b.velocity.y),
                    coordinate(a.position.z,b.position.z,a.velocity.z,b.velocity.z)};
    }
    s.velocity=Mix(a.velocity,b.velocity,t);
    s.seconds=a.seconds+(b.seconds-a.seconds)*t;
    s.meters=a.meters+(b.meters-a.meters)*t;
    return s;
}
}
