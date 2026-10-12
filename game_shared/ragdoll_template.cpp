#include "ragdoll_template.h"
#include "ragdoll_template_f5_presets.h"
#include "physics_asset_settings.h"
#include "ragdoll_geometry.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace ragtemplate
{
namespace
{
std::string Lower(std::string s)
{
    for(char &c:s) if(c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');
    return s;
}
bool Fail(std::string *error,const std::string &message) { if(error) *error=message; return false; }
bool Text(const std::string &s,size_t maximum,bool empty=false)
{
    if((s.empty() && !empty) || s.size()>maximum) return false;
    return std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32 || c==127;});
}
bool Id(const std::string &s,bool empty=false)
{
    return Text(s,63,empty) && std::all_of(s.begin(),s.end(),[](unsigned char c){
        return (c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='_' || c=='-';});
}
bool Range(float f,float minimum,float maximum) { return std::isfinite(f) && f>=minimum && f<=maximum; }
bool Finite(const physicsasset::Vec3 &v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
std::string Quote(const std::string &s) { std::ostringstream o;o<<std::quoted(s);return o.str(); }
struct Token { std::string text; unsigned line=1; bool quoted=false; };
class Reader
{
public:
    Reader(std::string_view input,std::string *error):m_input(input),m_error(error) {}
    bool Run(Template &out)
    {
        if(m_input.size()>kMaxFileBytes) return Error("template exceeds 1 MiB");
        if(!Expect("ragdoll_template") || !Expect("{")) return false;
        Template value;std::unordered_set<std::string> keys;bool version=false;
        while(Next())
        {
            if(m_token.text=="}" && !m_token.quoted)
            {
                if(Next()) return Error("trailing data after template");
                if(m_bad) return false;
                if(!version) return Error("missing template_version");
                if(!Validate(value,m_error)) return false;
                out=std::move(value);return true;
            }
            const std::string key=m_token.text;
            if(key=="role")
            {
                Role role;
                if(value.roles.size()>=128) return Error("too many roles (maximum 128)");
                if(!String(role.id) || !Expect("{") || !ReadRole(role)) return false;
                value.roles.push_back(std::move(role));continue;
            }
            if(key=="pair_rule")
            {
                physicsasset::CollisionPair p;std::string decision;
                if(!String(p.first)||!String(p.second)||!String(decision)||(decision!="allow"&&decision!="deny")) return Error("pair_rule needs two roles and allow/deny");
                p.enabled=decision=="allow";value.pairRules.push_back(p);continue;
            }
            if(key=="mass_region")
            {
                MassRegion region;if(!String(region.id)||!Number(region.weight)) return false;
                value.massRegions.push_back(region);continue;
            }
            if(key=="exclude_section")
            {
                std::string section;if(!String(section)) return false;value.excludedSections.push_back(section);continue;
            }
            if(key=="chain_exclude_bone")
            {
                std::string bone;if(!String(bone)) return false;value.chainExcludedBones.push_back(bone);continue;
            }
            if(!keys.insert(key).second) return Error("duplicate key '"+key+"'");
            if(key=="template_version") { if(!Integer(value.version)) return false;version=true; }
            else if(key=="id") { if(!String(value.id)) return false; }
            else if(key=="name") { if(!String(value.name)) return false; }
            else if(key=="description") { if(!String(value.description)) return false; }
            else if(key=="anatomy") { if(!String(value.anatomy)) return false; }
            else if(key=="radius_ratio") { if(!Number(value.radiusRatio)) return false; }
            else if(key=="end_trim") { if(!Number(value.endTrim)) return false; }
            else if(key=="mass_ratio_warning") { if(!Number(value.massRatioWarning)) return false; }
            else if(key=="solver_position_iterations") {if(!Integer(value.simulation.positionIterations)) return false;}
            else if(key=="solver_velocity_iterations") {if(!Integer(value.simulation.velocityIterations)) return false;}
            else if(key=="max_linear_velocity") {if(!Number(value.simulation.maxLinearVelocity)) return false;}
            else if(key=="max_angular_velocity") {if(!Number(value.simulation.maxAngularVelocity)) return false;}
            else if(key=="max_depenetration_velocity") {if(!Number(value.simulation.maxDepenetrationVelocity)) return false;}
            else if(key=="sleep_threshold") {if(!Number(value.simulation.sleepThreshold)) return false;}
            else if(key=="contact_offset") {if(!Number(value.simulation.contactOffset)) return false;}
            else if(key=="rest_offset") {if(!Number(value.simulation.restOffset)) return false;}
            else if(key=="ccd") {int v;if(!Integer(v)||v<0||v>1) return Error("ccd must be 0/1");value.simulation.ccd=v!=0;}
            else if(key=="angular_resistance") {if(!Number(value.simulation.angularResistance)) return false;}
            else if(key=="drive_max_torque") {if(!Number(value.simulation.maxTorque)) return false;}
            else if(key=="projection") {std::string v;if(!String(v)||!physicsasset::ParseProjectionMode(v.c_str(),value.simulation.projection)) return Error("invalid projection");}
            else if(key=="projection_linear_tolerance") {if(!Number(value.simulation.projectionLinearTolerance)) return false;}
            else if(key=="projection_angular_tolerance") {if(!Number(value.simulation.projectionAngularTolerance)) return false;}
            else if(key=="self_collision") {std::string v;if(!String(v)||!physicsasset::ParseSelfCollisionMode(v.c_str(),value.selfCollision)) return Error("invalid self_collision");}
            else if(key=="collision_neighbor_depth") {if(!Integer(value.collisionNeighborDepth)) return false;}
            else if(key=="collision_overlap_margin") {if(!Number(value.collisionOverlapMargin)) return false;}
            else if(key=="mass_budget") { if(!Number(value.massBudget)) return false; }
            else if(key=="linear_damping") { if(!Number(value.simulation.linearDamping)) return false; }
            else if(key=="angular_damping") { if(!Number(value.simulation.angularDamping)) return false; }
            else return Error("unknown template key '"+key+"'");
        }
        return Error("missing template closing '}'");
    }
private:
    bool ReadRole(Role &role)
    {
        std::unordered_set<std::string> keys;
        while(Next())
        {
            if(m_token.text=="}"&&!m_token.quoted) return true;
            const std::string key=m_token.text;
            if(key!="alias" && !keys.insert(key).second) return Error("duplicate role key '"+key+"'");
            if(key=="parent") { if(!String(role.parent)) return false; }
            else if(key=="override") { if(!String(role.boneOverride)) return false; }
            else if(key=="alias") { std::string alias;if(!String(alias)) return false;role.aliases.push_back(std::move(alias)); }
            else if(key=="required") { int n;if(!Integer(n)) return false;if(n!=0&&n!=1) return Error("required must be 0 or 1");role.required=n!=0; }
            else if(key=="minimum_detail") { if(!Integer(role.minimumDetail)) return false; }
            else if(key=="chain_depth") {if(!Integer(role.chainDepth)) return false;}
            else if(key=="chain_branches") {int n;if(!Integer(n)) return false;if(n!=0&&n!=1) return Error("chain_branches must be 0 or 1");role.chainBranches=n!=0;}
            else if(key=="hitgroup") { if(!Integer(role.hitGroup)) return false; }
            else if(key=="shape") { std::string s;if(!String(s)) return false;if(!physicsasset::ParseShapeType(s.c_str(),role.shape)) return Error("unknown role shape"); }
            else if(key=="region") { if(!String(role.massRegion)) return false; }
            else if(key=="frame_mode")
            {
                std::string mode;if(!String(mode)) return false;
                if(mode=="bone") role.joint.frameMode=FrameMode::Bone;
                else if(mode=="segment") role.joint.frameMode=FrameMode::Segment;
                else if(mode=="flexion") role.joint.frameMode=FrameMode::Flexion;
                else if(mode=="explicit") role.joint.frameMode=FrameMode::Explicit;
                else return Error("frame_mode must be bone, segment, flexion or explicit");
            }
            else if(key=="frame_anchor") { if(!Number(role.joint.frameAnchor.x)||!Number(role.joint.frameAnchor.y)||!Number(role.joint.frameAnchor.z)) return false; }
            else if(key=="frame_angles") { if(!Number(role.joint.frameAngles.x)||!Number(role.joint.frameAngles.y)||!Number(role.joint.frameAngles.z)) return false; }
            else if(key=="flexion_sign") { if(!Integer(role.joint.flexionSign)) return false; }
            else if(key=="linear_damping") {if(!Number(role.linearDamping)) return false;}
            else if(key=="angular_damping") {if(!Number(role.angularDamping)) return false;}
            else if(key=="solver_position_iterations") {if(!Integer(role.bodyTuning.positionIterations)) return false;}
            else if(key=="solver_velocity_iterations") {if(!Integer(role.bodyTuning.velocityIterations)) return false;}
            else if(key=="max_linear_velocity") {if(!Number(role.bodyTuning.maxLinearVelocity)) return false;}
            else if(key=="max_angular_velocity") {if(!Number(role.bodyTuning.maxAngularVelocity)) return false;}
            else if(key=="max_depenetration_velocity") {if(!Number(role.bodyTuning.maxDepenetrationVelocity)) return false;}
            else if(key=="sleep_threshold") {if(!Number(role.bodyTuning.sleepThreshold)) return false;}
            else if(key=="contact_offset") {if(!Number(role.bodyTuning.contactOffset)) return false;}
            else if(key=="rest_offset") {if(!Number(role.bodyTuning.restOffset)) return false;}
            else if(key=="ccd") {if(!Integer(role.bodyTuning.ccd)) return false;}
            else if(key=="angular_resistance") {if(!Number(role.joint.tuning.angularResistance)) return false;}
            else if(key=="drive_max_torque") {if(!Number(role.joint.tuning.maxTorque)) return false;}
            else if(key=="projection") {std::string v;if(!String(v)||!physicsasset::ParseProjectionMode(v.c_str(),role.joint.tuning.projection)) return Error("invalid projection");}
            else if(key=="projection_linear_tolerance") {if(!Number(role.joint.tuning.projectionLinearTolerance)) return false;}
            else if(key=="projection_angular_tolerance") {if(!Number(role.joint.tuning.projectionAngularTolerance)) return false;}
            else if(key=="mass_weight") { if(!Number(role.massWeight)) return false; }
            else if(key=="swing1") { if(!Number(role.joint.swing1)) return false; }
            else if(key=="swing2") { if(!Number(role.joint.swing2)) return false; }
            else if(key=="twist_low") { if(!Number(role.joint.twistLow)) return false; }
            else if(key=="twist_high") { if(!Number(role.joint.twistHigh)) return false; }
            else if(key=="twist_motion"||key=="swing1_motion"||key=="swing2_motion")
            {
                std::string s;if(!String(s)) return false;
                auto &mode=key=="twist_motion"?role.joint.twistMotion:key=="swing1_motion"?role.joint.swing1Motion:role.joint.swing2Motion;
                if(!physicsasset::ParseAngularMotion(s.c_str(),mode)) return Error("angular motion must be locked, limited or free");
            }
            else return Error("unknown role key '"+key+"'");
        }
        return Error("missing role closing '}'");
    }
    bool Error(const std::string &s) { m_bad=true;return Fail(m_error,"line "+std::to_string(m_token.line)+": "+s); }
    bool Expect(const char *s) { return Next() && !m_token.quoted && m_token.text==s ? true : Error(std::string("expected '")+s+"'"); }
    bool String(std::string &s) { if(!Next()) return Error("missing value");if(!m_token.quoted&&(m_token.text=="{"||m_token.text=="}")) return Error("expected a value");s=m_token.text;return true; }
    bool Number(float &v)
    {
        std::string s;if(!String(s)) return false;char *end=nullptr;errno=0;
        const float n=std::strtof(s.c_str(),&end);
        if(s.empty()||end!=s.c_str()+s.size()||errno==ERANGE||!std::isfinite(n)) return Error("expected finite number");
        v=n;return true;
    }
    bool Integer(int &v)
    {
        std::string s;if(!String(s)) return false;char *end=nullptr;errno=0;const long n=std::strtol(s.c_str(),&end,10);
        if(s.empty()||end!=s.c_str()+s.size()||errno==ERANGE||n<std::numeric_limits<int>::min()||n>std::numeric_limits<int>::max()) return Error("expected integer");
        v=static_cast<int>(n);return true;
    }
    bool Next()
    {
        if(m_bad) return false;
        for(;;)
        {
            while(m_pos<m_input.size() && (m_input[m_pos]==' '||m_input[m_pos]=='\t'||m_input[m_pos]=='\r'||m_input[m_pos]=='\n'))
            { if(m_input[m_pos++]=='\n') ++m_line; }
            if(m_pos<m_input.size() && (m_input[m_pos]=='#'||(m_input[m_pos]=='/'&&m_pos+1<m_input.size()&&m_input[m_pos+1]=='/')))
            { while(m_pos<m_input.size()&&m_input[m_pos]!='\n') ++m_pos;continue; }
            break;
        }
        if(m_pos==m_input.size()) return false;
        m_token={"",m_line,false};char c=m_input[m_pos++];
        if(c=='{'||c=='}') { m_token.text=c;return true; }
        if(c=='"')
        {
            m_token.quoted=true;
            while(m_pos<m_input.size())
            {
                c=m_input[m_pos++];if(c=='"') return true;
                if(c=='\\')
                {
                    if(m_pos==m_input.size()) return Error("unfinished string escape");
                    c=m_input[m_pos++];if(c!='\\'&&c!='"') return Error("only quote/backslash escapes are supported");
                }
                if(static_cast<unsigned char>(c)<32) return Error("control character in string");
                m_token.text+=c;
            }
            return Error("unterminated quoted string");
        }
        m_token.text=c;
        while(m_pos<m_input.size())
        {
            c=m_input[m_pos];if(c==' '||c=='\t'||c=='\r'||c=='\n'||c=='{'||c=='}'||c=='#') break;
            if(c=='/'&&m_pos+1<m_input.size()&&m_input[m_pos+1]=='/') break;
            if(static_cast<unsigned char>(c)<32) return Error("control character in token");
            m_token.text+=c;++m_pos;
        }
        return true;
    }
    std::string_view m_input;std::string *m_error;size_t m_pos=0;unsigned m_line=1;Token m_token;bool m_bad=false;
};
}

bool Validate(const Template &t,std::string *error)
{
    if(t.version!=kVersion) return Fail(error,"unsupported template_version; expected 1");
    if(!Id(t.id)||!Text(t.name,100)||!Text(t.description,200,true)||!Id(t.anatomy)) return Fail(error,"invalid template identity/name/description/anatomy");
    if(!Range(t.massBudget,.01f,100000)||!physicsasset::ValidSimulationSettings(t.simulation)) return Fail(error,"invalid mass budget or simulation settings");
    if(!physicsasset::SelfCollisionModeName(t.selfCollision)||t.collisionNeighborDepth<0||t.collisionNeighborDepth>16||!Range(t.collisionOverlapMargin,0,1000)) return Fail(error,"invalid collision generation policy");
    std::unordered_set<std::string> pairKeys;
    for(const auto &p:t.pairRules)
    {
        const auto a=Lower(p.first),b=Lower(p.second);
        auto role=[&](const std::string &id){return std::any_of(t.roles.begin(),t.roles.end(),[&](const auto &r){return Lower(r.id)==id;});};
        if(a==b||!role(a)||!role(b)||!pairKeys.insert(std::min(a,b)+"\n"+std::max(a,b)).second) return Fail(error,"unknown, self or duplicate role pair");
    }
    if(!Range(t.radiusRatio,0.01f,0.45f)||!Range(t.endTrim,0,0.3f)||!Range(t.massRatioWarning,1,1000)) return Fail(error,"invalid radius_ratio, end_trim or mass_ratio_warning");
    std::unordered_set<std::string> regions,sections;
    if(t.massRegions.size()>128||t.excludedSections.size()>32||t.chainExcludedBones.size()>64) return Fail(error,"too many mass regions or exclusions");
    for(const auto &region:t.massRegions)
        if(!Id(region.id)||!Range(region.weight,0.001f,10000)||!regions.insert(Lower(region.id)).second) return Fail(error,"invalid/duplicate mass region");
    for(const auto &section:t.excludedSections)
        if(!Text(section,63)||!std::all_of(section.begin(),section.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z');})||!sections.insert(Lower(section)).second) return Fail(error,"invalid/duplicate accessory section");
    std::unordered_set<std::string> chainExclusions;
    for(const auto &name:t.chainExcludedBones)
        if(!Text(name,63)||!chainExclusions.insert(Lower(name)).second) return Fail(error,"invalid/duplicate chain bone exclusion");
    if(t.roles.empty()||t.roles.size()>128) return Fail(error,"template requires 1..128 roles");
    std::unordered_map<std::string,int> index;int roots=0;
    for(size_t i=0;i<t.roles.size();++i)
    {
        const Role &r=t.roles[i];
        if(!Id(r.id)||!Id(r.parent,true)||!Text(r.boneOverride,63,true)||r.minimumDetail<0||r.minimumDetail>2||r.hitGroup<0||r.hitGroup>255||!Range(r.massWeight,0.001f,10000)) return Fail(error,"invalid role '"+r.id+"'");
        if(r.chainDepth<0||r.chainDepth>16||(r.chainBranches&&r.chainDepth==0)||(!r.parent.size()&&r.chainDepth)) return Fail(error,"role '"+r.id+"': chain_depth must be 0..16 on a non-root role; branches require a positive depth");
        if(!index.emplace(Lower(r.id),static_cast<int>(i)).second) return Fail(error,"duplicate role '"+r.id+"'");
        if(r.parent.empty()) { ++roots;if(!r.required||r.minimumDetail!=0) return Fail(error,"root role must be required at every detail level"); }
        std::unordered_set<std::string> aliases;size_t aliasBytes=0;
        for(const auto &alias:r.aliases)
        {
            aliasBytes+=alias.size()+1;
            if(!Text(alias,63)||alias.find(';')!=std::string::npos||!aliases.insert(Lower(alias)).second) return Fail(error,"invalid/duplicate alias for role '"+r.id+"'");
        }
        if(aliasBytes>200) return Fail(error,"role aliases exceed the 200-byte editable limit");
        if(r.aliases.empty()&&r.boneOverride.empty()) return Fail(error,"role '"+r.id+"' requires aliases or an explicit override");
        if(r.shape!=physicsasset::ShapeType::Box&&r.shape!=physicsasset::ShapeType::Sphere&&r.shape!=physicsasset::ShapeType::Capsule) return Fail(error,"invalid role shape");
        if(!Id(r.massRegion,true)||(!regions.empty()&&!regions.count(Lower(r.massRegion)))|| (regions.empty()&&!r.massRegion.empty())) return Fail(error,"role '"+r.id+"' references an unknown mass region");
        physicsasset::AssetDef scope;scope.simulation=t.simulation;physicsasset::BodyDef body;body.linearDamping=r.linearDamping;body.angularDamping=r.angularDamping;body.tuning=r.bodyTuning;
        physicsasset::JointDef joint;joint.tuning=r.joint.tuning;
        if((r.linearDamping<0&&r.linearDamping!=-1)||(r.angularDamping<0&&r.angularDamping!=-1)||!std::isfinite(r.linearDamping)||!std::isfinite(r.angularDamping)||
            !physicsasset::ValidBodyTuning(body.tuning)||!physicsasset::ValidSimulationSettings(physicsasset::ResolveBodySettings(scope,body))||!physicsasset::ValidResolvedJointSettings(scope,joint)) return Fail(error,"invalid role stability override");
        const auto &j=r.joint;
        if(j.frameMode<FrameMode::Bone||j.frameMode>FrameMode::Explicit||!Finite(j.frameAnchor)||!Finite(j.frameAngles)||(j.flexionSign!=1&&j.flexionSign!=-1)) return Fail(error,"invalid frame or flexion_sign for role '"+r.id+"'");
        if(!Range(j.swing1,0.001f,179.999f)||!Range(j.swing2,0.001f,179.999f)||!Range(j.twistLow,-180,180)||!Range(j.twistHigh,-180,180)||j.twistLow>=j.twistHigh) return Fail(error,"invalid D6 limits for role '"+r.id+"'");
        for(auto motion:{j.twistMotion,j.swing1Motion,j.swing2Motion}) if(!physicsasset::AngularMotionName(motion)) return Fail(error,"invalid angular motion");
    }
    if(roots!=1) return Fail(error,"template requires exactly one root role");
    for(size_t i=0;i<t.roles.size();++i)
    {
        std::unordered_set<int> seen;int role=static_cast<int>(i);
        while(!t.roles[role].parent.empty())
        {
            if(!seen.insert(role).second) return Fail(error,"cycle in role hierarchy at '"+t.roles[i].id+"'");
            const auto p=index.find(Lower(t.roles[role].parent));
            if(p==index.end()) return Fail(error,"unknown parent role '"+t.roles[role].parent+"'");
            role=p->second;
        }
    }
    return true;
}
bool Parse(std::string_view text,Template &out,std::string *error) { return Reader(text,error).Run(out); }
std::string Serialize(const Template &t)
{
    std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(std::numeric_limits<float>::max_digits10);
    s<<"ragdoll_template\n{\n    template_version "<<t.version<<"\n    id "<<Quote(t.id)<<"\n    name "<<Quote(t.name)<<"\n    description "<<Quote(t.description)<<"\n    anatomy "<<Quote(t.anatomy)<<"\n    mass_budget "<<t.massBudget<<"\n    linear_damping "<<t.simulation.linearDamping<<"\n    angular_damping "<<t.simulation.angularDamping<<"\n";
    s<<"    solver_position_iterations "<<t.simulation.positionIterations<<"\n";
    s<<"    solver_velocity_iterations "<<t.simulation.velocityIterations<<"\n";
    s<<"    max_linear_velocity "<<t.simulation.maxLinearVelocity<<"\n";
    s<<"    max_angular_velocity "<<t.simulation.maxAngularVelocity<<"\n";
    s<<"    max_depenetration_velocity "<<t.simulation.maxDepenetrationVelocity<<"\n";
    s<<"    sleep_threshold "<<t.simulation.sleepThreshold<<"\n";
    s<<"    contact_offset "<<t.simulation.contactOffset<<"\n";
    s<<"    rest_offset "<<t.simulation.restOffset<<"\n";
    s<<"    ccd "<<(t.simulation.ccd?1:0)<<"\n";
    s<<"    angular_resistance "<<t.simulation.angularResistance<<"\n";
    s<<"    drive_max_torque "<<t.simulation.maxTorque<<"\n";
    s<<"    projection "<<physicsasset::ProjectionModeName(t.simulation.projection)<<"\n";
    s<<"    projection_linear_tolerance "<<t.simulation.projectionLinearTolerance<<"\n";
    s<<"    projection_angular_tolerance "<<t.simulation.projectionAngularTolerance<<"\n";
    s<<"    self_collision "<<physicsasset::SelfCollisionModeName(t.selfCollision)<<"\n    collision_neighbor_depth "<<t.collisionNeighborDepth<<"\n    collision_overlap_margin "<<t.collisionOverlapMargin<<"\n";
    for(const auto &p:t.pairRules) s<<"    pair_rule "<<Quote(p.first)<<" "<<Quote(p.second)<<" "<<(p.enabled?"allow":"deny")<<"\n";
    s<<"    radius_ratio "<<t.radiusRatio<<"\n    end_trim "<<t.endTrim<<"\n    mass_ratio_warning "<<t.massRatioWarning<<"\n";
    for(const auto &region:t.massRegions) s<<"    mass_region "<<Quote(region.id)<<" "<<region.weight<<"\n";
    for(const auto &section:t.excludedSections) s<<"    exclude_section "<<Quote(section)<<"\n";
    for(const auto &bone:t.chainExcludedBones) s<<"    chain_exclude_bone "<<Quote(bone)<<"\n";
    for(const auto &r:t.roles)
    {
        s<<"\n    role "<<Quote(r.id)<<"\n    {\n        parent "<<Quote(r.parent)<<"\n";
        for(const auto &a:r.aliases) s<<"        alias "<<Quote(a)<<"\n";
        s<<"        override "<<Quote(r.boneOverride)<<"\n        required "<<(r.required?1:0)<<"\n        minimum_detail "<<r.minimumDetail<<"\n        shape "<<physicsasset::ShapeTypeName(r.shape)<<"\n        hitgroup "<<r.hitGroup<<"\n        mass_weight "<<r.massWeight<<"\n";
    if(r.chainDepth) s<<"        chain_depth "<<r.chainDepth<<"\n        chain_branches "<<(r.chainBranches?1:0)<<"\n";
    if(r.linearDamping>=0) s<<"        linear_damping "<<r.linearDamping<<"\n";
    if(r.angularDamping>=0) s<<"        angular_damping "<<r.angularDamping<<"\n";
    if(r.bodyTuning.positionIterations>=0) s<<"        solver_position_iterations "<<r.bodyTuning.positionIterations<<"\n";
    if(r.bodyTuning.velocityIterations>=0) s<<"        solver_velocity_iterations "<<r.bodyTuning.velocityIterations<<"\n";
    if(r.bodyTuning.maxLinearVelocity>=0) s<<"        max_linear_velocity "<<r.bodyTuning.maxLinearVelocity<<"\n";
    if(r.bodyTuning.maxAngularVelocity>=0) s<<"        max_angular_velocity "<<r.bodyTuning.maxAngularVelocity<<"\n";
    if(r.bodyTuning.maxDepenetrationVelocity>=0) s<<"        max_depenetration_velocity "<<r.bodyTuning.maxDepenetrationVelocity<<"\n";
    if(r.bodyTuning.sleepThreshold>=0) s<<"        sleep_threshold "<<r.bodyTuning.sleepThreshold<<"\n";
    if(r.bodyTuning.contactOffset>=0) s<<"        contact_offset "<<r.bodyTuning.contactOffset<<"\n";
    if(r.bodyTuning.restOffset>=0) s<<"        rest_offset "<<r.bodyTuning.restOffset<<"\n";
    if(r.bodyTuning.ccd>=0) s<<"        ccd "<<r.bodyTuning.ccd<<"\n";
    if(r.joint.tuning.angularResistance>=0) s<<"        angular_resistance "<<r.joint.tuning.angularResistance<<"\n";
    if(r.joint.tuning.maxTorque>=0) s<<"        drive_max_torque "<<r.joint.tuning.maxTorque<<"\n";
    if(r.joint.tuning.projection!=physicsasset::ProjectionMode::Inherit) s<<"        projection "<<physicsasset::ProjectionModeName(r.joint.tuning.projection)<<"\n";
    if(r.joint.tuning.projectionLinearTolerance>=0) s<<"        projection_linear_tolerance "<<r.joint.tuning.projectionLinearTolerance<<"\n";
    if(r.joint.tuning.projectionAngularTolerance>=0) s<<"        projection_angular_tolerance "<<r.joint.tuning.projectionAngularTolerance<<"\n";
        const auto &j=r.joint;
        const char *mode=j.frameMode==FrameMode::Bone?"bone":j.frameMode==FrameMode::Segment?"segment":j.frameMode==FrameMode::Flexion?"flexion":"explicit";
        s<<"        region "<<Quote(r.massRegion)<<"\n        frame_mode "<<mode<<"\n        frame_anchor "<<j.frameAnchor.x<<" "<<j.frameAnchor.y<<" "<<j.frameAnchor.z<<"\n        frame_angles "<<j.frameAngles.x<<" "<<j.frameAngles.y<<" "<<j.frameAngles.z<<"\n        flexion_sign "<<j.flexionSign<<"\n";
        s<<"        twist_motion "<<physicsasset::AngularMotionName(j.twistMotion)<<"\n        swing1_motion "<<physicsasset::AngularMotionName(j.swing1Motion)<<"\n        swing2_motion "<<physicsasset::AngularMotionName(j.swing2Motion)<<"\n        swing1 "<<j.swing1<<"\n        swing2 "<<j.swing2<<"\n        twist_low "<<j.twistLow<<"\n        twist_high "<<j.twistHigh<<"\n    }\n";
    }
    s<<"}\n";return s.str();
}
bool LoadFile(const char *path,Template &out,std::string *error)
{
    if(!path||!*path) return Fail(error,"empty template path");
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file) return Fail(error,"cannot open template");
    const auto size=file.tellg();if(size<0||size>static_cast<std::streamoff>(kMaxFileBytes)) return Fail(error,"invalid/oversized template file");
    std::string text(static_cast<size_t>(size),'\0');file.seekg(0);
    if(!file.read(text.data(),size)) return Fail(error,"cannot read complete template");
    return Parse(text,out,error);
}
bool SaveFile(const char *path,const Template &t,bool overwrite,std::string *error)
{
    if(!path||!*path) return Fail(error,"empty template path");
    if(!Validate(t,error)) return false;
    const std::string text=Serialize(t);if(text.size()>kMaxFileBytes) return Fail(error,"template exceeds file limit");
    static std::atomic<unsigned> counter{0};FILE *file=nullptr;std::string temporary;
    for(unsigned i=0;i<100;++i)
    {
        temporary=std::string(path)+".tmp."+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"."+std::to_string(counter++);
        file=std::fopen(temporary.c_str(),"wbx");if(file) break;if(errno!=EEXIST) return Fail(error,"cannot create temporary template file");
    }
    if(!file) return Fail(error,"no temporary template filename available");
    bool ok=std::fwrite(text.data(),1,text.size(),file)==text.size();
    if(std::fflush(file)!=0) ok=false;
    if(std::fclose(file)!=0) ok=false;
    if(!ok) { std::remove(temporary.c_str());return Fail(error,"template write failed; original file retained"); }
#ifdef _WIN32
    ok=MoveFileExA(temporary.c_str(),path,overwrite?MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH:MOVEFILE_WRITE_THROUGH)!=0;
#else
    ok=overwrite?std::rename(temporary.c_str(),path)==0:link(temporary.c_str(),path)==0;
#endif
    std::remove(temporary.c_str());
    return ok?true:Fail(error,"cannot commit template; original file retained (confirm overwrite or choose another path)");
}

const std::vector<Template> &Builtins()
{
    static const std::vector<Template> presets=[]
    {
        Template human;human.id="human_standard";human.name="Human Standard";human.anatomy="human";
        human.description="Anatomical F2 rig; review inferred flexion signs, overlaps and live drops.";
        human.excludedSections={"weapon","gun","accessory"};
        human.massRegions={{"torso",0.44f},{"head",0.08f},{"left_arm",0.08f},{"right_arm",0.08f},{"left_leg",0.16f},{"right_leg",0.16f}};
        auto add=[&](const char *id,const char *parent,const char *hl,const char *valve,physicsasset::ShapeType shape,int hit,float weight,int detail=0,bool required=true)
        {
            Role r;r.id=id;r.parent=parent;r.aliases={hl,valve};r.shape=shape;r.hitGroup=hit;r.massWeight=weight;r.minimumDetail=detail;r.required=required;human.roles.push_back(r);
        };
        using S=physicsasset::ShapeType;
        add("pelvis","","Bip01 Pelvis","ValveBiped.Bip01_Pelvis",S::Box,3,16);
        add("spine","pelvis","Bip01 Spine","ValveBiped.Bip01_Spine",S::Box,2,4,2,false);
        add("spine_mid","spine","Bip01 Spine1","ValveBiped.Bip01_Spine1",S::Box,2,4,2,false);
        add("chest","spine_mid","Bip01 Spine3","ValveBiped.Bip01_Spine4",S::Box,2,20);
        add("head","chest","Bip01 Head","ValveBiped.Bip01_Head1",S::Sphere,1,5);
        add("left_upper_arm","chest","Bip01 L Arm1","ValveBiped.Bip01_L_UpperArm",S::Capsule,4,2);
        add("left_forearm","left_upper_arm","Bip01 L Arm2","ValveBiped.Bip01_L_Forearm",S::Capsule,4,1.6f);
        add("left_hand","left_forearm","Bip01 L Hand","ValveBiped.Bip01_L_Hand",S::Box,4,0.6f,2,false);
        add("right_upper_arm","chest","Bip01 R Arm1","ValveBiped.Bip01_R_UpperArm",S::Capsule,5,2);
        add("right_forearm","right_upper_arm","Bip01 R Arm2","ValveBiped.Bip01_R_Forearm",S::Capsule,5,1.6f);
        add("right_hand","right_forearm","Bip01 R Hand","ValveBiped.Bip01_R_Hand",S::Box,5,0.6f,2,false);
        add("left_thigh","pelvis","Bip01 L Leg","ValveBiped.Bip01_L_Thigh",S::Capsule,6,7);
        add("left_calf","left_thigh","Bip01 L Leg1","ValveBiped.Bip01_L_Calf",S::Capsule,6,4);
        add("left_foot","left_calf","Bip01 L Foot","ValveBiped.Bip01_L_Foot",S::Box,6,1,1,false);
        add("right_thigh","pelvis","Bip01 R Leg","ValveBiped.Bip01_R_Thigh",S::Capsule,7,7);
        add("right_calf","right_thigh","Bip01 R Leg1","ValveBiped.Bip01_R_Calf",S::Capsule,7,4);
        add("right_foot","right_calf","Bip01 R Foot","ValveBiped.Bip01_R_Foot",S::Box,7,1,1,false);
        // Optional shoulder bodies remain supported by chest even if the Studio
        // hierarchy routes their bones through a neck.
        add("left_shoulder","chest","Bip01 L Arm","ValveBiped.Bip01_L_Clavicle",S::Capsule,4,3,1,false);
        add("right_shoulder","chest","Bip01 R Arm","ValveBiped.Bip01_R_Clavicle",S::Capsule,5,3,1,false);
        add("neck","chest","Bip01 Neck","ValveBiped.Bip01_Neck1",S::Capsule,1,2,2,false);
        for(auto &role:human.roles)
        {
            const auto &id=role.id;
            role.massRegion=id.find("left_")==0?(id.find("thigh")!=std::string::npos||id.find("calf")!=std::string::npos||id.find("foot")!=std::string::npos?"left_leg":"left_arm"):
                id.find("right_")==0?(id.find("thigh")!=std::string::npos||id.find("calf")!=std::string::npos||id.find("foot")!=std::string::npos?"right_leg":"right_arm"):
                (id=="head"||id=="neck"?"head":"torso");
            auto &j=role.joint;j.frameMode=FrameMode::Segment;
            if(id=="left_upper_arm") role.parent="left_shoulder";
            if(id=="right_upper_arm") role.parent="right_shoulder";
            if(id=="head") { role.parent="neck";j.twistLow=-35;j.twistHigh=35;j.swing1=25;j.swing2=25; }
            else if(id=="neck") {j.twistLow=-20;j.twistHigh=20;j.swing1=20;j.swing2=20;}
            else if(id=="chest"||id=="spine"||id=="spine_mid") {j.twistLow=-15;j.twistHigh=15;j.swing1=20;j.swing2=20;}
            else if(id.find("forearm")!=std::string::npos||id.find("calf")!=std::string::npos)
            {
                j.frameMode=FrameMode::Flexion;j.twistLow=0;j.twistHigh=id.find("calf")!=std::string::npos?130:140;
                j.swing1Motion=j.swing2Motion=physicsasset::AngularMotion::Locked;
            }
            else if(id.find("upper_arm")!=std::string::npos) {j.twistLow=-65;j.twistHigh=65;j.swing1=85;j.swing2=75;}
            else if(id.find("thigh")!=std::string::npos) {j.twistLow=-35;j.twistHigh=35;j.swing1=65;j.swing2=45;}
            else {j.twistLow=-20;j.twistHigh=20;j.swing1=25;j.swing2=25;}
        }
        human.simulation.linearDamping=.1f;human.simulation.angularDamping=.5f;human.simulation.angularResistance=1.5f;human.selfCollision=physicsasset::SelfCollisionMode::Selective;
        Template free=human;free.simulation.angularResistance=0;free.simulation.angularDamping=.05f;free.id="free_rotation";free.name="Free Rotation";
        free.description="All D6 angles free; all joint translations remain locked. Review fitting and mappings.";
        for(auto &r:free.roles) r.joint.twistMotion=r.joint.swing1Motion=r.joint.swing2Motion=physicsasset::AngularMotion::Free;
        Template custom;custom.id="custom";custom.name="Custom";custom.description="Duplicate and edit roles/aliases to define a custom skeleton.";
        Role root;root.id="root";root.aliases={"root","Bip01 Pelvis"};root.shape=S::Box;custom.roles.push_back(root);
        return std::vector<Template>{human,free,custom,f5::HumanVariant(human,true),f5::HumanVariant(human,false),f5::Quadruped(),f5::Creature()};
    }();
    return presets;
}

bool Resolve(const Template &t,const std::vector<Bone> &bones,int detail,std::vector<Mapping> &out,std::string *error)
{
    if(!Validate(t,error)) return false;
    if(detail<0||detail>2||bones.empty()||bones.size()>256) return Fail(error,"invalid detail/skeleton size");
    std::unordered_map<std::string,int> names,roles;
    for(size_t i=0;i<bones.size();++i)
    {
        if(!Text(bones[i].name,63)||!names.emplace(Lower(bones[i].name),static_cast<int>(i)).second) return Fail(error,"invalid/duplicate skeleton bone name");
        if(bones[i].parent < -1 || bones[i].parent>=static_cast<int>(bones.size())) return Fail(error,"invalid skeleton parent index");
        int p=static_cast<int>(i);std::unordered_set<int> seen;
        while(p>=0)
        {
            if(p>=static_cast<int>(bones.size())||!seen.insert(p).second) return Fail(error,"cycle/invalid skeleton parent");
            p=bones[p].parent;
        }
    }
    for(size_t i=0;i<t.roles.size();++i) roles[Lower(t.roles[i].id)]=static_cast<int>(i);
    std::vector<Mapping> result;std::unordered_set<int> used;
    for(const auto &r:t.roles)
    {
        Mapping map;map.role=r.id;
        if(detail<r.minimumDetail) { map.source="excluded by detail";result.push_back(map);continue; }
        std::unordered_set<int> matches;
        if(!r.boneOverride.empty())
        {
            auto it=names.find(Lower(r.boneOverride));if(it==names.end()) return Fail(error,"role '"+r.id+"': explicit bone '"+r.boneOverride+"' not found");
            matches.insert(it->second);map.source="explicit override";map.confidence=1;
        }
        else
        {
            for(const auto &a:r.aliases) { auto it=names.find(Lower(a));if(it!=names.end()) matches.insert(it->second); }
            map.source="exact alias; topology checked";map.confidence=0.8f;
        }
        if(matches.size()>1) return Fail(error,"role '"+r.id+"' is ambiguous; select an explicit bone override");
        if(matches.empty())
        {
            if(r.required) return Fail(error,"required role '"+r.id+"' has no matching bone; edit aliases or override");
            map.source="optional role missing";map.confidence=0;result.push_back(map);continue;
        }
        map.boneIndex=*matches.begin();map.bone=bones[map.boneIndex].name;
        if(!used.insert(map.boneIndex).second) return Fail(error,"multiple roles map to bone '"+map.bone+"'");
        result.push_back(map);
    }
    for(size_t i=0;i<t.roles.size();++i)
    {
        if(result[i].boneIndex<0) continue;
        std::string parent=t.roles[i].parent;
        while(!parent.empty())
        {
            const int p=roles.at(Lower(parent));
            if(result[p].boneIndex>=0)
            {
                int bone=bones[result[i].boneIndex].parent;
                while(bone>=0&&bone!=result[p].boneIndex) bone=bones[bone].parent;
                if(bone<0) return Fail(error,"role '"+t.roles[i].id+"' is not below mapped parent '"+parent+"'; review the skeleton mapping");
                break;
            }
            parent=t.roles[p].parent;
        }
    }
    out=std::move(result);return true;
}

bool Compose(const physicsasset::AssetDef &current,const physicsasset::AssetDef &generated,ComposeMode mode,Candidate &out,std::string *error)
{
    if(mode!=ComposeMode::ReplaceRagdoll&&mode!=ComposeMode::Merge) return Fail(error,"invalid composition mode");
    if(!physicsasset::Validate(generated,error)) return false;
    if((!current.bodies.empty()||!current.collisions.empty())&&!physicsasset::Validate(current,error)) return false;
    physicsasset::AssetDef result=current;result.version=physicsasset::kCurrentVersion;result.collisionFingerprint=0;
    const bool newProfile=mode==ComposeMode::ReplaceRagdoll||current.bodies.empty();
    if(newProfile) {result.simulation=generated.simulation;result.selfCollision=generated.selfCollision;}
    std::unordered_set<std::string> protect;
    for(const auto &b:current.bodies) if(b.propDrive!=physicsasset::PropDriveMode::None||!b.part.empty()) protect.insert(Lower(b.name));
    for(const auto &j:current.joints) if(j.kind==physicsasset::JointKind::Hinge) { protect.insert(Lower(j.parent));protect.insert(Lower(j.child)); }
    bool changed=true;
    while(changed)
    {
        changed=false;
        for(const auto &j:current.joints) if(protect.count(Lower(j.child))) changed=protect.insert(Lower(j.parent)).second||changed;
    }
    if(mode==ComposeMode::ReplaceRagdoll)
    {
        result.bodies.clear();result.joints.clear();result.collisionPairs.clear();
        for(const auto &b:current.bodies) if(protect.count(Lower(b.name))) {auto value=b;physicsasset::MaterializeBodySettings(current,value);result.bodies.push_back(value);}
        for(const auto &j:current.joints) if(protect.count(Lower(j.parent))&&protect.count(Lower(j.child)))
        {auto value=j;value.tuning=physicsasset::ResolveJointSettings(current,j);if(j.kind==physicsasset::JointKind::Hinge) value.tuning.angularResistance=0;result.joints.push_back(value);}
        for(const auto &p:current.collisionPairs) if(protect.count(Lower(p.first))&&protect.count(Lower(p.second))) result.collisionPairs.push_back({p.first,p.second,current.selfCollision!=physicsasset::SelfCollisionMode::Off&&p.enabled});
    }
    std::unordered_map<std::string,std::string> resolved;
    unsigned added=0,reused=0,addedJoints=0;
    for(const auto &b:generated.bodies)
    {
        int match=-1;
        for(size_t i=0;i<result.bodies.size();++i)
            if(Lower(result.bodies[i].name)==Lower(b.name)||Lower(result.bodies[i].bone)==Lower(b.bone))
            {
                if(match>=0&&match!=static_cast<int>(i)) return Fail(error,"ambiguous authored name/bone match for '"+b.name+"'");
                match=static_cast<int>(i);
            }
        if(match>=0) { resolved[Lower(b.name)]=result.bodies[match].name;++reused; }
        else { resolved[Lower(b.name)]=b.name;auto value=b;if(!newProfile) physicsasset::MaterializeBodySettings(generated,value);result.bodies.push_back(value);++added; }
    }
    for(const auto &j:generated.joints)
    {
        physicsasset::JointDef next=j;next.parent=resolved.at(Lower(j.parent));next.child=resolved.at(Lower(j.child));
        if(Lower(next.parent)==Lower(next.child)) return Fail(error,"generation collapses a joint onto one authored body");
        bool childOwned=false;
        for(const auto &old:result.joints) if(Lower(old.child)==Lower(next.child)) { childOwned=true;break; }
        if(childOwned) continue;
        for(const auto &old:result.joints) if(Lower(old.name)==Lower(next.name)) return Fail(error,"generated joint name conflicts with authored joint '"+next.name+"'");
        if(!newProfile) next.tuning=physicsasset::ResolveJointSettings(generated,j);
        result.joints.push_back(next);++addedJoints;
    }
    if(!physicsasset::Validate(result,error)) return false;
    out.asset=std::move(result);out.addedBodies=added;out.reusedBodies=reused;
    out.removedBodies=static_cast<unsigned>(current.bodies.size()+added-out.asset.bodies.size());
    out.addedJoints=addedJoints;out.removedJoints=static_cast<unsigned>(current.joints.size()+addedJoints-out.asset.joints.size());
    out.protectedBodies=static_cast<unsigned>(protect.size());out.preservedCollisions=static_cast<unsigned>(current.collisions.size());
    return true;
}

namespace
{
using V=physicsasset::Vec3;
using raggeometry::Pose;
float Quantile(std::vector<float> v,double q)
{
    if(v.empty()) return 0;
    std::sort(v.begin(),v.end());return v[static_cast<size_t>(q*(v.size()-1))];
}
std::vector<V> Samples(const Bone &bone)
{
    if(!bone.vertices.empty()) return bone.vertices;
    std::vector<V> points;
    if(bone.hasBounds) for(int i=0;i<8;++i) points.push_back({i&1?bone.boundsMax.x:bone.boundsMin.x,i&2?bone.boundsMax.y:bone.boundsMin.y,i&4?bone.boundsMax.z:bone.boundsMin.z});
    return points;
}
void Fit(const Template &t,const Role &role,const Bone &bone,V segment,std::vector<V> points,physicsasset::BodyDef &body,Candidate &result)
{
    using namespace raggeometry;
    if(points.empty()) result.observations.push_back("No vertices for "+bone.name+"; inspect fallback dimensions.");
    if(role.shape==physicsasset::ShapeType::Capsule && Len(segment)>0.1)
    {
        const double length=Len(segment);const Pose axes=Axis(segment);
        std::vector<float> y,z,radial;
        for(V p:points) {const V q=axes.InverseRotate(p);y.push_back(q.y);z.push_back(q.z);}
        // Robust transverse center; constrain offsets to avoid weapon/outlier leverage.
        const double cy=std::clamp(double((Quantile(y,0.25)+Quantile(y,0.75))*0.5f),-length*0.15,length*0.15),cz=std::clamp(double((Quantile(z,0.25)+Quantile(z,0.75))*0.5f),-length*0.15,length*0.15);
        for(V p:points) {const V q=axes.InverseRotate(p);radial.push_back(float(std::hypot(q.y-cy,q.z-cz)));}
        const double minimum=std::min(0.25,length*0.05);
        body.shape.radius=float(std::clamp(points.empty()?length*0.16:double(Quantile(radial,0.85)),minimum,length*t.radiusRatio));
        const double extent=length*(0.5-t.endTrim);
        body.shape.halfHeight=float(std::max(extent-body.shape.radius,minimum*0.1));
        body.offset=axes.Rotate({float(length*0.5),float(cy),float(cz)});body.angles=Angles(axes);
        body.shape.halfExtents={float(extent),body.shape.radius,body.shape.radius};
    }
    else if(!points.empty())
    {
        std::vector<float> x,y,z;for(V p:points) {x.push_back(p.x);y.push_back(p.y);z.push_back(p.z);}
        const bool robust=!bone.vertices.empty()&&points.size()>=20;
        const V lo={Quantile(x,robust?0.05:0),Quantile(y,robust?0.05:0),Quantile(z,robust?0.05:0)},hi={Quantile(x,robust?0.95:1),Quantile(y,robust?0.95:1),Quantile(z,robust?0.95:1)};
        body.offset=Mul(Add(lo,hi),0.5);V half=Mul(Sub(hi,lo),0.5);
        body.shape.halfExtents={std::max(half.x*0.9f,0.1f),std::max(half.y*0.9f,0.1f),std::max(half.z*0.9f,0.1f)};
        body.shape.radius=std::max({half.x,half.y,half.z,0.1f})*0.9f;body.shape.halfHeight=0.1f;
        if(role.shape==physicsasset::ShapeType::Capsule)
        {
            int axis=half.y>half.x?1:0;if(half.z>(axis?half.y:half.x)) axis=2;
            const double extent=axis==0?half.x:axis==1?half.y:half.z;
            body.shape.radius=std::max(0.05f,float(extent*t.radiusRatio));body.shape.halfHeight=std::max(float(extent)-body.shape.radius,0.01f);
            body.angles=axis==0?V{}:axis==1?V{0,90,0}:V{-90,0,0};
            result.observations.push_back("No anatomical segment for "+bone.name+"; capsule used bounds axis, review it.");
        }
    }
    else
    {
        const float size=float(std::clamp(Len(bone.localOrigin)*0.15,0.25,4.0));
        body.shape.radius=size;body.shape.halfHeight=size;body.shape.halfExtents={size,size,size};
    }
}
}

bool Generate(const Template &t,const std::vector<Bone> &bones,int detail,const physicsasset::AssetDef &current,ComposeMode mode,Candidate &out,std::string *error)
{
    // Chain expansion is authoring only. Recursion evaluates the resolved roles
    // once; no template or discovery dependency enters the runtime asset.
    if(std::any_of(t.roles.begin(),t.roles.end(),[](const Role &r){return r.chainDepth>0;}))
    {
        std::vector<Mapping> mapped;if(!Resolve(t,bones,detail,mapped,error)) return false;
        Template expanded=t;for(auto &r:expanded.roles) {r.chainDepth=0;r.chainBranches=false;}
        std::unordered_set<int> reserved;std::unordered_set<std::string> ids;
        for(const auto &r:t.roles)
        {
            ids.insert(Lower(r.id));
            for(size_t b=0;b<bones.size();++b)
                if((!r.boneOverride.empty()&&Lower(r.boneOverride)==Lower(bones[b].name))||
                   (r.boneOverride.empty()&&std::any_of(r.aliases.begin(),r.aliases.end(),[&](const std::string &a){return Lower(a)==Lower(bones[b].name);}))) reserved.insert(int(b));
        }
        std::vector<std::vector<int>> children(bones.size());
        for(size_t b=0;b<bones.size();++b) if(bones[b].parent>=0) children[bones[b].parent].push_back(int(b));
        for(auto &list:children) std::sort(list.begin(),list.end(),[&](int a,int b){return Lower(bones[a].name)<Lower(bones[b].name);});
        // Never mistake accessory/helper bones for physical limb descendants.
        // Reject named attachment tokens even when the bone has no vertices: a
        // fallback shape would otherwise manufacture unwanted collision mass.
        // An explicitly authored role maps the bone normally (reserved above).
        const std::unordered_set<std::string> accessoryTokens={
            "weapon","gun","scope","attachment","attach","muzzle","accessory",
            "holster","socket","effect","fx","prop","gear"};
        auto excludedBone=[&](const std::string &name)->bool
        {
            const auto key=Lower(name);
            if(std::any_of(t.chainExcludedBones.begin(),t.chainExcludedBones.end(),
                [&](const std::string &excluded){return Lower(excluded)==key;})) return true;
            // Match complete underscore/punctuation-separated tokens, not
            // substrings: e.g. 'gear' excludes 'tail_gear' but not 'gearbox'.
            std::string token;
            for(size_t i=0;i<=key.size();++i)
            {
                const char ch=i<key.size()?key[i]:'_';
                if((ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')) token+=ch;
                else {if(accessoryTokens.count(token)) return true;token.clear();}
            }
            return false;
        };
        std::vector<std::string> notes;unsigned reviews=0;
        for(size_t seed=0;seed<t.roles.size();++seed)
        {
            const auto &profile=t.roles[seed];if(!profile.chainDepth||mapped[seed].boneIndex<0) continue;
            struct Node {int bone,depth;std::string role;};std::vector<Node> queue{{mapped[seed].boneIndex,0,profile.id}};
            unsigned ordinal=0;
            for(size_t q=0;q<queue.size();++q)
            {
                const Node node=queue[q];if(node.depth>=profile.chainDepth) {if(!children[node.bone].empty()) notes.push_back("CHAIN DEPTH REVIEW "+profile.id+" stops at "+bones[node.bone].name+"; deeper bones remain passive.");continue;}
                std::vector<int> available;
                for(int child:children[node.bone])
                {
                    if(reserved.count(child)) continue;
                    if(excludedBone(bones[child].name))
                    {
                        notes.push_back("CHAIN SKIP accessory/excluded bone '"+bones[child].name+"' (explicit role mapping can include it).");
                        continue;
                    }
                    available.push_back(child);
                }
                if(available.size()>1&&!profile.chainBranches) return Fail(error,"chain '"+profile.id+"' branches at '"+bones[node.bone].name+"'; add explicit roles or enable chain_branches after review");
                for(int child:available)
                {
                    if(expanded.roles.size()>=128) return Fail(error,"expanded chains exceed the 128-role Studio budget; reduce chain depth/detail");
                    Role role=profile;role.chainDepth=0;role.chainBranches=false;role.required=false;role.boneOverride=bones[child].name;role.aliases.clear();role.parent=node.role;
                    do {role.id=profile.id.substr(0,40)+"_chain_"+std::to_string(++ordinal);} while(ids.count(Lower(role.id)));
                    ids.insert(Lower(role.id));role.joint.frameMode=FrameMode::Segment;role.joint.frameAnchor={};role.joint.frameAngles={};
                    expanded.roles.push_back(role);reserved.insert(child);queue.push_back({child,node.depth+1,role.id});
                    notes.push_back("CHAIN REVIEW "+role.id+" -> "+role.boneOverride+"; inherited angular/body profile, inferred segment frame.");
                }
            }
            if(ordinal) ++reviews;
        }
        Candidate resolved;if(!Generate(expanded,bones,detail,current,mode,resolved,error)) return false;
        resolved.chainReviews+=reviews;resolved.observations.insert(resolved.observations.begin(),notes.begin(),notes.end());
        for(auto &map:resolved.mapping) if(std::any_of(expanded.roles.begin()+t.roles.size(),expanded.roles.end(),[&](const Role &r){return r.id==map.role;})) {map.source="bounded chain expansion; review mapping";map.confidence=.6f;}
        out=std::move(resolved);return true;
    }
    using namespace raggeometry;
    Candidate result;if(!Resolve(t,bones,detail,result.mapping,error)) return false;
    std::unordered_map<std::string,int> roleIndex,boneIndex;
    for(size_t i=0;i<t.roles.size();++i) roleIndex[Lower(t.roles[i].id)]=int(i);
    for(size_t i=0;i<bones.size();++i) boneIndex[Lower(bones[i].name)]=int(i);
    std::vector<Pose> bind(bones.size());std::vector<bool> ready(bones.size(),false);
    for(size_t i=0;i<bones.size();++i)
    {
        const auto &b=bones[i];
        if(!Finite(b.localOrigin)||!Finite(b.localAngles)||!Finite(b.childOffset)||!Finite(b.boundsMin)||!Finite(b.boundsMax)) return Fail(error,"non-finite geometry for '"+b.name+"'");
        if(b.hasBounds&&(b.boundsMin.x>b.boundsMax.x||b.boundsMin.y>b.boundsMax.y||b.boundsMin.z>b.boundsMax.z)) return Fail(error,"inverted vertex bounds for '"+b.name+"'");
        for(V vertex:b.vertices) if(!Finite(vertex)) return Fail(error,"non-finite vertex for '"+b.name+"'");
        std::vector<int> chain;int p=int(i);while(p>=0&&!ready[p]) {chain.push_back(p);p=bones[p].parent;}
        for(auto it=chain.rbegin();it!=chain.rend();++it)
        {
            const auto &b2=bones[*it];const Pose local=Studio(b2.localAngles,b2.localOrigin);
            bind[*it]=b2.parent<0?local:Concat(bind[b2.parent],local);ready[*it]=true;
        }
    }
    // Empty intermediate neck/spine links should not become tiny mass carriers.
    if(Lower(t.anatomy)=="human") for(size_t i=0;i<t.roles.size();++i)
    {
        auto &map=result.mapping[i];const auto id=Lower(t.roles[i].id);
        if(map.boneIndex>=0&&!t.roles[i].required&&(id=="neck"||id=="spine_mid") &&
            !bones[map.boneIndex].hasBounds&&bones[map.boneIndex].vertices.empty())
        {
            result.observations.push_back("Merged empty optional "+id+"; its Studio bone remains in the model.");
            map.boneIndex=-1;map.bone.clear();map.source="merged empty intermediate";map.confidence=0;
        }
    }
    auto roleBone=[&](const std::string &id)->int {auto it=roleIndex.find(id);return it==roleIndex.end()?-1:result.mapping[it->second].boneIndex;};
    auto rolePoint=[&](const std::string &id,V fallback)->V {int b=roleBone(id);return b>=0?bind[b].p:fallback;};
    const V pelvis=rolePoint("pelvis",{}),head=rolePoint("head",Add(pelvis,{0,0,1}));
    V up=Unit(Sub(head,pelvis),{0,0,1});
    V right=Unit(Sub(rolePoint("right_thigh",{1,0,0}),rolePoint("left_thigh",{-1,0,0})),{1,0,0});
    V forward=Unit(Cross(right,up),{0,1,0});
    if(Lower(t.anatomy)=="quadruped")
    {
        forward=Unit(Sub(rolePoint("chest",head),pelvis),{1,0,0});
        right=Unit(Sub(rolePoint("right_front_upper",{0,-1,0}),rolePoint("left_front_upper",{0,1,0})),{0,-1,0});
        up=Unit(Cross(forward,right),{0,0,1});
    }
    std::unordered_map<std::string,double> weights;
    for(size_t i=0;i<t.roles.size();++i) if(result.mapping[i].boneIndex>=0) weights[Lower(t.roles[i].massRegion)]+=t.roles[i].massWeight;
    double activeBudget=0;
    if(t.massRegions.empty()) activeBudget=1;
    else for(const auto &region:t.massRegions) if(weights[Lower(region.id)]>0) activeBudget+=region.weight;
    result.generated.sourcePath=current.sourcePath;result.generated.simulation=t.simulation;result.generated.selfCollision=t.selfCollision;
    std::vector<V> segment(t.roles.size());
    for(size_t i=0;i<t.roles.size();++i)
    {
        const int b=result.mapping[i].boneIndex;if(b<0) continue;
        segment[i]=bones[b].childOffset;
        for(size_t j=0;j<t.roles.size();++j) if(Lower(t.roles[j].parent)==Lower(t.roles[i].id)&&result.mapping[j].boneIndex>=0)
        {segment[i]=bind[b].InversePoint(bind[result.mapping[j].boneIndex].p);break;}
        if(Lower(t.anatomy)=="human"&&(Lower(t.roles[i].id)=="head"||Lower(t.roles[i].id)=="neck")) segment[i]=bind[b].InverseRotate(up);
    }
    for(size_t i=0;i<t.roles.size();++i)
    {
        const int b=result.mapping[i].boneIndex;if(b<0) continue;
        const Role &r=t.roles[i];const Bone &bone=bones[b];const auto region=Lower(r.massRegion);
        double share=1;
        if(!t.massRegions.empty()) for(const auto &mr:t.massRegions) if(Lower(mr.id)==region) share=mr.weight/activeBudget;
        physicsasset::BodyDef body;body.name=body.bone=bone.name;body.hitGroup=r.hitGroup;body.shape.type=r.shape;
        body.mass=float(t.massBudget*share*r.massWeight/weights.at(region));body.linearDamping=r.linearDamping;body.angularDamping=r.angularDamping;body.tuning=r.bodyTuning;
        auto points=Samples(bone);
        // Include every omitted intermediate on a selected torso chain,
        // including bones without a named template role. Sibling attachments
        // are not on these paths and are never pooled into the torso.
        if(region=="torso")
        {
            std::unordered_set<int> selected,pooled;
            for(const auto &map:result.mapping) if(map.boneIndex>=0) selected.insert(map.boneIndex);
            for(size_t j=0;j<t.roles.size();++j)
            {
                const int child=result.mapping[j].boneIndex;
                if(child<0||child==b||Lower(t.roles[j].massRegion)!=region) continue;
                std::vector<int> intermediates;int ancestor=bones[child].parent;
                while(ancestor>=0&&!selected.count(ancestor)) {intermediates.push_back(ancestor);ancestor=bones[ancestor].parent;}
                if(ancestor!=b) continue;
                for(int source:intermediates) if(pooled.insert(source).second)
                    for(V point:Samples(bones[source])) points.push_back(bind[b].InversePoint(bind[source].Point(point)));
            }
        }
        Fit(t,r,bone,segment[i],std::move(points),body,result);result.generated.bodies.push_back(body);
        std::string parent=r.parent;
        while(!parent.empty())
        {
            const int p=roleIndex.at(Lower(parent));const int pb=result.mapping[p].boneIndex;
            if(pb>=0)
            {
                physicsasset::JointDef j;j.parent=result.mapping[p].bone;j.child=bone.name;j.name=j.parent+"_to_"+j.child;
                j.twistMotion=r.joint.twistMotion;j.swing1Motion=r.joint.swing1Motion;j.swing2Motion=r.joint.swing2Motion;
                j.swing1=r.joint.swing1;j.swing2=r.joint.swing2;j.twistLow=r.joint.twistLow;j.twistHigh=r.joint.twistHigh;
                j.frameAnchor=r.joint.frameAnchor;j.tuning=r.joint.tuning;
                if(r.joint.frameMode==FrameMode::Explicit) j.frameAngles=r.joint.frameAngles;
                else if(r.joint.frameMode==FrameMode::Segment) j.frameAngles=Angles(Axis(segment[i]));
                else if(r.joint.frameMode==FrameMode::Flexion)
                {
                    const V incoming=Unit(Sub(bind[b].p,bind[pb].p));const V outgoing=Unit(bind[b].Rotate(segment[i]));
                    V axis=Cross(incoming,outgoing);
                    const bool straight=Len(axis)<0.03;
                    if(straight)
                    {
                        const bool knee=Lower(r.id).find("calf")!=std::string::npos||Lower(r.id).find("hind_lower")!=std::string::npos;
                        V target=knee?Mul(forward,-1):forward;axis=Cross(incoming,target);
                        if(Len(axis)<0.03) axis=Cross(incoming,right);
                    }
                    axis=Mul(Unit(axis),r.joint.flexionSign);
                    j.frameAngles=Angles(Axis(bind[b].InverseRotate(axis),bind[b].InverseRotate(outgoing)));
                    if(r.joint.twistMotion!=physicsasset::AngularMotion::Free||r.joint.swing1Motion!=physicsasset::AngularMotion::Free||r.joint.swing2Motion!=physicsasset::AngularMotion::Free)
                    {
                        ++result.axisReviews;
                        result.observations.push_back("AXIS REVIEW "+r.id+": "+(straight?"straight-chain anatomical fallback":"bind bend plane")+"; confirm positive flexion, or use explicit frame.");
                    }
                }
                result.generated.joints.push_back(j);break;
            }
            parent=t.roles[p].parent;
        }
    }
    if(!Compose(current,result.generated,mode,result,error)) return false;
    for(const auto &body:result.asset.bodies)
        if(!boneIndex.count(Lower(body.bone))) return Fail(error,"preserved body '"+body.name+"' references a missing model bone; review the authored asset");
    for(const auto &collision:result.asset.collisions)
        if(!collision.bone.empty()&&!boneIndex.count(Lower(collision.bone))) return Fail(error,"preserved collision '"+collision.name+"' references a missing model bone; review the authored asset");
    std::unordered_set<int> generatedIndices;
    for(const auto &body:result.generated.bodies) for(size_t i=0;i<result.asset.bodies.size();++i) if(Lower(result.asset.bodies[i].bone)==Lower(body.bone)) generatedIndices.insert(int(i));
    const size_t count=result.asset.bodies.size();std::vector<std::vector<int>> adjacency(count),distances(count,std::vector<int>(count,-1));
    for(const auto &j:result.asset.joints) {int a=physicsasset::FindBody(result.asset,j.parent.c_str()),b=physicsasset::FindBody(result.asset,j.child.c_str());adjacency[a].push_back(b);adjacency[b].push_back(a);}
    for(size_t first=0;first<count;++first)
    {
        std::vector<int> queue{int(first)};distances[first][first]=0;
        for(size_t k=0;k<queue.size();++k) for(int next:adjacency[queue[k]]) if(distances[first][next]<0) {distances[first][next]=distances[first][queue[k]]+1;queue.push_back(next);}
    }
    auto padded=[&](const physicsasset::BodyDef &b) {auto v=b;const float padding=physicsasset::ResolveBodySettings(result.asset,b).contactOffset+t.collisionOverlapMargin*.5f;v.shape.radius+=padding;v.shape.halfExtents=Add(v.shape.halfExtents,{padding,padding,padding});return v;};
    auto key=[](const std::string &a,const std::string &b){return std::min(Lower(a),Lower(b))+"\n"+std::max(Lower(a),Lower(b));};
    std::unordered_set<std::string> existingPairs;for(const auto &p:result.asset.collisionPairs) existingPairs.insert(key(p.first,p.second));
    for(size_t i=0;i<count;++i) for(size_t j=i+1;j<count;++j)
    {
        const auto &a=result.asset.bodies[i],&b=result.asset.bodies[j];
        if(!generatedIndices.count(int(i))||!generatedIndices.count(int(j))||existingPairs.count(key(a.name,b.name))) continue;
        if(mode==ComposeMode::Merge&&physicsasset::FindBody(current,a.name.c_str())>=0&&physicsasset::FindBody(current,b.name.c_str())>=0) continue;
        const bool enabled=t.selfCollision==physicsasset::SelfCollisionMode::Selective&&distances[i][j]>t.collisionNeighborDepth&&Overlap(padded(a),bind[boneIndex.at(Lower(a.bone))],padded(b),bind[boneIndex.at(Lower(b.bone))])==0;
        result.asset.collisionPairs.push_back({a.name,b.name,enabled});
    }
    for(const auto &rule:t.pairRules)
    {
        const int a=roleBone(Lower(rule.first)),b=roleBone(Lower(rule.second));
        if(a<0||b<0) {result.observations.push_back("Skipped pair override with omitted optional role: "+rule.first+" / "+rule.second);continue;}
        std::string first,second;for(const auto &body:result.asset.bodies) {if(Lower(body.bone)==Lower(bones[a].name)) first=body.name;if(Lower(body.bone)==Lower(bones[b].name)) second=body.name;}
        if(mode==ComposeMode::Merge&&physicsasset::FindBody(current,first.c_str())>=0&&physicsasset::FindBody(current,second.c_str())>=0) continue;
        bool found=false;for(auto &pair:result.asset.collisionPairs) if(key(pair.first,pair.second)==key(first,second)) {pair.enabled=rule.enabled;found=true;break;}
        if(!found) result.asset.collisionPairs.push_back({first,second,rule.enabled});
    }
    if(!physicsasset::Validate(result.asset,error)) return false;
    unsigned contacts=0;for(const auto &pair:result.asset.collisionPairs) if(pair.enabled)
    {
        ++contacts;const auto &a=result.asset.bodies[physicsasset::FindBody(result.asset,pair.first.c_str())],&b=result.asset.bodies[physicsasset::FindBody(result.asset,pair.second.c_str())];
        if(Overlap(padded(a),bind[boneIndex.at(Lower(a.bone))],padded(b),bind[boneIndex.at(Lower(b.bone))])>0) result.observations.push_back("CONTACT CLEARANCE REVIEW "+a.name+" / "+b.name+": authored allow overlaps contact margins; inspect before simulation.");
    }
    result.observations.push_back("Resolved self collision="+std::string(physicsasset::SelfCollisionModeName(result.asset.selfCollision))+"; configured allow pairs="+std::to_string(contacts)+". Off suppresses the matrix.");
    for(const auto &joint:result.asset.joints)
    {
        const auto &a=result.asset.bodies[physicsasset::FindBody(result.asset,joint.parent.c_str())],&b2=result.asset.bodies[physicsasset::FindBody(result.asset,joint.child.c_str())];
        const double ratio=std::max(a.mass,b2.mass)/std::min(a.mass,b2.mass);
        if(ratio>t.massRatioWarning) {++result.massRatioWarnings;result.observations.push_back("MASS RATIO "+joint.name+" = "+std::to_string(ratio)+"; rebalance neighbors or merge the small link.");}
    }
    for(size_t i=0;i<result.asset.bodies.size();++i) for(size_t j=i+1;j<result.asset.bodies.size();++j)
    {
        const auto &a=result.asset.bodies[i],&b2=result.asset.bodies[j];const double overlap=Overlap(a,bind[boneIndex.at(Lower(a.bone))],b2,bind[boneIndex.at(Lower(b2.bone))]);
        if(overlap>0.1) {++result.overlapWarnings;result.observations.push_back("OVERLAP "+a.name+" / "+b2.name+" depth estimate="+std::to_string(overlap)+"; trim/offset colliders before enabling contact.");}
    }
    result.observations.push_back("F2 candidate: inspect axes, overlaps and live drops. No solver or visual stability certification.");
    if(result.reusedBodies) result.observations.push_back("Authored/protected bodies and child-owned joints reused unchanged; mass budget applies to generated bodies before reuse.");
    out=std::move(result);return true;
}

void Transaction::Begin(const physicsasset::AssetDef &asset) { m_before=asset;m_active=true;m_ready=false; }
void Transaction::Cancel() { m_active=m_ready=false;m_candidate=Candidate{}; }
bool Transaction::SetCandidate(const Candidate &candidate,std::string *error)
{
    if(!m_active) return Fail(error,"generation session is closed");
    if(!physicsasset::Validate(candidate.asset,error)) return false;
    m_candidate=candidate;m_ready=true;return true;
}
bool Transaction::Apply(physicsasset::AssetDef &asset,std::string *error)
{
    if(!HasCandidate()) return Fail(error,"generate and review a preview first");
    if(physicsasset::Serialize(asset)!=physicsasset::Serialize(m_before)) return Fail(error,"asset changed since preview opened; cancel and reopen generation");
    m_undoBefore=m_before;asset=m_candidate.asset;m_applied=physicsasset::Serialize(asset);m_undo=true;m_active=m_ready=false;return true;
}
bool Transaction::Undo(physicsasset::AssetDef &asset,std::string *error)
{
    if(!m_undo) return Fail(error,"no applied generation to undo");
    if(physicsasset::Serialize(asset)!=m_applied) return Fail(error,"asset edited after generation; undo would discard those edits");
    asset=m_undoBefore;m_undo=false;Cancel();return true;
}
}
