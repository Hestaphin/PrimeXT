#include "ragdoll_template.h"
#include "physics_asset_contact_policy.h"
#include "fixtures/j10_6_10_5/skeletons.h"
#include "fixtures/j10_6_10_1/barney_skeleton.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numeric>
using namespace ragtemplate;
static unsigned checks=0;
static void Check(bool v,const char *s){++checks;if(!v){std::cerr<<"FAIL "<<s<<'\n';std::abort();}}
static const Template &Preset(const char *id){for(const auto &t:Builtins())if(t.id==id)return t;std::abort();}
static double Mass(const physicsasset::AssetDef &a){double m=0;for(const auto &b:a.bodies)m+=b.mass;return m;}
static void CheckRig(const Candidate &c,float mass)
{
    std::string error;Check(physicsasset::Validate(c.asset,&error),"resolved rig validates");
    Check(std::fabs(Mass(c.asset)-mass)<0.0001,"regional/expanded chains preserve mass budget");
    Check(c.asset.joints.size()+1==c.asset.bodies.size(),"generated articulation is one connected tree");
    physicsasset::AssetDef copy;Check(physicsasset::Parse(physicsasset::Serialize(c.asset),copy,&error)&&physicsasset::Serialize(copy)==physicsasset::Serialize(c.asset),"self-contained v5 runtime round trip");
    physicsasset::ContactGroup table(c.asset);bool symmetric=true,neighbors=true;
    for(size_t a=0;a<c.asset.bodies.size();++a)for(size_t b=0;b<c.asset.bodies.size();++b)symmetric=symmetric&&table.Allows(a,b)==table.Allows(b,a);
    for(const auto &j:c.asset.joints)neighbors=neighbors&&!table.Allows(physicsasset::FindBody(c.asset,j.parent.c_str()),physicsasset::FindBody(c.asset,j.child.c_str()));
    Check(symmetric&&neighbors,"selective matrix is symmetric and denies connected neighbors");
}
int main(int argc,char **argv)
{
    std::string error;physicsasset::AssetDef empty;Candidate c,repeat;Check(Builtins().size()==7,"seven practical presets");
    for(const auto &t:Builtins()){Template copy;Check(Validate(t,&error),"preset validates");Check(Parse(Serialize(t),copy,&error)&&Serialize(copy)==Serialize(t),"all preset keys/chain fields round trip");}
    const auto quad=F5Quadruped();const auto creature=F5Creature();
    for(int detail=0;detail<=2;++detail)
    {
        Check(Generate(Preset("quadruped"),quad,detail,empty,ComposeMode::ReplaceRagdoll,c,&error),"quadruped generated at every detail");
        const size_t expected[]={11,21,22};Check(c.asset.bodies.size()==expected[detail],"quadruped detail includes declared optional neck/feet/tail/jaw");CheckRig(c,40);
        if(detail) {const auto &neck=c.asset.bodies[physicsasset::FindBody(c.asset,"neck")];Check(2*(neck.shape.radius+neck.shape.halfHeight)>4,"animal neck fits its actual child segment rather than human unit up vector");}
        Check(c.axisReviews==4&&c.chainReviews==(detail?1u:0u),"four flexion reviews and bounded tail review");
        Check(Generate(Preset("quadruped"),quad,detail,empty,ComposeMode::ReplaceRagdoll,repeat,&error)&&physicsasset::Serialize(c.asset)==physicsasset::Serialize(repeat.asset),"quadruped deterministic generation");
        Check(Generate(Preset("creature"),creature,detail,empty,ComposeMode::ReplaceRagdoll,c,&error),"creature branching rig generated");
        const size_t expectedCreature[]={3,13,24};Check(c.asset.bodies.size()==expectedCreature[detail],"creature detail bounds limbs/tail and branched wings");CheckRig(c,90);
        Check(c.chainReviews==(detail==0?0u:detail==1?3u:5u),"expanded chain mapping review is explicit");
    }
    auto humanBones=BarneySkeleton();std::vector<Mapping> map;Check(Resolve(Preset("human_standard"),humanBones,1,map,&error),"second naming convention mapping input");
    for(size_t i=0;i<map.size();++i)if(map[i].boneIndex>=0)humanBones[map[i].boneIndex].name=Preset("human_standard").roles[i].aliases[1];
    for(const char *id:{"human_heavy","human_loose"})
    {
        const auto &t=Preset(id);Check(Generate(t,humanBones,1,empty,ComposeMode::ReplaceRagdoll,c,&error),"human variants map ValveBiped convention");Check(c.asset.bodies.size()==15&&c.axisReviews==4,"human variants preserve anatomical graph and review");CheckRig(c,t.massBudget);
        Check(c.asset.simulation.angularResistance==t.simulation.angularResistance,"variant profile is concretely resolved");
    }
    Check(Preset("human_heavy").massBudget>Preset("human_standard").massBudget&&Preset("human_loose").simulation.angularResistance<Preset("human_standard").simulation.angularResistance,"variant tuning differs without unlocking translation");
    auto branching=quad;const int tail=int(std::find_if(branching.begin(),branching.end(),[](const Bone &b){return b.name=="tail";})-branching.begin());F5Bone(branching,"tail_spur",tail,{-2,2,0});
    const std::string before=physicsasset::Serialize(c.asset);
    Check(!Generate(Preset("quadruped"),branching,1,empty,ComposeMode::ReplaceRagdoll,c,&error)&&error.find("branches")!=std::string::npos&&physicsasset::Serialize(c.asset)==before,"ambiguous linear chain refuses without changing candidate");
    auto authored=Preset("quadruped");for(auto &r:authored.roles)if(r.id=="tail")r.chainBranches=true;
    Check(Generate(authored,branching,1,empty,ComposeMode::ReplaceRagdoll,c,&error)&&c.asset.bodies.size()==22,"explicit branching consent includes both reviewed descendants");
    auto reserved=Preset("quadruped");Role excluded;excluded.id="reserved_spur";excluded.parent="tail";excluded.aliases={"tail_spur"};excluded.required=false;excluded.minimumDetail=2;excluded.massRegion="tail";reserved.roles.push_back(excluded);
    Check(Generate(reserved,branching,1,empty,ComposeMode::ReplaceRagdoll,c,&error)&&c.asset.bodies.size()==21,"explicit detail exclusion reserves a bone against discovery");
    // J.10.6.10.8: an accessory with NO vertices must not acquire a
    // fallback capsule just because a chain reaches its bone.
    auto accessory=quad;
    const int tailEnd=int(std::find_if(accessory.begin(),accessory.end(),[](const Bone &b){return b.name=="tail_05";})-accessory.begin());
    const int ignored=F5Bone(accessory,"weapon_scope",tailEnd,{-2,0,0});
    accessory[ignored].vertices.clear();accessory[ignored].hasBounds=false;
    auto guarded=Preset("quadruped");
    for(auto &role:guarded.roles) if(role.id=="tail")role.chainDepth=5;
    Check(Generate(guarded,accessory,1,empty,ComposeMode::ReplaceRagdoll,c,&error)&&c.asset.bodies.size()==21,
          "unweighted attachment named weapon_scope is not converted into a physics body");
    Check(std::any_of(c.observations.begin(),c.observations.end(),[](const std::string &s){return s.find("CHAIN SKIP")!=std::string::npos;}),"skipped attachment is disclosed to the author");
    accessory[ignored].name="tail_decoration";
    guarded.chainExcludedBones={"tail_decoration"};
    Template parsed;
    Check(Parse(Serialize(guarded),parsed,&error)&&parsed.chainExcludedBones==guarded.chainExcludedBones,
          "custom chain exclusion saves and loads");
    Check(Generate(parsed,accessory,1,empty,ComposeMode::ReplaceRagdoll,c,&error)&&c.asset.bodies.size()==21,
          "exact authored chain exclusion skips a custom bone without geometry");
    guarded.chainExcludedBones.clear();
    Check(!Generate(guarded,accessory,1,empty,ComposeMode::ReplaceRagdoll,repeat,&error)||repeat.asset.bodies.size()==22,
          "custom bone is not silently skipped without an explicit exclusion");
    auto explicitAccessory=guarded;Role author;author.id="tail_accessory";author.parent="tail";author.boneOverride="tail_decoration";
    author.required=false;author.minimumDetail=2;author.massRegion="tail";explicitAccessory.roles.push_back(author);
    Check(Generate(explicitAccessory,accessory,1,empty,ComposeMode::ReplaceRagdoll,c,&error)&&c.asset.bodies.size()==21,
          "explicitly mapped accessory is reserved rather than duplicated by automatic expansion");
    auto badExclude=guarded;badExclude.chainExcludedBones={"tail_05","TAIL_05"};
    Check(!Validate(badExclude,&error),"duplicate custom chain exclusions are rejected");
    auto permuted=creature;std::reverse(permuted.begin(),permuted.end());for(auto &b:permuted)if(b.parent>=0)b.parent=int(creature.size())-1-b.parent;
    Check(Generate(Preset("creature"),creature,2,empty,ComposeMode::ReplaceRagdoll,c,&error)&&Generate(Preset("creature"),permuted,2,empty,ComposeMode::ReplaceRagdoll,repeat,&error)&&physicsasset::Serialize(c.asset)==physicsasset::Serialize(repeat.asset),"bone table reordering preserves deterministic branching asset");
    auto invalid=Preset("creature");invalid.roles.back().chainDepth=17;Check(!Validate(invalid,&error),"depth upper bound");invalid=Preset("creature");invalid.roles[0].chainDepth=1;Check(!Validate(invalid,&error),"root-wide discovery is forbidden");
    invalid=Preset("creature");invalid.roles[1].chainBranches=true;Check(!Validate(invalid,&error),"branch flag requires positive depth");
    auto malformed=creature;malformed[0].parent=2;Check(!Generate(Preset("creature"),malformed,2,empty,ComposeMode::ReplaceRagdoll,repeat,&error),"cyclic synthetic skeleton rejected before expansion");
    malformed=creature;malformed[0].name="unknown_root";Check(!Generate(Preset("creature"),malformed,2,empty,ComposeMode::ReplaceRagdoll,repeat,&error),"unknown required anatomy needs an explicit mapping");
    auto pairs=Preset("creature");pairs.pairRules.push_back({"left_wing","right_wing",false});Check(Generate(pairs,creature,2,empty,ComposeMode::ReplaceRagdoll,c,&error),"role pairs persist with expanded role graph");
    auto oversized=Preset("quadruped");while(oversized.roles.size()<128){Role r;r.id="unused_"+std::to_string(oversized.roles.size());r.parent="pelvis";r.aliases={r.id};r.required=false;r.massRegion="torso";oversized.roles.push_back(r);}
    Check(!Generate(oversized,quad,1,empty,ComposeMode::ReplaceRagdoll,repeat,&error)&&error.find("128")!=std::string::npos,"expanded roles cannot exceed Studio capacity");
    if(argc==2)
    {
        const std::filesystem::path folder=argv[1];std::filesystem::create_directories(folder/"test_fixtures");
        for(const auto &t:Builtins())Check(SaveFile((folder/(t.id+".ragtemplate")).string().c_str(),t,true,&error),"export practical presets");
        for(const auto &entry:std::vector<std::pair<std::string,std::vector<Bone>>>{{"quadruped",quad},{"creature",creature}})
        {
            Check(Generate(Preset(entry.first.c_str()),entry.second,2,empty,ComposeMode::ReplaceRagdoll,c,&error),"export synthetic fixture generation");
            Check(physicsasset::SaveFile((folder/"test_fixtures"/("synthetic_"+entry.first+".phys")).string().c_str(),c.asset,&error),"export labeled test-only v5 fixture");
            std::ofstream notes(folder/"test_fixtures"/(entry.first+"_review.txt"));notes<<"SYNTHETIC TEST FIXTURE; no matching MDL and no solver/game acceptance.\n";
            for(const auto &s:c.observations)notes<<s<<'\n';
        }
    }
    std::cout<<"F5 presets/quad/creature/branches: "<<checks<<" checks PASS; synthetic rigs and second naming convention, NO native model/solver/game\n";
}
