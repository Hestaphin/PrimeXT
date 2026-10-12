// Real SDK capture target. The recording facade may syntax-check this file,
// but cannot link/run it or supply physical acceptance evidence.
#include "physics_asset_construction.h"
#include "physics_asset_contact_filter.h"
#include "ragdoll_geometry.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <unordered_map>
#include <cstdlib>
using namespace physx;using namespace physicsasset;
struct Events final:PxSimulationEventCallback
{
    float penetration=0;unsigned wake=0,sleep=0;
    void onConstraintBreak(PxConstraintInfo *,PxU32) override {}void onWake(PxActor **,PxU32 n) override {wake+=n;}void onSleep(PxActor **,PxU32 n) override {sleep+=n;}
    void onTrigger(PxTriggerPair *,PxU32) override {}void onAdvance(const PxRigidBody *const *,const PxTransform *,const PxU32) override {}
    void onContact(const PxContactPairHeader &,const PxContactPair *pairs,PxU32 count) override
    {for(PxU32 i=0;i<count;++i) if(!(pairs[i].flags&(PxContactPairFlag::eREMOVED_SHAPE_0|PxContactPairFlag::eREMOVED_SHAPE_1))) {PxContactPairPoint points[64];const auto n=pairs[i].extractContacts(points,64);for(PxU32 k=0;k<n;++k) penetration=std::max(penetration,-points[k].separation);}}
};
struct World
{
    PxDefaultAllocator allocator;PxDefaultErrorCallback errors;Events events;AssetContactFilter filter;std::unique_ptr<ContactGroup> group;
    PxFoundation *foundation=nullptr;PxPhysics *physics=nullptr;PxDefaultCpuDispatcher *dispatcher=nullptr;PxMaterial *material=nullptr;PxScene *scene=nullptr;PxRigidStatic *ground=nullptr;
    std::vector<PxRigidDynamic *> actors;std::vector<PxJoint *> joints;bool extensions=false;
    ~World() {for(auto *j:joints) if(j) j->release();for(auto *a:actors) if(a) a->release();if(ground) ground->release();group.reset();if(scene) scene->release();if(material) material->release();if(dispatcher) dispatcher->release();if(extensions) PxCloseExtensions();if(physics) physics->release();if(foundation) foundation->release();}
    bool Init()
    {
        foundation=PxCreateFoundation(PX_PHYSICS_VERSION,allocator,errors);if(!foundation) return false;
        physics=PxCreatePhysics(PX_PHYSICS_VERSION,*foundation,AssetTolerancesScale(),true,nullptr);if(!physics) return false;
        extensions=PxInitExtensions(*physics,nullptr);if(!extensions) return false;
        dispatcher=PxDefaultCpuDispatcherCreate(2);material=CreateAssetMaterial(*physics);if(!dispatcher||!material) return false;
        PxSceneDesc desc(physics->getTolerancesScale());desc.gravity=PxVec3(0,0,-800);desc.flags=PxSceneFlag::eENABLE_CCD;desc.cpuDispatcher=dispatcher;desc.filterCallback=&filter;desc.simulationEventCallback=&events;
        desc.filterShader=[](PxFilterObjectAttributes a,PxFilterData fa,PxFilterObjectAttributes b,PxFilterData fb,PxPairFlags &flags,const void *,PxU32)->PxFilterFlags
        {if(PxFilterObjectIsTrigger(a)||PxFilterObjectIsTrigger(b)) {flags=PxPairFlag::eTRIGGER_DEFAULT;return PxFilterFlag::eDEFAULT;}flags=PxPairFlag::eCONTACT_DEFAULT|PxPairFlag::eDETECT_CCD_CONTACT|PxPairFlag::eNOTIFY_TOUCH_FOUND|PxPairFlag::eNOTIFY_TOUCH_PERSISTS|PxPairFlag::eNOTIFY_CONTACT_POINTS;return AssetPairFilter(fa,fb);};
        scene=physics->createScene(desc);if(!scene) return false;ground=PxCreatePlane(*physics,PxPlane(0,0,1,0),*material);if(!ground) return false;scene->addActor(*ground);return true;
    }
};
static PxVec3 Vector(raggeometry::V v) {return {v.x,v.y,v.z};}
static PxTransform Transform(const raggeometry::Pose &p) {return {Vector(p.p),PxQuat(PxMat33(Vector(p.x),Vector(p.y),Vector(p.z)))};}
static bool Near(float a,float b) {return std::fabs(a-b)<=.0001f*std::max(1.f,std::max(std::fabs(a),std::fabs(b)));}
static bool BodyParity(PxRigidDynamic &actor,PxShape &shape,const AssetDef &a,const BodyDef &b,float scale)
{
    auto s=ResolveBodySettings(a,b);PxU32 p=0,v=0;actor.getSolverIterationCounts(p,v);
    return p==PxU32(s.positionIterations)&&v==PxU32(s.velocityIterations)&&Near(actor.getLinearDamping(),s.linearDamping)&&Near(actor.getAngularDamping(),s.angularDamping)&&Near(actor.getMaxLinearVelocity(),s.maxLinearVelocity*scale)&&Near(actor.getMaxAngularVelocity(),s.maxAngularVelocity)&&Near(actor.getMaxDepenetrationVelocity(),s.maxDepenetrationVelocity*scale)&&Near(actor.getSleepThreshold(),s.sleepThreshold*scale*scale)&&actor.getRigidBodyFlags().isSet(PxRigidBodyFlag::eENABLE_CCD)==s.ccd&&Near(shape.getContactOffset(),s.contactOffset*scale)&&Near(shape.getRestOffset(),s.restOffset*scale);
}
static bool JointParity(PxJoint &joint,const AssetDef &a,const JointDef &d,float scale)
{
    auto s=ResolveJointSettings(a,d);const bool projection=(joint.getConstraintFlags()&PxConstraintFlag::ePROJECTION)!=0;
    auto *j=joint.is<PxD6Joint>();auto *h=joint.is<PxRevoluteJoint>();if(!j&&!h) return false;
    const float linear=j?j->getProjectionLinearTolerance():h->getProjectionLinearTolerance(),angular=j?j->getProjectionAngularTolerance():h->getProjectionAngularTolerance();
    if(projection!=(s.projection==ProjectionMode::On)||!Near(linear,s.projectionLinearTolerance*scale)||!Near(angular,s.projectionAngularTolerance*PxPi/180)) return false;
    if(h) return Near(h->getLimit().lower,d.hingeMin*PxPi/180)&&Near(h->getLimit().upper,d.hingeMax*PxPi/180);
    for(auto axis:{PxD6Axis::eX,PxD6Axis::eY,PxD6Axis::eZ}) if(j->getMotion(axis)!=PxD6Motion::eLOCKED) return false;
    const AngularMotion motions[]={d.twistMotion,d.swing1Motion,d.swing2Motion};
    for(int i=0;i<3;++i) if(j->getMotion(PxD6Axis::Enum(i+3))!=(motions[i]==AngularMotion::Free?PxD6Motion::eFREE:motions[i]==AngularMotion::Locked?PxD6Motion::eLOCKED:PxD6Motion::eLIMITED)) return false;
    for(auto kind:{PxD6Drive::eTWIST,PxD6Drive::eSWING}) {auto drive=j->getDrive(kind);if(!Near(drive.stiffness,0)||!Near(drive.damping,s.angularResistance*scale*scale)||!Near(drive.forceLimit,s.maxTorque*scale*scale)) return false;}
    return Near(j->getTwistLimit().lower,d.twistLow*PxPi/180)&&Near(j->getTwistLimit().upper,d.twistHigh*PxPi/180)&&Near(j->getSwingLimit().yAngle,d.swing1*PxPi/180)&&Near(j->getSwingLimit().zAngle,d.swing2*PxPi/180);
}
int main(int argc,char **argv)
{
    if(argc!=8) {std::cerr<<"Usage: primext-f3-drop asset.phys bind.txt trace.csv angle step scale off|authored\n";return 2;}
    const float angle=std::strtof(argv[4],nullptr),step=std::strtof(argv[5],nullptr),scale=std::strtof(argv[6],nullptr);
    if(!std::isfinite(angle)||!std::isfinite(step)||step<.001f||step>.05f||!std::isfinite(scale)||scale<=0||scale>100) return 2;
    AssetDef a;std::string error;if(!LoadFile(argv[1],a,&error)) {std::cerr<<error<<'\n';return 2;}
    if(a.bodies.empty()) return 2;if(std::string(argv[7])=="off") a.selfCollision=SelfCollisionMode::Off;else if(std::string(argv[7])!="authored") return 2;
    struct Bone {std::string name;int parent;raggeometry::Pose local,world;int state=0;};std::vector<Bone> bones;std::unordered_map<std::string,size_t> byName;
    std::ifstream bind(argv[2]);std::string header;std::getline(bind,header);if(header!="studio_bind_v1") return 2;
    std::string name,line;int parent;raggeometry::V p,r;
    while(std::getline(bind,line)) {std::istringstream row(line);row.imbue(std::locale::classic());if(!(row>>std::quoted(name)>>parent>>p.x>>p.y>>p.z>>r.x>>r.y>>r.z)||!std::isfinite(raggeometry::Len(p))||!std::isfinite(raggeometry::Len(r))) return 2;row>>std::ws;if(!row.eof()||name.empty()||bones.size()>=1024||byName.count(ContactName(name))) return 2;byName.emplace(ContactName(name),bones.size());bones.push_back({name,parent,raggeometry::Studio(r,raggeometry::Mul(p,scale)),{},0});}
    if(bones.empty()||!bind.eof()) return 2;
    auto resolve=[&](auto &&self,size_t i)->bool {auto &b=bones[i];if(b.state==2) return true;if(b.state==1||b.parent< -1||b.parent>=int(bones.size())) return false;b.state=1;if(b.parent>=0&&!self(self,size_t(b.parent))) return false;b.world=b.parent<0?b.local:raggeometry::Concat(bones[size_t(b.parent)].world,b.local);b.state=2;return true;};
    for(size_t i=0;i<bones.size();++i) if(!resolve(resolve,i)) return 2;
    World w;if(!w.Init()) {std::cerr<<"Native SDK initialization failed\n";return 3;}w.group.reset(new ContactGroup(a));
    const auto rotation=Transform(raggeometry::Euler({angle,0,0}));std::vector<PxTransform> bodyReferences,boneReferences;float lowest=1e9f;
    for(size_t i=0;i<a.bodies.size();++i)
    {
        const auto &b=a.bodies[i];const auto found=byName.find(ContactName(b.bone));if(found==byName.end()) {std::cerr<<"Missing matching MDL bone: "<<b.bone<<'\n';return 2;}
        const auto bone=Transform(bones[found->second].world),body=bone*Transform(raggeometry::Euler(b.angles,raggeometry::Mul(b.offset,scale)));boneReferences.push_back(bone);bodyReferences.push_back(body);
        PxRigidDynamic *actor=nullptr;PxShape *shape=nullptr;if(!CreatePrimitiveBody(*w.physics,*w.material,rotation*body,a,i,scale,actor,shape)) return 3;w.actors.push_back(actor);
        shape->userData=const_cast<BodyContactToken *>(w.group->Token(i));shape->setSimulationFilterData(AssetBodyFilterData(1,i,*w.group));actor->setActorFlag(PxActorFlag::eSEND_SLEEP_NOTIFIES,true);
        if(!BodyParity(*actor,*shape,a,b,scale)) {std::cerr<<"Native resolved body getter mismatch\n";return 4;}lowest=std::min(lowest,actor->getWorldBounds().minimum.z);
    }
    for(auto *actor:w.actors) {auto pose=actor->getGlobalPose();pose.p.z+=60*scale-lowest;actor->setGlobalPose(pose);w.scene->addActor(*actor);}
    for(const auto &d:a.joints) {const int i=FindBody(a,d.parent.c_str()),j=FindBody(a,d.child.c_str());auto *joint=CreateAssetJoint(*w.physics,*w.actors[size_t(i)],*w.actors[size_t(j)],bodyReferences[size_t(i)],bodyReferences[size_t(j)],boneReferences[size_t(j)],a,d,scale);if(!joint) return 3;w.joints.push_back(joint);if(!JointParity(*joint,a,d,scale)) {std::cerr<<"Native resolved joint getter mismatch\n";return 4;}}
    size_t root=0,impulse=0;for(size_t i=0;i<a.bodies.size();++i) {bool child=false;for(const auto &d:a.joints) if(ContactName(d.child)==ContactName(a.bodies[i].name)) child=true;if(!child) root=i;if(ContactName(a.bodies[i].bone).find("head")!=std::string::npos) impulse=i;}
    std::ofstream csv(argv[3]);csv.imbue(std::locale::classic());csv<<std::setprecision(9)<<"sim_s,step_s,active,sleeping,root_speed,max_linear,max_angular,max_relative_angular,anchor_error,root_step_delta,penetration_sampled,scene_contact_pairs,wake_events,sleep_events,impulse_applied\n";
    if(!csv) return 2;PxVec3 previous=w.actors[root]->getGlobalPose().p;bool hit=false;float firstSleep=-1;unsigned samples=0;
    for(float time=0;time<18;time+=step)
    {
        if(!hit&&time>=15) {auto *actor=w.actors[impulse];PxRigidBodyExt::addForceAtPos(*actor,PxVec3(0,100*scale*actor->getMass(),40*scale*actor->getMass()),actor->getGlobalPose().p,PxForceMode::eIMPULSE);hit=true;}
        w.events.penetration=0;w.scene->simulate(step);w.scene->fetchResults(true);unsigned active=0,sleeping=0;float linear=0,angular=0,relative=0,anchor=0;
        for(auto *actor:w.actors) {const auto pose=actor->getGlobalPose();if(!pose.isValid()||!actor->getLinearVelocity().isFinite()||!actor->getAngularVelocity().isFinite()) {std::cerr<<"Nonfinite native actor state\n";return 5;}if(actor->isSleeping()) ++sleeping;else ++active;linear=std::max(linear,actor->getLinearVelocity().magnitude());angular=std::max(angular,actor->getAngularVelocity().magnitude());}
        for(auto *joint:w.joints) {PxRigidActor *a0=nullptr,*a1=nullptr;joint->getActors(a0,a1);anchor=std::max(anchor,((a0->getGlobalPose()*joint->getLocalPose(PxJointActorIndex::eACTOR0)).p-(a1->getGlobalPose()*joint->getLocalPose(PxJointActorIndex::eACTOR1)).p).magnitude());relative=std::max(relative,joint->getRelativeAngularVelocity().magnitude());}
        if(!hit&&sleeping==w.actors.size()&&firstSleep<0) firstSleep=time+step;PxSimulationStatistics stats;w.scene->getSimulationStatistics(stats);const auto position=w.actors[root]->getGlobalPose().p;
        csv<<time+step<<','<<step<<','<<active<<','<<sleeping<<','<<w.actors[root]->getLinearVelocity().magnitude()<<','<<linear<<','<<angular<<','<<relative<<','<<anchor<<','<<(position-previous).magnitude()<<','<<w.events.penetration<<','<<stats.nbDiscreteContactPairsWithContacts<<','<<w.events.wake<<','<<w.events.sleep<<','<<hit<<'\n';previous=position;++samples;
    }
    csv.flush();if(!csv) return 6;std::cout<<"CAPTURE_COMPLETE samples="<<samples<<" first_all_sleep_s="<<firstSleep<<" impulse_body="<<a.bodies[impulse].name<<" (physical quality requires review; no mesh/game/FPS evidence)\n";return 0;
}
