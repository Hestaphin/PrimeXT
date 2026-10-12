#pragma once
#include "physics_asset_format.h"
#include <cstdint>
#include <memory>
#include <unordered_map>
namespace physicsasset
{
constexpr std::uint32_t kConveyorFlag=1u,kAssetBodyFlag=2u,kAssetPolicyFlag=4u,kAssetPolicyCookie=0x50355841u;
enum class ContactDecision { Default, Suppress, Callback };
inline std::string ContactName(std::string s) {for(char &c:s) if(c>='A'&&c<='Z') c=char(c-'A'+'a');return s;}
inline ContactDecision SameOwnerDecision(std::uint32_t flags0,std::uint32_t owner0,std::uint32_t body0,std::uint32_t cookie0,std::uint32_t flags1,std::uint32_t owner1,std::uint32_t body1,std::uint32_t cookie1)
{
    if(!(flags0&kAssetBodyFlag)||!(flags1&kAssetBodyFlag)||owner0==0||owner0!=owner1) return ContactDecision::Default;
    if((flags0&kAssetPolicyFlag)&&(flags1&kAssetPolicyFlag)&&body0&&body1&&cookie0==kAssetPolicyCookie&&cookie1==kAssetPolicyCookie) return ContactDecision::Callback;
    return ContactDecision::Suppress; // proxy/interactive same-owner exclusion
}
class ContactGroup;
struct BodyContactToken {const ContactGroup *group=nullptr;std::uint32_t index=0;};
// Construct only after Validate. Immutable until all actors/shapes have been
// released outside simulate/fetchResults. There is no global entity registry.
class ContactGroup
{
public:
    explicit ContactGroup(const AssetDef &asset):m_tokens(asset.bodies.size()),m_allow(asset.bodies.size()*asset.bodies.size(),false),m_active(asset.selfCollision!=SelfCollisionMode::Off)
    {
        std::unordered_map<std::string,size_t> names;
        for(size_t i=0;i<m_tokens.size();++i) {m_tokens[i]={this,std::uint32_t(i)};names.emplace(ContactName(asset.bodies[i].name),i);}
        if(m_active) for(const auto &pair:asset.collisionPairs)
        {
            const auto a=names.find(ContactName(pair.first)),b=names.find(ContactName(pair.second));
            if(a!=names.end()&&b!=names.end()&&a->second!=b->second) m_allow[a->second*m_tokens.size()+b->second]=m_allow[b->second*m_tokens.size()+a->second]=pair.enabled;
        }
    }
    ContactGroup(const ContactGroup &)=delete;ContactGroup &operator=(const ContactGroup &)=delete;
    ContactGroup(ContactGroup &&)=delete;ContactGroup &operator=(ContactGroup &&)=delete;
    const BodyContactToken *Token(size_t i) const {return i<m_tokens.size()?&m_tokens[i]:nullptr;}
    bool Owns(const BodyContactToken *token,std::uint32_t encoded) const {return token&&encoded&&token->group==this&&token->index<m_tokens.size()&&token==&m_tokens[token->index]&&encoded==token->index+1;}
    bool Allows(size_t a,size_t b) const {return a<m_tokens.size()&&b<m_tokens.size()&&a!=b&&m_allow[a*m_tokens.size()+b];}
    bool Active() const {return m_active;}
private:
    std::vector<BodyContactToken> m_tokens;
    std::vector<bool> m_allow;
    bool m_active;
};
inline bool CollisionPairEnabled(const AssetDef &asset,int a,int b)
{
    if(a<0||b<0||a>=int(asset.bodies.size())||b>=int(asset.bodies.size())||a==b||asset.selfCollision==SelfCollisionMode::Off) return false;
    const auto first=ContactName(asset.bodies[size_t(a)].name),second=ContactName(asset.bodies[size_t(b)].name);
    for(const auto &pair:asset.collisionPairs) {const auto x=ContactName(pair.first),y=ContactName(pair.second);if((first==x&&second==y)||(first==y&&second==x)) return pair.enabled;}
    return false;
}
}
