#pragma once
// Mathematical/frame/setter facade only. No PhysX SDK, actors or solver.
#include <cmath>
namespace physx
{
constexpr float PxPi=3.14159265358979323846f;
struct PxVec3 {float x=0,y=0,z=0;PxVec3()=default;PxVec3(float a,float b,float c):x(a),y(b),z(c){};};
struct PxMat33 {PxVec3 a,b,c;PxMat33(PxVec3 x,PxVec3 y,PxVec3 z):a(x),b(y),c(z){};};
struct PxQuat
{
    float x=0,y=0,z=0,w=1;
    explicit PxQuat(const PxMat33 &m)
    {
        const float trace=m.a.x+m.b.y+m.c.z;
        if(trace>0) {const float s=std::sqrt(trace+1)*2;w=s/4;x=(m.b.z-m.c.y)/s;y=(m.c.x-m.a.z)/s;z=(m.a.y-m.b.x)/s;}
        else if(m.a.x>m.b.y&&m.a.x>m.c.z) {const float s=std::sqrt(1+m.a.x-m.b.y-m.c.z)*2;w=(m.b.z-m.c.y)/s;x=s/4;y=(m.b.x+m.a.y)/s;z=(m.c.x+m.a.z)/s;}
        else if(m.b.y>m.c.z) {const float s=std::sqrt(1+m.b.y-m.a.x-m.c.z)*2;w=(m.c.x-m.a.z)/s;x=(m.b.x+m.a.y)/s;y=s/4;z=(m.c.y+m.b.z)/s;}
        else {const float s=std::sqrt(1+m.c.z-m.a.x-m.b.y)*2;w=(m.a.y-m.b.x)/s;x=(m.c.x+m.a.z)/s;y=(m.c.y+m.b.z)/s;z=s/4;}
    }
    PxVec3 rotate(PxVec3 v) const
    {
        const float tx=2*(y*v.z-z*v.y),ty=2*(z*v.x-x*v.z),tz=2*(x*v.y-y*v.x);
        return {v.x+w*tx+y*tz-z*ty,v.y+w*ty+z*tx-x*tz,v.z+w*tz+x*ty-y*tx};
    }
};
struct PxTransform {PxVec3 p;PxQuat q;PxTransform(PxVec3 v,PxQuat r):p(v),q(r){};};
struct PxD6Axis {enum Enum {eX,eY,eZ,eTWIST,eSWING1,eSWING2};};
struct PxD6Motion {enum Enum {eLOCKED,eLIMITED,eFREE};};
struct PxJointAngularLimitPair {float lower,upper;PxJointAngularLimitPair(float a,float b):lower(a),upper(b){};};
struct PxJointLimitCone {float yAngle,zAngle;PxJointLimitCone(float a,float b):yAngle(a),zAngle(b){};};
struct PxD6Joint
{
    PxD6Motion::Enum motions[6]{PxD6Motion::eFREE,PxD6Motion::eFREE,PxD6Motion::eFREE,PxD6Motion::eFREE,PxD6Motion::eFREE,PxD6Motion::eFREE};
    unsigned writes[6]{};PxJointAngularLimitPair twist{-1,1};PxJointLimitCone swing{1,1};
    void setMotion(PxD6Axis::Enum a,PxD6Motion::Enum m) {motions[a]=m;++writes[a];}
    void setTwistLimit(PxJointAngularLimitPair p) {twist=p;}
    void setSwingLimit(PxJointLimitCone p) {swing=p;}
};
}
