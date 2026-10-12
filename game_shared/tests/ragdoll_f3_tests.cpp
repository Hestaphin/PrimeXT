#include "physics_asset_format.h"
#include "physics_asset_settings.h"
#include "physics_asset_contact_policy.h"
#include "physics_asset_scale.h"
#include "ragdoll_template.h"
#include "ragdoll_geometry.h"
#include "fixtures/j10_6_10_1/barney_skeleton.h"
#include <cstdlib>
#include <functional>
#include <iostream>
#include <limits>
using namespace physicsasset;
static unsigned checks=0;
static void Check(bool v,const char *name) {++checks;if(!v) {std::cerr<<"FAIL "<<name<<'\n';std::abort();}}
static AssetDef Chain(unsigned count=4)
{
    AssetDef a;for(unsigned i=0;i<count;++i) {BodyDef b;b.name=b.bone="body"+std::to_string(i);b.shape.type=ShapeType::Sphere;b.shape.radius=1;a.bodies.push_back(b);if(i) {JointDef j;j.name="joint"+std::to_string(i);j.parent=a.bodies[i-1].name;j.child=b.name;a.joints.push_back(j);}}return a;
}
static void Reject(const std::string &text,const char *name) {auto a=Chain();const auto before=Serialize(a);std::string error;Check(!Parse(text,a,&error)&&!error.empty()&&Serialize(a)==before,name);}
int main()
{
    std::string error;const auto engine=Chain();Check(kCurrentVersion==5&&Validate(engine,&error),"v5 engine defaults validate");
    Check(engine.simulation.positionIterations==16&&engine.simulation.velocityIterations==4&&engine.simulation.projection==ProjectionMode::Off,"declared solver and projection defaults");
    for(int version:{0,1,2,3,4,6}) {auto a=engine;const auto before=Serialize(a);Check(!Parse("physics_asset { version "+std::to_string(version)+" body root { bone root } }",a,&error)&&error.find("Regenerate")!=std::string::npos&&Serialize(a)==before,"obsolete versions fail transactionally with regeneration diagnostic");}
    for(const auto &entry:std::vector<std::pair<std::string,const char *>>{
        {"body root { bone root }","version required"},{"version 5 version 5 body root { bone root }","duplicate version"},{"version 5 simulation {} simulation {} body root { bone root }","duplicate profile"},
        {"version 5 simulation { sleep_threshold 1 sleep_threshold 2 } body root { bone root }","duplicate setting"},{"version 5 simulation { max_angular_velocity nan } body root { bone root }","nonfinite"},{"version 5 simulation { solver_position_iterations 16.5 } body root { bone root }","fractional solver"},
        {"version 5 self_collision off self_collision custom body root { bone root }","duplicate contact mode"},{"version 5 body root { bone root mass 1 mass 2 }","duplicate body field"},{"version 5 body root { bone root ccd 2 }","invalid CCD"},
        {"version 5 body root { bone root } collision_pair root root allow","self pair"},{"version 5 body root { bone root } collision_pair root missing allow","unknown pair"},{"version 5 simulation { projection inherit } body root { bone root }","profile cannot inherit"}}) Reject("physics_asset { "+entry.first+" }",entry.second);
    auto a=engine;a.simulation.linearDamping=.25f;a.simulation.angularDamping=.5f;a.simulation.positionIterations=24;a.simulation.velocityIterations=8;a.simulation.maxLinearVelocity=123.125f;a.simulation.maxAngularVelocity=17;a.simulation.maxDepenetrationVelocity=72;a.simulation.sleepThreshold=.75f;
    a.simulation.contactOffset=.25f;a.simulation.restOffset=.125f;a.simulation.ccd=false;a.simulation.angularResistance=4;a.simulation.maxTorque=50;a.simulation.projection=ProjectionMode::On;a.simulation.projectionLinearTolerance=.125f;a.simulation.projectionAngularTolerance=4;
    a.selfCollision=SelfCollisionMode::Selective;a.collisionPairs={{"body0","body3",true},{"body1","body2",false}};a.bodies[1].linearDamping=.7f;a.bodies[1].tuning.positionIterations=32;a.bodies[1].tuning.maxAngularVelocity=9;a.bodies[1].tuning.ccd=1;a.bodies[1].tuning.contactOffset=.5f;a.joints[0].tuning.angularResistance=0;a.joints[0].tuning.projection=ProjectionMode::Off;
    AssetDef round;Check(Validate(a,&error)&&Parse(Serialize(a),round,&error)&&Serialize(a)==Serialize(round),"all settings/contacts exact deterministic roundtrip");
    const auto body=ResolveBodySettings(a,a.bodies[1]);const auto joint=ResolveJointSettings(a,a.joints[0]);
    Check(body.linearDamping==.7f&&body.angularDamping==.5f&&body.positionIterations==32&&body.velocityIterations==8&&body.maxAngularVelocity==9&&body.maxLinearVelocity==123.125f&&body.ccd&&body.contactOffset==.5f&&body.restOffset==.125f,"override > profile > engine resolution");
    Check(joint.angularResistance==0&&joint.maxTorque==50&&joint.projection==ProjectionMode::Off&&joint.projectionLinearTolerance==.125f,"explicit zero and Off override defaults");
    Check(round.bodies[0].linearDamping==-1&&round.bodies[1].tuning.velocityIterations==-1&&round.joints[0].tuning.maxTorque==-1,"inherit sentinels persist by omission");
    const std::vector<std::function<void(AssetDef&)>> invalid={
        [](auto &v){v.simulation.positionIterations=0;},[](auto &v){v.simulation.velocityIterations=256;},[](auto &v){v.simulation.maxLinearVelocity=0;},[](auto &v){v.simulation.angularResistance=1;v.simulation.maxTorque=0;},[](auto &v){v.simulation.projectionAngularTolerance=180;},
        [](auto &v){v.bodies[0].linearDamping=-2;},[](auto &v){v.bodies[0].tuning.maxAngularVelocity=-2;},[](auto &v){v.bodies[0].tuning.maxLinearVelocity=0;},[](auto &v){v.joints[0].tuning.angularResistance=-2;},[](auto &v){v.joints[0].tuning.projectionLinearTolerance=0;},
        [](auto &v){v.simulation.restOffset=v.simulation.contactOffset;},[](auto &v){v.bodies[0].tuning.contactOffset=0;},[](auto &v){v.selfCollision=SelfCollisionMode(99);},[](auto &v){v.collisionPairs.push_back({"BODY3","BODY0",false});}};
    for(const auto &change:invalid) {auto v=a;change(v);Check(!Validate(v,&error)&&!error.empty(),"programmatic invalid tuning rejected");}
    Check(Validate(Chain(256),&error)&&!Validate(Chain(257),&error),"full declaration identity limit supports 256 bodies");
    ContactGroup group(a);Check(group.Allows(0,3)&&group.Allows(3,0)&&!group.Allows(1,2)&&!group.Allows(0,2)&&!group.Allows(0,0),"symmetric immutable allow/default-deny table");
    Check(group.Owns(group.Token(3),4)&&!group.Owns(group.Token(3),3)&&!group.Token(4),"encoded index and token address checked");BodyContactToken forged{&group,3};Check(!group.Owns(&forged,4),"forged token rejected");
    auto off=a;off.selfCollision=SelfCollisionMode::Off;ContactGroup disabled(off);Check(!disabled.Active()&&!disabled.Allows(0,3)&&off.collisionPairs[0].enabled,"Off retains authored rows and suppresses all contacts");
    auto many=Chain(80);many.selfCollision=SelfCollisionMode::Custom;many.collisionPairs={{"body1","body79",true}};ContactGroup large(many);Check(large.Allows(1,79)&&large.Token(79)->index==79&&large.Owns(large.Token(79),80),"large rig uses full indices, not a 32-body mask");
    auto scaled=a;Check(ScaleAuthoredAsset(scaled,2)&&scaled.simulation.maxLinearVelocity==2*a.simulation.maxLinearVelocity&&scaled.simulation.sleepThreshold==4*a.simulation.sleepThreshold&&scaled.simulation.angularResistance==4*a.simulation.angularResistance&&scaled.simulation.maxTorque==4*a.simulation.maxTorque&&scaled.simulation.projectionLinearTolerance==2*a.simulation.projectionLinearTolerance,"dimensional tuning scales consistently");
    Check(scaled.simulation.contactOffset==.5f&&scaled.simulation.restOffset==.25f&&scaled.bodies[1].tuning.contactOffset==1&&scaled.bodies[1].tuning.maxAngularVelocity==9&&scaled.bodies[0].tuning.maxLinearVelocity==-1&&scaled.joints[0].tuning.angularResistance==0,"margins resize while angular values/zeros/sentinels remain");
    const auto before=Serialize(scaled);Check(!ScaleAuthoredAsset(scaled,std::numeric_limits<float>::max())&&Serialize(scaled)==before,"overflow resize transactional");
    auto t=ragtemplate::Builtins()[0];t.roles[4].bodyTuning.positionIterations=32;t.roles[4].joint.tuning.angularResistance=6;t.pairRules={{"left_forearm","chest",false}};ragtemplate::Template copy;
    Check(ragtemplate::Parse(ragtemplate::Serialize(t),copy,&error)&&ragtemplate::Serialize(t)==ragtemplate::Serialize(copy),"profile/role/pair template roundtrip");ragtemplate::Candidate c;
    Check(ragtemplate::Generate(t,BarneySkeleton(),1,AssetDef{},ragtemplate::ComposeMode::ReplaceRagdoll,c,&error),"selective policy resolves final rig");
    Check(c.asset.selfCollision==SelfCollisionMode::Selective&&c.asset.collisionPairs.size()==c.asset.bodies.size()*(c.asset.bodies.size()-1)/2,"every generated unordered pair resolved");
    unsigned allowed=0;for(const auto &p:c.asset.collisionPairs) if(p.enabled) ++allowed;Check(allowed>0,"remote separated pairs can contact");
    for(const auto &j:c.asset.joints) Check(!CollisionPairEnabled(c.asset,FindBody(c.asset,j.parent.c_str()),FindBody(c.asset,j.child.c_str())),"direct neighbors excluded by default");
    Check(!CollisionPairEnabled(c.asset,FindBody(c.asset,"Bip01 L Arm2"),FindBody(c.asset,"Bip01 Spine3")),"semantic deny overrides generation");
    auto custom=t;custom.selfCollision=SelfCollisionMode::Custom;custom.pairRules={{"left_forearm","chest",true}};ragtemplate::Candidate manual;
    Check(ragtemplate::Generate(custom,BarneySkeleton(),1,AssetDef{},ragtemplate::ComposeMode::ReplaceRagdoll,manual,&error)&&CollisionPairEnabled(manual.asset,FindBody(manual.asset,"Bip01 L Arm2"),FindBody(manual.asset,"Bip01 Spine3")),"custom explicit pair allowed");
    Check(std::count_if(manual.asset.collisionPairs.begin(),manual.asset.collisionPairs.end(),[](const auto &p){return p.enabled;})==1,"custom unlisted pairs denied");
    auto authored=c.asset;authored.simulation.positionIterations=8;authored.simulation.angularResistance=9;authored.selfCollision=SelfCollisionMode::Off;ragtemplate::Candidate merged;
    Check(ragtemplate::Generate(custom,BarneySkeleton(),1,authored,ragtemplate::ComposeMode::Merge,merged,&error)&&Serialize(merged.asset)==Serialize(authored),"Merge preserves authored profile and pairs");
    auto bad=t;bad.pairRules.push_back({"CHEST","LEFT_FOREARM",true});Check(!ragtemplate::Validate(bad,&error),"reversed semantic duplicates rejected");bad=t;bad.pairRules[0].second="missing";Check(!ragtemplate::Validate(bad,&error),"unknown semantic role pair rejected");
    std::cout<<"F3 v5/profile/override/scaling/selective generation: "<<checks<<" checks PASS (portable logic; no solver)\n";
}
