#pragma once

// Milestone I.4: portable animation-to-physics velocity handoff.
// Engine/PhysX adapters supply world-space position and normalized orientation.
// Kept SDK-independent so degenerate timestamps and quaternion wrapping can be tested.
#include <cmath>

namespace physicsasset {
struct MotionVec3 { float x=0, y=0, z=0; };
struct MotionQuat { float x=0, y=0, z=0, w=1; };
struct MotionPose { MotionVec3 position; MotionQuat rotation; };
struct MotionVelocity { MotionVec3 linear; MotionVec3 angular; bool valid=false; };

inline bool MotionFinite(float v) { return std::isfinite(v); }
inline MotionVelocity EstimateMotion(const MotionPose& from, const MotionPose& to,
                                     float seconds, float maxLinear=4000.0f,
                                     float maxAngular=80.0f)
{
    MotionVelocity result;
    if(!MotionFinite(seconds) || seconds < 0.0001f || seconds > 0.25f ||
       !MotionFinite(maxLinear) || maxLinear <= 0 ||
       !MotionFinite(maxAngular) || maxAngular <= 0) return result;
    const float values[] = {from.position.x,from.position.y,from.position.z,
        to.position.x,to.position.y,to.position.z,
        from.rotation.x,from.rotation.y,from.rotation.z,from.rotation.w,
        to.rotation.x,to.rotation.y,to.rotation.z,to.rotation.w};
    for(float v : values) if(!MotionFinite(v)) return result;
    result.linear = {(to.position.x-from.position.x)/seconds,
                     (to.position.y-from.position.y)/seconds,
                     (to.position.z-from.position.z)/seconds};
    float dot=from.rotation.x*to.rotation.x+from.rotation.y*to.rotation.y+
              from.rotation.z*to.rotation.z+from.rotation.w*to.rotation.w;
    if(dot < -1.0001f || dot > 1.0001f) return MotionVelocity{};
    // Shortest-arc quaternion (q and -q are the same orientation).
    const float sign = dot < 0 ? -1.0f : 1.0f;
    const MotionQuat a{ -from.rotation.x,-from.rotation.y,-from.rotation.z,from.rotation.w };
    const MotionQuat b{ sign*to.rotation.x,sign*to.rotation.y,sign*to.rotation.z,sign*to.rotation.w };
    MotionQuat d{b.w*a.x+b.x*a.w+b.y*a.z-b.z*a.y,
                 b.w*a.y-b.x*a.z+b.y*a.w+b.z*a.x,
                 b.w*a.z+b.x*a.y-b.y*a.x+b.z*a.w,
                 b.w*a.w-b.x*a.x-b.y*a.y-b.z*a.z};
    if(d.w < 0) {d.x=-d.x; d.y=-d.y; d.z=-d.z; d.w=-d.w;}
    const float sinHalf = std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z);
    if(sinHalf > 0.000001f)
    {
        const float angle = 2.0f*std::atan2(sinHalf,d.w);
        const float scale = angle/(seconds*sinHalf);
        result.angular = {d.x*scale,d.y*scale,d.z*scale};
    }
    auto clamp = [](MotionVec3 &v, float limit) {
        const float length=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
        if(!MotionFinite(length)) return false;
        if(length > limit) {const float s=limit/length; v.x*=s;v.y*=s;v.z*=s;}
        return true;
    };
    if(!clamp(result.linear,maxLinear) || !clamp(result.angular,maxAngular)) return MotionVelocity{};
    result.valid=true;
    return result;
}
} // namespace physicsasset
