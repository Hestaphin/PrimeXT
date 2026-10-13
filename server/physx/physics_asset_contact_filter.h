#pragma once
#include "physics_asset_contact_policy.h"
#include <PxPhysicsAPI.h>
namespace physicsasset
{
inline physx::PxFilterData AssetBodyFilterData(physx::PxU32 owner,size_t index,const ContactGroup &group)
{return physx::PxFilterData(kAssetBodyFlag|(group.Active()?kAssetPolicyFlag:0u),owner,physx::PxU32(index+1),group.Active()?kAssetPolicyCookie:0u);}
inline physx::PxFilterFlags AssetPairFilter(physx::PxFilterData a,physx::PxFilterData b)
{
    switch(SameOwnerDecision(a.word0,a.word1,a.word2,a.word3,b.word0,b.word1,b.word2,b.word3))
    {case ContactDecision::Suppress:return physx::PxFilterFlag::eSUPPRESS;case ContactDecision::Callback:return physx::PxFilterFlag::eCALLBACK;case ContactDecision::Default:return physx::PxFilterFlag::eDEFAULT;}
    return physx::PxFilterFlag::eSUPPRESS;
}
class AssetContactFilter final:public physx::PxSimulationFilterCallback
{
public:
    physx::PxFilterFlags pairFound(physx::PxU32,physx::PxFilterObjectAttributes,physx::PxFilterData a,const physx::PxActor *,const physx::PxShape *sa,physx::PxFilterObjectAttributes,physx::PxFilterData b,const physx::PxActor *,const physx::PxShape *sb,physx::PxPairFlags &) override
    {
        if(!sa||!sb||a.word3!=kAssetPolicyCookie||b.word3!=kAssetPolicyCookie||!sa->userData||!sb->userData) return physx::PxFilterFlag::eSUPPRESS;
        const auto *ta=static_cast<const BodyContactToken *>(sa->userData),*tb=static_cast<const BodyContactToken *>(sb->userData);
        if(!ta->group||!tb->group||!ta->group->Owns(ta,a.word2)||!tb->group->Owns(tb,b.word2)) return physx::PxFilterFlag::eSUPPRESS;
        // Owner numbers may be reused; live immutable group identity wins.
        if(ta->group!=tb->group) return physx::PxFilterFlag::eDEFAULT;
        return ta->group->Allows(ta->index,tb->index)?physx::PxFilterFlag::eDEFAULT:physx::PxFilterFlag::eSUPPRESS;
    }
    void pairLost(physx::PxU32,physx::PxFilterObjectAttributes,physx::PxFilterData,physx::PxFilterObjectAttributes,physx::PxFilterData,bool) override {}
    bool statusChange(physx::PxU32 &,physx::PxPairFlags &,physx::PxFilterFlags &) override {return false;}
};
}
