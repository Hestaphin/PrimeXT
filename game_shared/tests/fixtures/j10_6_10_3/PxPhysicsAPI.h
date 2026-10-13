#pragma once
// Mathematical/API recording facade. No simulation, collisions or solver.
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace physx
{
using PxU32=std::uint32_t;constexpr float PxPi=3.14159265358979323846f;
struct PxIdentityType {};constexpr PxIdentityType PxIdentity{};
struct PxVec3
{
    float x=0,y=0,z=0;PxVec3()=default;explicit PxVec3(float v):x(v),y(v),z(v){};PxVec3(float a,float b,float c):x(a),y(b),z(c){}
    PxVec3 operator+(PxVec3 v) const {return {x+v.x,y+v.y,z+v.z};}PxVec3 operator-(PxVec3 v) const {return {x-v.x,y-v.y,z-v.z};}PxVec3 operator-() const {return {-x,-y,-z};}PxVec3 operator*(float s) const {return {x*s,y*s,z*s};}PxVec3 &operator*=(float s) {x*=s;y*=s;z*=s;return *this;}
    float dot(PxVec3 v) const {return x*v.x+y*v.y+z*v.z;}PxVec3 cross(PxVec3 v) const {return {y*v.z-z*v.y,z*v.x-x*v.z,x*v.y-y*v.x};}
    float magnitude() const {return std::sqrt(dot(*this));}PxVec3 getNormalized() const {return *this*(1/magnitude());}bool isFinite() const {return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z);}
};
struct PxMat33 {PxVec3 a,b,c;PxMat33(PxVec3 x,PxVec3 y,PxVec3 z):a(x),b(y),c(z){}};
struct PxQuat
{
    float x=0,y=0,z=0,w=1;PxQuat()=default;PxQuat(float a,float b,float c,float d):x(a),y(b),z(c),w(d){}
    explicit PxQuat(const PxMat33 &m)
    {
        const float trace=m.a.x+m.b.y+m.c.z;
        if(trace>0) {const float s=std::sqrt(trace+1)*2;w=s/4;x=(m.b.z-m.c.y)/s;y=(m.c.x-m.a.z)/s;z=(m.a.y-m.b.x)/s;}
        else if(m.a.x>m.b.y&&m.a.x>m.c.z) {const float s=std::sqrt(1+m.a.x-m.b.y-m.c.z)*2;w=(m.b.z-m.c.y)/s;x=s/4;y=(m.b.x+m.a.y)/s;z=(m.c.x+m.a.z)/s;}
        else if(m.b.y>m.c.z) {const float s=std::sqrt(1+m.b.y-m.a.x-m.c.z)*2;w=(m.c.x-m.a.z)/s;x=(m.b.x+m.a.y)/s;y=s/4;z=(m.c.y+m.b.z)/s;}
        else {const float s=std::sqrt(1+m.c.z-m.a.x-m.b.y)*2;w=(m.a.y-m.b.x)/s;x=(m.c.x+m.a.z)/s;y=(m.c.y+m.b.z)/s;z=s/4;}
    }
    PxQuat operator*(PxQuat q) const {return {w*q.x+x*q.w+y*q.z-z*q.y,w*q.y-x*q.z+y*q.w+z*q.x,w*q.z+x*q.y-y*q.x+z*q.w,w*q.w-x*q.x-y*q.y-z*q.z};}
    PxVec3 rotate(PxVec3 v) const {const float tx=2*(y*v.z-z*v.y),ty=2*(z*v.x-x*v.z),tz=2*(x*v.y-y*v.x);return {v.x+w*tx+y*tz-z*ty,v.y+w*ty+z*tx-x*tz,v.z+w*tz+x*ty-y*tx};}
    bool isUnit() const {return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&std::isfinite(w)&&std::fabs(x*x+y*y+z*z+w*w-1)<.001f;}
};
struct PxTransform
{
    PxVec3 p;PxQuat q;PxTransform()=default;explicit PxTransform(PxIdentityType){};explicit PxTransform(PxVec3 v):p(v){};PxTransform(PxVec3 v,PxQuat r):p(v),q(r){}
    PxTransform operator*(PxTransform t) const {return {p+q.rotate(t.p),q*t.q};}PxTransform getInverse() const {const PxQuat i{-q.x,-q.y,-q.z,q.w};return {i.rotate(-p),i};}bool isValid() const {return p.isFinite()&&q.isUnit();}
};
struct PxTolerancesScale {float length=1,speed=10;};struct PxScene;struct PxSceneDesc;
struct PxFilterData {PxU32 word0=0,word1=0,word2=0,word3=0;PxFilterData()=default;PxFilterData(PxU32 a,PxU32 b,PxU32 c,PxU32 d):word0(a),word1(b),word2(c),word3(d){}};
struct PxFilterFlag {enum Enum {eDEFAULT=0,eSUPPRESS=1,eCALLBACK=2};};using PxFilterFlags=PxFilterFlag::Enum;using PxFilterObjectAttributes=PxU32;using PxPairFlags=PxU32;
struct PxShapeFlag {enum Enum {eSCENE_QUERY_SHAPE=1,eSIMULATION_SHAPE=2};};struct PxRigidBodyFlag {enum Enum {eKINEMATIC=1,eENABLE_CCD=4};};
struct PxConstraintFlag {enum Enum {eCOLLISION_ENABLED=1,eDRIVE_LIMITS_ARE_FORCES=2,ePROJECTION=4,eBROKEN=8};};
struct PxD6Axis {enum Enum {eX,eY,eZ,eTWIST,eSWING1,eSWING2};};struct PxD6Motion {enum Enum {eLOCKED,eLIMITED,eFREE};};struct PxD6Drive {enum Enum {eX,eY,eZ,eSWING,eTWIST,eSLERP};};
struct PxRevoluteJointFlag {enum Enum {eLIMIT_ENABLED=1};};struct PxJointActorIndex {enum Enum {eACTOR0,eACTOR1};};struct PxForceMode {enum Enum {eIMPULSE};};
struct PxJointAngularLimitPair {float lower,upper;PxJointAngularLimitPair(float a,float b):lower(a),upper(b){}};struct PxJointLimitCone {float yAngle,zAngle;PxJointLimitCone(float a,float b):yAngle(a),zAngle(b){}};
struct PxD6JointDrive {float stiffness=0,damping=0,forceLimit=0;bool acceleration=false;PxD6JointDrive()=default;PxD6JointDrive(float s,float d,float f,bool a):stiffness(s),damping(d),forceLimit(f),acceleration(a){}};
struct PxBoxGeometry {PxVec3 halfExtents;explicit PxBoxGeometry(PxVec3 v):halfExtents(v){};bool isValid() const {return halfExtents.isFinite()&&halfExtents.x>0&&halfExtents.y>0&&halfExtents.z>0;}};
struct PxSphereGeometry {float radius;explicit PxSphereGeometry(float r):radius(r){};bool isValid() const {return std::isfinite(radius)&&radius>0;}};
struct PxCapsuleGeometry {float radius,halfHeight;PxCapsuleGeometry(float r,float h):radius(r),halfHeight(h){};bool isValid() const {return std::isfinite(radius)&&std::isfinite(halfHeight)&&radius>0&&halfHeight>=0;}};
struct PxMaterial {float staticFriction=0,dynamicFriction=0,restitution=0;void release();};struct PxBounds3 {PxVec3 minimum,maximum;bool isValid() const {return minimum.isFinite()&&maximum.isFinite()&&minimum.x<=maximum.x&&minimum.y<=maximum.y&&minimum.z<=maximum.z;}};
struct PxShape
{
    void *userData=nullptr;PxFilterData filter;float contactOffset=0,restOffset=0;unsigned flags=3;int geometry=0;PxVec3 dimensions;
    void setContactOffset(float f) {contactOffset=f;}void setRestOffset(float f) {restOffset=f;}float getContactOffset() const {return contactOffset;}float getRestOffset() const {return restOffset;}
    void setFlag(PxShapeFlag::Enum f,bool on) {flags=on?flags|unsigned(f):flags&~unsigned(f);}void setSimulationFilterData(PxFilterData f) {filter=f;}PxFilterData getSimulationFilterData() const {return filter;}
};
struct PxActor {void *userData=nullptr;virtual ~PxActor()=default;};
struct PxRigidActor:PxActor {PxTransform pose;explicit PxRigidActor(PxTransform p):pose(p){};PxTransform getGlobalPose() const {return pose;}void setGlobalPose(PxTransform p,bool=true) {pose=p;}PxBounds3 getWorldBounds(float=1.01f) const {return {};}void setActorFlag(int,bool) {}};
struct PxRigidDynamic:PxRigidActor
{
    explicit PxRigidDynamic(PxTransform p):PxRigidActor(p){};bool released=false,sleeping=false;float linearDamping=0,angularDamping=0,maxLinearVelocity=0,maxAngularVelocity=0,maxDepenetrationVelocity=0,sleepThreshold=0,mass=0;
    PxU32 positionIterations=0,velocityIterations=0;unsigned flags=0;std::string name;std::vector<std::unique_ptr<PxShape>> shapes;
    struct FlagSet {unsigned value;bool isSet(PxRigidBodyFlag::Enum f) const {return (value&unsigned(f))!=0;}unsigned operator&(PxRigidBodyFlag::Enum f) const {return value&unsigned(f);}};
    void setLinearDamping(float f) {linearDamping=f;}void setAngularDamping(float f) {angularDamping=f;}void setSolverIterationCounts(PxU32 p,PxU32 v) {positionIterations=p;velocityIterations=v;}
    void setMaxLinearVelocity(float f) {maxLinearVelocity=f;}void setMaxAngularVelocity(float f) {maxAngularVelocity=f;}void setMaxDepenetrationVelocity(float f) {maxDepenetrationVelocity=f;}void setSleepThreshold(float f) {sleepThreshold=f;}
    // Mirror the documented SDK flag transition, not physical integration.
    void setRigidBodyFlag(PxRigidBodyFlag::Enum f,bool on) {flags=on?flags|unsigned(f):flags&~unsigned(f);if(flags&PxRigidBodyFlag::eKINEMATIC) flags&=~unsigned(PxRigidBodyFlag::eENABLE_CCD);}
    inline static void (*releaseObserver)(PxRigidDynamic *)=nullptr;
    void setName(const char *s) {name=s;}void release() {if(releaseObserver) releaseObserver(this);shapes.clear();released=true;}
    void getSolverIterationCounts(PxU32 &p,PxU32 &v) const {p=positionIterations;v=velocityIterations;}PxU32 getShapes(PxShape **out,PxU32 n) const {if(!n||shapes.empty()) return 0;*out=shapes[0].get();return 1;}
    float getLinearDamping() const {return linearDamping;}float getAngularDamping() const {return angularDamping;}float getMaxLinearVelocity() const {return maxLinearVelocity;}float getMaxAngularVelocity() const {return maxAngularVelocity;}float getMaxDepenetrationVelocity() const {return maxDepenetrationVelocity;}float getSleepThreshold() const {return sleepThreshold;}
    FlagSet getRigidBodyFlags() const {return {flags};}bool isSleeping() const {return sleeping;}PxVec3 getLinearVelocity() const {return {};}PxVec3 getAngularVelocity() const {return {};}float getMass() const {return mass;}void wakeUp() {sleeping=false;}void setLinearVelocity(PxVec3) {}void setAngularVelocity(PxVec3) {}
};
using PxRigidBody=PxRigidDynamic;
struct PxJoint
{
    virtual ~PxJoint()=default;PxRigidActor *parent=nullptr,*child=nullptr;PxTransform frame0,frame1;unsigned flags=0;float projectionLinear=0,projectionAngular=0;bool released=false;
    template<class T> T *is() {return dynamic_cast<T *>(this);}template<class T> const T *is() const {return dynamic_cast<const T *>(this);}
    unsigned getConstraintFlags() const {return flags;}void getActors(PxRigidActor *&a,PxRigidActor *&b) const {a=parent;b=child;}PxTransform getLocalPose(PxJointActorIndex::Enum i) const {return i==PxJointActorIndex::eACTOR0?frame0:frame1;}PxVec3 getRelativeAngularVelocity() const {return {};}
    float getProjectionLinearTolerance() const {return projectionLinear;}float getProjectionAngularTolerance() const {return projectionAngular;}
    void setConstraintFlag(PxConstraintFlag::Enum f,bool on) {flags=on?flags|unsigned(f):flags&~unsigned(f);}void setProjectionLinearTolerance(float f) {projectionLinear=f;}void setProjectionAngularTolerance(float f) {projectionAngular=f;}void release() {released=true;}
};
struct PxD6Joint:PxJoint
{
    PxD6Motion::Enum motions[6]{PxD6Motion::eFREE,PxD6Motion::eFREE,PxD6Motion::eFREE,PxD6Motion::eFREE,PxD6Motion::eFREE,PxD6Motion::eFREE};unsigned writes[6]{};PxJointAngularLimitPair twist{-1,1};PxJointLimitCone swing{1,1};PxD6JointDrive drives[6];PxVec3 driveLinear,driveAngular;bool autowake=true;
    PxD6Motion::Enum getMotion(PxD6Axis::Enum a) const {return motions[a];}PxJointAngularLimitPair getTwistLimit() const {return twist;}PxJointLimitCone getSwingLimit() const {return swing;}float getTwistAngle() const {return 0;}float getSwingYAngle() const {return 0;}float getSwingZAngle() const {return 0;}PxD6JointDrive getDrive(PxD6Drive::Enum d) const {return drives[d];}
    void setMotion(PxD6Axis::Enum a,PxD6Motion::Enum m) {motions[a]=m;++writes[a];}void setTwistLimit(PxJointAngularLimitPair p) {twist=p;}void setSwingLimit(PxJointLimitCone p) {swing=p;}void setDrive(PxD6Drive::Enum d,PxD6JointDrive v) {drives[d]=v;}void setDriveVelocity(PxVec3 l,PxVec3 a,bool w) {driveLinear=l;driveAngular=a;autowake=w;}
};
struct PxRevoluteJoint:PxJoint {PxJointAngularLimitPair limit{-1,1};bool limitEnabled=false;PxJointAngularLimitPair getLimit() const {return limit;}unsigned getRevoluteJointFlags() const {return limitEnabled?1u:0u;}float getAngle() const {return 0;}void setLimit(PxJointAngularLimitPair l) {limit=l;}void setRevoluteJointFlag(PxRevoluteJointFlag::Enum,bool on) {limitEnabled=on;}};
struct PxPhysics
{
    std::vector<std::unique_ptr<PxMaterial>> materials;
    PxMaterial *createMaterial(float s,float d,float r) {materials.emplace_back(new PxMaterial{s,d,r});return materials.back().get();}PxScene *createScene(const PxSceneDesc &);PxTolerancesScale getTolerancesScale() const;void release();
    bool actorFailure=false,jointFailure=false;std::vector<std::unique_ptr<PxRigidDynamic>> actors;std::vector<std::unique_ptr<PxJoint>> joints;
    PxRigidDynamic *createRigidDynamic(PxTransform p) {if(actorFailure) return nullptr;actors.emplace_back(new PxRigidDynamic(p));return actors.back().get();}
};
struct PxRigidActorExt
{
    inline static bool shapeFailure=false;
    template<class G> static PxShape *Shape(PxRigidDynamic &a,const G &,int t,PxVec3 d) {if(shapeFailure) return nullptr;a.shapes.emplace_back(new PxShape);auto *s=a.shapes.back().get();s->geometry=t;s->dimensions=d;return s;}
    static PxShape *createExclusiveShape(PxRigidDynamic &a,const PxBoxGeometry &g,PxMaterial &) {return Shape(a,g,0,g.halfExtents);}static PxShape *createExclusiveShape(PxRigidDynamic &a,const PxSphereGeometry &g,PxMaterial &) {return Shape(a,g,1,{g.radius,0,0});}static PxShape *createExclusiveShape(PxRigidDynamic &a,const PxCapsuleGeometry &g,PxMaterial &) {return Shape(a,g,2,{g.radius,g.halfHeight,0});}
};
struct PxRigidBodyExt {static void addForceAtPos(PxRigidDynamic &,PxVec3,PxVec3,PxForceMode::Enum);inline static bool massResult=true;static bool setMassAndUpdateInertia(PxRigidDynamic &a,float m) {a.mass=m;return massResult;}};
template<class J> inline J *MakeJoint(PxPhysics &p,PxRigidActor *a,PxTransform f0,PxRigidActor *b,PxTransform f1) {if(p.jointFailure) return nullptr;auto *j=new J;j->parent=a;j->child=b;j->frame0=f0;j->frame1=f1;p.joints.emplace_back(j);return j;}
inline PxD6Joint *PxD6JointCreate(PxPhysics &p,PxRigidActor *a,PxTransform f0,PxRigidActor *b,PxTransform f1) {return MakeJoint<PxD6Joint>(p,a,f0,b,f1);}inline PxRevoluteJoint *PxRevoluteJointCreate(PxPhysics &p,PxRigidActor *a,PxTransform f0,PxRigidActor *b,PxTransform f1) {return MakeJoint<PxRevoluteJoint>(p,a,f0,b,f1);}
struct PxSimulationFilterCallback
{
    virtual ~PxSimulationFilterCallback()=default;
    virtual PxFilterFlags pairFound(PxU32,PxFilterObjectAttributes,PxFilterData,const PxActor *,const PxShape *,PxFilterObjectAttributes,PxFilterData,const PxActor *,const PxShape *,PxPairFlags &)=0;
    virtual void pairLost(PxU32,PxFilterObjectAttributes,PxFilterData,PxFilterObjectAttributes,PxFilterData,bool)=0;virtual bool statusChange(PxU32 &,PxPairFlags &,PxFilterFlags &)=0;
};
// Additional declarations are only used for native capture -fsyntax-only.
struct PxSimulationStatistics {unsigned nbDiscreteContactPairsTotal=0,nbDiscreteContactPairsWithContacts=0;};struct PxContactPairFlag {enum Enum {eREMOVED_SHAPE_0=1,eREMOVED_SHAPE_1=2};};
struct PxPairFlag {enum Enum {eCONTACT_DEFAULT=1,eTRIGGER_DEFAULT=2,eDETECT_CCD_CONTACT=4,eNOTIFY_TOUCH_FOUND=8,eNOTIFY_TOUCH_PERSISTS=16,eNOTIFY_CONTACT_POINTS=32,eNOTIFY_TOUCH_CCD=64,eCONTACT_EVENT_POSE=128,eMODIFY_CONTACTS=256};};struct PxSceneFlag {enum Enum {eENABLE_CCD=1};};struct PxActorFlag {enum Enum {eSEND_SLEEP_NOTIFIES=1};};
inline bool PxFilterObjectIsTrigger(PxFilterObjectAttributes a) {return (a&1)!=0;}
struct PxConstraintInfo {};struct PxContactPairHeader {};struct PxTriggerPair {};struct PxContactPairPoint {float separation=0;};struct PxContactPair {unsigned flags=0,contactCount=0;PxU32 extractContacts(PxContactPairPoint *,PxU32) const;};
struct PxSimulationEventCallback {virtual ~PxSimulationEventCallback()=default;virtual void onConstraintBreak(PxConstraintInfo *,PxU32)=0;virtual void onWake(PxActor **,PxU32)=0;virtual void onSleep(PxActor **,PxU32)=0;virtual void onTrigger(PxTriggerPair *,PxU32)=0;virtual void onAdvance(const PxRigidBody *const *,const PxTransform *,const PxU32)=0;virtual void onContact(const PxContactPairHeader &,const PxContactPair *,PxU32)=0;};
struct PxDefaultAllocator {};struct PxDefaultErrorCallback {};struct PxFoundation {void release();};struct PxDefaultCpuDispatcher {void release();};using Shader=PxFilterFlags (*)(PxFilterObjectAttributes,PxFilterData,PxFilterObjectAttributes,PxFilterData,PxPairFlags &,const void *,PxU32);
struct PxSceneDesc {explicit PxSceneDesc(PxTolerancesScale) {}PxVec3 gravity;unsigned flags=0;PxDefaultCpuDispatcher *cpuDispatcher=nullptr;PxSimulationFilterCallback *filterCallback=nullptr;PxSimulationEventCallback *simulationEventCallback=nullptr;Shader filterShader=nullptr;};
struct PxScene {void addActor(PxActor &);void simulate(float);void fetchResults(bool);void getSimulationStatistics(PxSimulationStatistics &);void release();};struct PxRigidStatic:PxRigidActor {using PxRigidActor::PxRigidActor;void release();};struct PxPlane {PxPlane(float,float,float,float) {}};
PxFoundation *PxCreateFoundation(unsigned,PxDefaultAllocator &,PxDefaultErrorCallback &);PxPhysics *PxCreatePhysics(unsigned,PxFoundation &,PxTolerancesScale,bool,void *);bool PxInitExtensions(PxPhysics &,void *);void PxCloseExtensions();PxDefaultCpuDispatcher *PxDefaultCpuDispatcherCreate(unsigned);PxRigidStatic *PxCreatePlane(PxPhysics &,PxPlane,PxMaterial &);
}
#define PX_PHYSICS_VERSION 0x04010000
