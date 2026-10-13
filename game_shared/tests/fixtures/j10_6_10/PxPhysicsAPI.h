#pragma once
// SDK-unavailable getter facade. NOT PhysX, NOT a solver or native SDK build.
// No forces/setters/sleep/filter API exists here: production measurement code
// must compile and run entirely through const getters.
#include <cmath>
#include <cstdint>
namespace physx
{
enum PxIDENTITY { PxIdentity };
struct PxVec3
{
    float x=0,y=0,z=0;
    explicit PxVec3(float v=0):x(v),y(v),z(v){}
    PxVec3(float a,float b,float c):x(a),y(b),z(c){}
    PxVec3 operator+(const PxVec3 &v)const{return {x+v.x,y+v.y,z+v.z};}
};
struct PxQuat
{
    float x=0,y=0,z=0,w=1;
    PxQuat()=default;
    PxQuat(float a,float b,float c,float d):x(a),y(b),z(c),w(d){}
    PxQuat operator*(const PxQuat &q)const
    {return {w*q.x+x*q.w+y*q.z-z*q.y,w*q.y-x*q.z+y*q.w+z*q.x,w*q.z+x*q.y-y*q.x+z*q.w,w*q.w-x*q.x-y*q.y-z*q.z};}
    PxVec3 rotate(const PxVec3 &v)const
    {const PxQuat p=(*this)*PxQuat(v.x,v.y,v.z,0)*PxQuat(-x,-y,-z,w);return {p.x,p.y,p.z};}
};
struct PxTransform
{
    PxVec3 p; PxQuat q;
    PxTransform()=default;
    explicit PxTransform(PxIDENTITY){}
    explicit PxTransform(PxVec3 v,PxQuat r={}):p(v),q(r){}
    bool isValid()const
    {const double n=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::isfinite(n)&&std::fabs(n-1)<0.001;}
    PxTransform operator*(const PxTransform &t)const{return PxTransform(p+q.rotate(t.p),q*t.q);}
};
struct PxRigidBodyFlag { enum Enum { eKINEMATIC=1 }; };
struct PxConstraintFlag { enum Enum { eBROKEN=1 }; };
struct PxRevoluteJointFlag { enum Enum { eLIMIT_ENABLED=1 }; };
struct PxJointActorIndex { enum Enum { eACTOR0,eACTOR1 }; };
struct PxD6Axis { enum Enum { eX,eY,eZ,eTWIST,eSWING1,eSWING2 }; };
struct PxD6Motion { enum Enum { eLOCKED,eLIMITED,eFREE }; };
struct PxJointAngularLimitPair { float lower=-0.5f,upper=0.5f; };
struct PxJointLimitCone { float yAngle=0.5f,zAngle=0.5f; };
class PxRigidActor
{
public:
    mutable unsigned reads=0;
    PxTransform pose;
    virtual ~PxRigidActor()=default;
    PxTransform getGlobalPose()const{++reads;return pose;}
};
class PxRigidDynamic:public PxRigidActor
{
public:
    unsigned flags=0; bool sleeping=false;
    PxVec3 linear,angular;
    unsigned getRigidBodyFlags()const{++reads;return flags;}
    bool isSleeping()const{++reads;return sleeping;}
    PxVec3 getLinearVelocity()const{++reads;return linear;}
    PxVec3 getAngularVelocity()const{++reads;return angular;}
};
class PxJoint
{
public:
    mutable unsigned reads=0;
    PxRigidActor *parent=nullptr,*child=nullptr;
    PxTransform a,b; unsigned flags=0; PxVec3 relativeAngular;
    virtual ~PxJoint()=default;
    template<class T>const T *is()const{return dynamic_cast<const T *>(this);}
    unsigned getConstraintFlags()const{++reads;return flags;}
    void getActors(PxRigidActor *&p,PxRigidActor *&c)const{++reads;p=parent;c=child;}
    PxTransform getLocalPose(PxJointActorIndex::Enum i)const{++reads;return i==PxJointActorIndex::eACTOR0?a:b;}
    PxVec3 getRelativeAngularVelocity()const{++reads;return relativeAngular;}
};
class PxD6Joint:public PxJoint
{
public:
    float twist=0,swingY=0,swingZ=0;
    PxD6Motion::Enum motion[6]{PxD6Motion::eLOCKED,PxD6Motion::eLOCKED,PxD6Motion::eLOCKED,PxD6Motion::eLIMITED,PxD6Motion::eLIMITED,PxD6Motion::eLIMITED};
    PxJointAngularLimitPair twistLimit; PxJointLimitCone swingLimit;
    PxJointAngularLimitPair getTwistLimit()const{++reads;return twistLimit;}
    PxJointLimitCone getSwingLimit()const{++reads;return swingLimit;}
    PxD6Motion::Enum getMotion(PxD6Axis::Enum i)const{++reads;return motion[i];}
    float getTwistAngle()const{++reads;return twist;}
    float getSwingYAngle()const{++reads;return swingY;}
    float getSwingZAngle()const{++reads;return swingZ;}
};
class PxRevoluteJoint:public PxJoint
{
public:
    unsigned revoluteFlags=PxRevoluteJointFlag::eLIMIT_ENABLED;
    PxJointAngularLimitPair limit; float angle=0;
    unsigned getRevoluteJointFlags()const{++reads;return revoluteFlags;}
    PxJointAngularLimitPair getLimit()const{++reads;return limit;}
    float getAngle()const{++reads;return angle;}
};
struct PxSimulationStatistics { std::uint32_t nbDiscreteContactPairsTotal=0,nbDiscreteContactPairsWithContacts=0; };
}
