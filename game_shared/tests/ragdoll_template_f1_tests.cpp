#include "ragdoll_template.h"
#include "fixtures/j10_6_10_1/barney_skeleton.h"
#include <cassert>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>

using namespace ragtemplate;
static unsigned checks=0;
static void Check(bool value,const char *name) { ++checks;if(!value) { std::cerr<<"FAIL "<<name<<'\n';std::abort(); } }
static std::string Read(const std::filesystem::path &p) { std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}}; }
static void Reject(const std::string &s,const char *name)
{
    Template out=Builtins()[0];const std::string before=Serialize(out);std::string error;
    Check(!Parse(s,out,&error)&&!error.empty()&&Serialize(out)==before,name);
}
int main(int argc,char **argv)
{
    const auto &presets=Builtins();Check(presets.size()>=3&&presets[0].id=="human_standard"&&presets[1].id=="free_rotation"&&presets[2].id=="custom","legacy presets retain their identities");
    for(const auto &t:presets)
    {
        Template parsed;std::string error;
        Check(Validate(t,&error),"valid built-in");Check(Parse(Serialize(t),parsed,&error)&&Serialize(parsed)==Serialize(t),"template round trip");
    }
    const std::string text=Serialize(presets[0]);
    Reject(text+" junk","trailing garbage");Reject(text.substr(0,text.size()-2),"truncation");
    Reject("ragdoll_template { template_version 8 id x name x role root { alias root } }","version rejection");
    Reject("ragdoll_template { id x name x role root { alias root } }","missing version");
    Reject("ragdoll_template { template_version 1 template_version 1 id x name x role root { alias root } }","duplicate key");
    Reject("ragdoll_template { template_version 1 id x name x mass_budget nan role root { alias root } }","nonfinite");
    Reject("ragdoll_template { template_version 1 id x name x mass_budget 70junk role root { alias root } }","numeric suffix");
    Reject("ragdoll_template { template_version 1 id x name x solver_iterations 16 role root { alias root } }","unsupported tuning rejected");
    Reject("ragdoll_template { template_version 1 id x name x role root { alias root translation free } }","free translation forbidden");
    Reject("ragdoll_template { template_version 1 id x name x role root { alias root mass_weight 1 mass_weight 2 } }","duplicate role field");
    Reject("ragdoll_template { template_version 1 id x name x role root { alias root } role ROOT { parent root alias head } }","duplicate role id");
    Reject(std::string(kMaxFileBytes+1,' '),"file limit");
    auto bad=presets[0];bad.roles[0].parent="head";std::string error;
    Check(!Validate(bad,&error),"role cycle");bad=presets[0];bad.roles[1].parent="missing";Check(!Validate(bad,&error),"unknown parent");
    bad=presets[0];bad.roles[0].massWeight=std::numeric_limits<float>::infinity();Check(!Validate(bad,&error),"infinite weight");
    bad=presets[0];bad.roles[0].aliases.push_back("bip01 pelvis");Check(!Validate(bad,&error),"duplicate casefolded alias");
    auto bones=BarneySkeleton();std::vector<Mapping> mapping;
    Check(Resolve(presets[0],bones,1,mapping,&error)&&mapping.size()==20,"actual Barney alias/topology mapping");
    Check(mapping[0].boneIndex==1&&mapping[3].boneIndex==11&&mapping[5].boneIndex==14&&mapping[6].boneIndex==15,"Barney root/chest/upperarm/forearm");
    auto valve=bones;
    for(size_t i=0;i<mapping.size();++i) if(mapping[i].boneIndex>=0) valve[mapping[i].boneIndex].name=presets[0].roles[i].aliases[1];
    Check(Resolve(presets[0],valve,1,mapping,&error),"ValveBiped aliases with the same checked hierarchy");
    Template generic=presets[2];generic.roles[0].aliases={"root"};
    Role limb;limb.id="limb";limb.parent="root";limb.aliases={"limb"};generic.roles.push_back(limb);
    Bone genericRoot;genericRoot.name="root";Bone genericLimb;genericLimb.name="limb";genericLimb.parent=0;genericLimb.localOrigin={4,0,0};
    Check(Resolve(generic,{genericRoot,genericLimb},1,mapping,&error)&&mapping[1].boneIndex==1,"custom generic mapping");
    Check(Resolve(presets[0],bones,1,mapping,&error),"restore Barney mapping");
    auto ambiguous=bones;ambiguous.push_back(bones[1]);ambiguous.back().name="ValveBiped.Bip01_Pelvis";
    Check(!Resolve(presets[0],ambiguous,1,mapping,&error)&&error.find("ambiguous")!=std::string::npos,"ambiguous aliases");
    auto explicitTemplate=presets[0];explicitTemplate.roles[0].boneOverride="Bip01 Pelvis";
    Check(Resolve(explicitTemplate,ambiguous,1,mapping,&error),"explicit disambiguation");
    explicitTemplate.roles[0].boneOverride="missing";Check(!Resolve(explicitTemplate,bones,1,mapping,&error),"missing override");
    auto wrong=bones;wrong[14].parent=2;Check(!Resolve(presets[0],wrong,1,mapping,&error),"topology contradiction");
    wrong=bones;wrong[1].parent=2;Check(!Resolve(presets[0],wrong,1,mapping,&error),"skeleton cycle");
    wrong=bones;wrong[1].parent=999;Check(!Resolve(presets[0],wrong,1,mapping,&error),"invalid skeleton parent");
    wrong=bones;wrong[0].name=wrong[1].name;Check(!Resolve(presets[0],wrong,1,mapping,&error),"duplicate bone name");
    auto optional=bones;optional[16].name="unmapped hand";Check(Resolve(presets[0],optional,1,mapping,&error)&&mapping[7].boneIndex==-1,"missing optional role");
    optional=bones;optional[14].name="unmapped upperarm";Check(!Resolve(presets[0],optional,1,mapping,&error),"missing required role");
    physicsasset::AssetDef empty;empty.sourcePath="models/barney.phys";Candidate standard,repeat,simple,detailed,free;
    Check(Generate(presets[0],bones,1,empty,ComposeMode::ReplaceRagdoll,standard,&error),"generate standard");
    Check(standard.asset.bodies.size()==15&&standard.asset.joints.size()==14,"standard body/joint count");
    double mass=0;for(const auto &b:standard.asset.bodies) mass+=b.mass;
    Check(std::fabs(mass-70)<0.0001,"mass budget normalization");
    Check(Generate(presets[0],bones,1,empty,ComposeMode::ReplaceRagdoll,repeat,&error)&&physicsasset::Serialize(repeat.asset)==physicsasset::Serialize(standard.asset),"determinism");
    Check(Generate(presets[0],bones,0,empty,ComposeMode::ReplaceRagdoll,simple,&error)&&simple.asset.bodies.size()==11,"simple detail");
    Check(Generate(presets[0],bones,2,empty,ComposeMode::ReplaceRagdoll,detailed,&error)&&detailed.asset.bodies.size()==18,"detailed detail");
    Check(Generate(presets[1],bones,1,empty,ComposeMode::ReplaceRagdoll,free,&error),"generate free rotation");
    for(const auto &j:free.asset.joints) Check(j.twistMotion==physicsasset::AngularMotion::Free&&j.swing1Motion==physicsasset::AngularMotion::Free&&j.swing2Motion==physicsasset::AngularMotion::Free,"resolved free DOFs");
    physicsasset::AssetDef restored;
    Check(physicsasset::Parse(physicsasset::Serialize(free.asset),restored,&error)&&physicsasset::Serialize(restored)==physicsasset::Serialize(free.asset),"runtime asset round trip without template dependency");
    auto old=free.asset;old.version=3;Check(!physicsasset::Validate(old,&error),"obsolete v3 rejected");
    old=free.asset;old.joints[0].kind=physicsasset::JointKind::Hinge;Check(!physicsasset::Validate(old,&error),"hinges reject D6 angular-mode tuning");
    old=free.asset;old.joints[0].twistMotion=static_cast<physicsasset::AngularMotion>(99);Check(!physicsasset::Validate(old,&error),"invalid runtime mode rejected");
    for(const auto &j:standard.asset.joints) if(j.child=="Bip01 L Arm1"||j.child=="Bip01 R Arm1") Check((j.parent=="Bip01 L Arm"||j.parent=="Bip01 R Arm"),"arms map to torso-supported shoulder physics roles");
    auto invalidBones=bones;invalidBones[1].boundsMin.x=std::numeric_limits<float>::quiet_NaN();
    const std::string before=physicsasset::Serialize(repeat.asset);
    Check(!Generate(presets[0],invalidBones,1,empty,ComposeMode::ReplaceRagdoll,repeat,&error)&&physicsasset::Serialize(repeat.asset)==before,"invalid geometry leaves candidate unchanged");
    invalidBones=bones;invalidBones[1].boundsMax.x=-999;Check(!Generate(presets[0],invalidBones,1,empty,ComposeMode::ReplaceRagdoll,repeat,&error),"inverted bounds");
    auto fallback=bones;fallback[1].hasBounds=false;Check(Generate(presets[0],fallback,1,empty,ComposeMode::ReplaceRagdoll,repeat,&error)&&repeat.observations.size()>1,"fallback shape observation");
    physicsasset::AssetDef authored=standard.asset;
    physicsasset::CollisionDef collision;collision.name="authored collision";collision.vertices={{0,0,0},{1,0,0},{0,1,0}};collision.indices={0,1,2};authored.collisions.push_back(collision);
    authored.bodies[0].name="authored_pelvis";authored.bodies[0].mass=123;for(auto &j:authored.joints) if(j.parent=="Bip01 Pelvis") j.parent="authored_pelvis";
    for(auto &pair:authored.collisionPairs) {if(pair.first=="Bip01 Pelvis") pair.first="authored_pelvis";if(pair.second=="Bip01 Pelvis") pair.second="authored_pelvis";}
    physicsasset::BodyDef part;part.name="authored door";part.bone="Bone03";part.propDrive=physicsasset::PropDriveMode::Driven;part.part="door";part.driveStrength=12;part.driveDamping=3;part.driveMaxForce=99;authored.bodies.push_back(part);
    physicsasset::JointDef hinge;hinge.name="authored hinge";hinge.parent="authored_pelvis";hinge.child=part.name;hinge.kind=physicsasset::JointKind::Hinge;hinge.hingeAxis={0,0,1};hinge.breakDamage=42;authored.joints.push_back(hinge);
    Check(physicsasset::Validate(authored,&error),"authored fixture valid");Candidate protectedCandidate;
    Check(Generate(presets[1],bones,1,authored,ComposeMode::ReplaceRagdoll,protectedCandidate,&error),"replace preserves interactive closure");
    Check(protectedCandidate.asset.collisions.size()==1&&protectedCandidate.asset.collisions[0].vertices[1].x==1,"collision retained");
    Check(protectedCandidate.asset.bodies[0].name=="authored_pelvis"&&protectedCandidate.asset.bodies[0].mass==123&&protectedCandidate.protectedBodies==2,"authored protected anchor retained");
    Check(protectedCandidate.asset.joints[0].name=="authored hinge"&&protectedCandidate.asset.joints[0].breakDamage==42&&protectedCandidate.asset.joints[0].hingeAxis.z==1,"hinge fields retained");
    auto missingProtected=authored;missingProtected.bodies.back().bone="absent prop bone";
    Check(!Generate(presets[0],bones,1,missingProtected,ComposeMode::ReplaceRagdoll,repeat,&error),"protected body requires a real model bone");
    missingProtected=authored;missingProtected.collisions[0].bone="absent collision bone";
    Check(!Generate(presets[0],bones,1,missingProtected,ComposeMode::ReplaceRagdoll,repeat,&error),"preserved collision requires a real model bone");
    Candidate merge;Check(Generate(presets[1],bones,1,authored,ComposeMode::Merge,merge,&error)&&physicsasset::Serialize(merge.asset)==physicsasset::Serialize(authored),"merge retains authored values");
    auto collisionConflict=authored;collisionConflict.bodies.push_back(standard.asset.bodies[0]);
    Check(!Generate(presets[0],bones,1,collisionConflict,ComposeMode::Merge,merge,&error),"ambiguous authored matches rejected");
    Transaction transaction;auto live=authored;const std::string original=physicsasset::Serialize(live);
    transaction.Begin(live);Check(transaction.SetCandidate(protectedCandidate,&error)&&physicsasset::Serialize(live)==original,"preview does not mutate live asset");
    transaction.Cancel();Check(physicsasset::Serialize(live)==original&&!transaction.Apply(live,&error),"cancel rollback");
    transaction.Begin(live);transaction.SetCandidate(protectedCandidate);live.bodies[0].mass=321;
    Check(!transaction.Apply(live,&error)&&live.bodies[0].mass==321,"stale preview does not clobber edits");
    live=authored;transaction.Begin(live);transaction.SetCandidate(protectedCandidate);
    Check(transaction.Apply(live,&error)&&physicsasset::Serialize(live)==physicsasset::Serialize(protectedCandidate.asset),"apply reviewed candidate");
    live.bodies[0].mass=456;Check(!transaction.Undo(live,&error)&&live.bodies[0].mass==456,"undo guards later edits");
    live=protectedCandidate.asset;Check(transaction.Undo(live,&error)&&physicsasset::Serialize(live)==original,"undo restores exact asset");
    const auto parent=argc>1?std::filesystem::path(argv[1]):std::filesystem::current_path();
    const auto folder=parent/("ragtemplate_test_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code filesystemError;Check(std::filesystem::create_directory(folder,filesystemError),"create isolated persistence fixture");
    const auto path=folder/"custom.ragtemplate";
    Check(SaveFile(path.string().c_str(),presets[0],false,&error),"save new template");
    const std::string saved=Read(path);Check(!SaveFile(path.string().c_str(),presets[1],false,&error)&&Read(path)==saved,"exclusive commit preserves existing file");
    Check(SaveFile(path.string().c_str(),presets[1],true,&error),"atomic confirmed replacement");
    Template loaded;Check(LoadFile(path.string().c_str(),loaded,&error)&&Serialize(loaded)==Serialize(presets[1]),"import exported template");
    bad=presets[0];bad.massBudget=-1;const std::string final=Read(path);
    Check(!SaveFile(path.string().c_str(),bad,true,&error)&&Read(path)==final,"invalid template cannot overwrite file");
    Check(!SaveFile((folder/"missing/file.ragtemplate").string().c_str(),presets[0],false,&error),"write failure");
    std::filesystem::remove_all(folder,filesystemError);Check(!filesystemError,"persistence fixture cleanup");
    std::cout<<"F1 template core: "<<checks<<" checks PASS (actual Barney skeleton fixture; no solver/GUI certification)\n";
}
