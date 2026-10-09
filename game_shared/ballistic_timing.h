// J.10.1 - dependency-free time accounting for a projectile crossing material.
// All distances/coordinates are in metres and times are in seconds.
#pragma once
#include "ballistic_flight.h"
#include <algorithm>
#include <cmath>

namespace modular {
// Per-shot delta, never the elapsed time since another projectile or the
// manager's previous update. Unused catch-up remains on the shot's clock.
inline double ShotTimeBudget(double now,double lastSimulated,double maxFrame=0.1) {
    if (!std::isfinite(now) || !std::isfinite(lastSimulated) ||
        !std::isfinite(maxFrame) || maxFrame<=0) return 0;
    return std::clamp(now-lastSimulated,0.0,maxFrame);
}
struct BallisticTransit {
    BallisticState entry;
    BallisticVec3 exitPosition, exitVelocity;
    double distanceMeters = 0;
    double durationSeconds = 0;
    double elapsedSeconds = 0;
    bool active = false;
};
inline bool ValidTransit(const BallisticTransit& t) {
    const auto finiteVec=[](BallisticVec3 v) {
        return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);
    };
    return finiteVec(t.entry.position)&&finiteVec(t.entry.velocity)&&
        finiteVec(t.exitPosition)&&finiteVec(t.exitVelocity)&&
        std::isfinite(t.entry.seconds)&&std::isfinite(t.entry.meters)&&
        std::isfinite(t.distanceMeters)&&t.distanceMeters>0&&
        std::isfinite(t.durationSeconds)&&t.durationSeconds>0&&
        std::isfinite(t.elapsedSeconds)&&t.elapsedSeconds>=0&&
        t.elapsedSeconds<t.durationSeconds&&t.entry.seconds>=0&&t.entry.meters>=0;
}
// A falling shot may have *more kinetic energy* than at the muzzle.
// Its possible gravitational gain is bounded by m*g*distance travelled;
// penetration/drag only remove energy. This intentionally avoids assuming
// muzzle kinetic energy is a hard ceiling when validating restore snapshots.
inline bool PlausibleBallisticEnergy(const BallisticState& state,
    const BallisticParameters& physics,double muzzleEnergy) {
    const double current=BallisticEnergy(state,physics.massKg);
    if (!std::isfinite(current) || !std::isfinite(muzzleEnergy) ||
        muzzleEnergy<=0 || !std::isfinite(state.meters) || state.meters<0 ||
        !std::isfinite(physics.massKg) || physics.massKg<=0 ||
        !std::isfinite(physics.gravity) || physics.gravity<0) return false;
    const double allowance=physics.massKg*physics.gravity*state.meters;
    const double tolerance=std::max(0.1,muzzleEnergy*0.01);
    return std::isfinite(allowance) && current<=muzzleEnergy+allowance+tolerance;
}
inline double Length(BallisticVec3 v) {
    return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
}
// Constant longitudinal deceleration during material traversal. Fraction of
// thickness crossed at fraction u of transit time (NOT linear in time).
inline double TransitDistanceFraction(const BallisticTransit& t,double u) {
    const double a=Length(t.entry.velocity), b=Length(t.exitVelocity);
    const double weight=a+b;
    if (!(weight>0)) return u;
    return (2*a*u+(b-a)*u*u)/weight;
}
inline double AdvanceTransit(BallisticState& state,BallisticTransit& t,
    double budget,double maxMeters,double maxSeconds) {
    if (!t.active || !ValidTransit(t) || !std::isfinite(budget) || budget<=0) return 0;
    const double oldElapsed=t.elapsedSeconds;
    double newElapsed=std::min(t.durationSeconds,oldElapsed+budget);
    newElapsed=std::min(newElapsed,oldElapsed+std::max(0.0,maxSeconds-state.seconds));
    // Clamp by remaining total range, including distance already spent inside
    // the current material. Bisection is deterministic and independent of FPS.
    const double remainingDistance=std::max(0.0,maxMeters-t.entry.meters);
    const auto distanceAt=[&](double elapsed) {
        return t.distanceMeters*TransitDistanceFraction(t,elapsed/t.durationSeconds);
    };
    if (distanceAt(newElapsed)>remainingDistance) {
        double lo=oldElapsed,hi=newElapsed;
        for (int i=0;i<48;++i) {
            const double mid=0.5*(lo+hi);
            if (distanceAt(mid)>remainingDistance) hi=mid;
            else lo=mid;
        }
        newElapsed=lo;
    }
    const double consumed=std::max(0.0,newElapsed-oldElapsed);
    t.elapsedSeconds=newElapsed;
    const double u=std::clamp(newElapsed/t.durationSeconds,0.0,1.0);
    const double d=std::clamp(TransitDistanceFraction(t,u),0.0,1.0);
    const auto lerp=[](double a,double b,double f) { return a+(b-a)*f; };
    state.position={lerp(t.entry.position.x,t.exitPosition.x,d),
                    lerp(t.entry.position.y,t.exitPosition.y,d),
                    lerp(t.entry.position.z,t.exitPosition.z,d)};
    state.velocity={lerp(t.entry.velocity.x,t.exitVelocity.x,u),
                    lerp(t.entry.velocity.y,t.exitVelocity.y,u),
                    lerp(t.entry.velocity.z,t.exitVelocity.z,u)};
    state.seconds=t.entry.seconds+newElapsed;
    state.meters=std::min(maxMeters,t.entry.meters+t.distanceMeters*d);
    if (t.durationSeconds-newElapsed<=1e-10) {
        state.position=t.exitPosition;
        state.velocity=t.exitVelocity;
        state.seconds=t.entry.seconds+t.durationSeconds;
        state.meters=std::min(maxMeters,t.entry.meters+t.distanceMeters);
        t.active=false;
    }
    return consumed;
}
} // namespace modular
