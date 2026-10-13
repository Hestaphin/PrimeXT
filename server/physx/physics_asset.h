#pragma once

#include <memory>
#include <vector>

#include <PxPhysicsAPI.h>
#include "physics_asset_format.h"
#include "physics_asset_contact_policy.h"
#include "ragdoll_diagnostics.h"
#include "animation_pose_stamp.h"

class CBaseEntity;
struct PhysicsRagdollBodySaveRecord;

using PhysicsAssetDef = physicsasset::AssetDef;
using PhysicsAssetBodyDef = physicsasset::BodyDef;
using PhysicsAssetJointDef = physicsasset::JointDef;

struct PhysicsAssetRayHit
{
    float distance = 0.0f;
    physx::PxVec3 position{ 0.0f };
    physx::PxVec3 normal{ 0.0f, 0.0f, 1.0f };
    int bodyIndex = -1;
    int boneIndex = -1;
    int hitGroup = 0;
};

struct PhysicsAssetBodyRuntime
{
    int boneIndex = -1;
    physx::PxRigidDynamic *actor = nullptr;
    physx::PxShape *shape = nullptr;

    // Animation-space history is intentionally kept even when no PhysX actor
    // exists. This lets lazily-created ragdolls inherit limb motion at the
    // animation -> simulation handoff without keeping hundreds of dormant
    // rigid actors alive for every NPC.
    // Reference/bind transforms are model-space and never change. Joint local
    // frames are derived from these, so D6 limits are anatomical and do not
    // depend on whatever animation happened to be playing when ragdoll began.
    physx::PxTransform referenceBonePose{ physx::PxIdentity };
    physx::PxTransform referenceBodyPose{ physx::PxIdentity };
    physx::PxTransform bodyLocalPose{ physx::PxIdentity };
    physx::PxTransform bodyToBonePose{ physx::PxIdentity };
    physx::PxTransform previousAnimationPose{ physx::PxIdentity };
    physx::PxTransform currentAnimationPose{ physx::PxIdentity };
    physx::PxVec3 animationLinearVelocity{ 0.0f };
    physx::PxVec3 animationAngularVelocity{ 0.0f };
    bool hasPreviousAnimationPose = false;
    bool hasCurrentAnimationPose = false;
    bool hasAnimationVelocity = false;

    // World-space bone pose reconstructed from the simulated rigid body.
    // The actor itself is offset from the bone by the authored body local
    // transform, so renderer/network code must consume this bone pose rather
    // than actor->getGlobalPose() directly.
    physx::PxTransform simulatedBonePose{ physx::PxIdentity };
    bool hasSimulationPose = false;

    // 0 = animation/kinematic, 1 = unconstrained ragdoll physics. Intermediate
    // values keep the body dynamic but drive it toward animation (physical
    // animation). This one scalar also makes partial ragdoll authoring/runtime
    // control deterministic without creating a second body representation.
    float physicsWeight = 1.0f;
};

class PhysicsAssetInstance
{
public:
    PhysicsAssetInstance(CBaseEntity *owner, std::shared_ptr<PhysicsAssetDef> def);
    ~PhysicsAssetInstance();

    // Lightweight setup: resolves body -> studio-bone mappings and builds the
    // reference skeleton, but creates no PhysX actors or joints. Animation
    // samples are recorded separately when Studio animation actually advances.
    bool InitializePoseTracking();
    void UpdateAnimationPose(float sampleTime);

    // Expensive/lazy step. May be called much later than pose initialization.
    bool CreateActors(physx::PxPhysics *physics, physx::PxScene *scene, physx::PxMaterial *material);
    bool HasActors() const { return m_actorsCreated; }
    bool MatchesOwnerModel() const;

    void Destroy();
    void FollowAnimation();
    void ApplyAnimationControl(float dt, float strength, float damping, float maxForce);
    void CaptureSimulationPose();
    // F0 measurement only, after fetchResults/CaptureSimulationPose. Mode 0
    // performs no actor/joint reads; 1 reports window peaks once per second.
    void RecordDiagnostics(float step, const physx::PxSimulationStatistics *sceneStats, float mode,
        float inputFrameDelta = -1, float accumulator = -1);
    bool HasSimulationPose() const { return m_hasSimulationPose; }
    int RootBodyIndex() const { return m_rootBodyIndex; }
    bool GetRootBonePose(physx::PxTransform &out) const;
    bool GetSimulationBounds(physx::PxBounds3 &out) const;
    const std::vector<PhysicsAssetBodyRuntime> &Bodies() const { return m_bodies; }
    bool SetRagdoll(bool enabled);
    bool IsRagdoll() const { return m_ragdoll; }
    bool SaveArticulatedBodies(PhysicsRagdollBodySaveRecord *out, int capacity) const;
    bool RestoreArticulatedBodies(const PhysicsRagdollBodySaveRecord *input,
        int count, bool frozen, const physx::PxVec3 &originOffset);

    bool IsSleeping() const;
    bool IsFrozen() const { return m_frozen; }
    bool UpdateSleepFreeze(float now, bool enableFreeze, float freezeDelay);
    void Thaw();
    bool SetBodyPhysicsWeight(const char *bodyName, float weight, bool includeChildren);
    bool NeedsAnimationControl() const;
    bool BuildDecalAttachment(const physx::PxVec3 &worldPosition, const physx::PxVec3 &worldNormal, int preferredHitGroup, int &boneIndex, physx::PxVec3 &localPosition, physx::PxVec3 &localNormal) const;
    bool Raycast(const physx::PxVec3 &origin, const physx::PxVec3 &unitDir, float maxDistance, PhysicsAssetRayHit &out) const;
    // J.9: only the specific shape hit at entry may supply its exit.
    bool RaycastBodyExit(int bodyIndex, const physx::PxVec3 &entry,
        const physx::PxVec3 &direction, float depth, physx::PxVec3 &exit, physx::PxVec3 &normal) const;
    bool AddImpulseToBody(int bodyIndex, const physx::PxVec3 &impulse, const physx::PxVec3 &position);
    void AddRadialImpulse(const physx::PxVec3 &origin, float radius, float totalImpulse);
    CBaseEntity *Owner() const { return m_owner; }
    const PhysicsAssetDef &Definition() const { return *m_def; }
    void CollectActors(std::vector<physx::PxActor *> &out) const;

private:
    std::unique_ptr<physicsasset::ContactGroup> m_contactGroup;
    int FindBone(const char *name) const;
    int FindBody(const char *name) const;
    bool BuildReferencePoses();
    physx::PxTransform BoneWorldTransform(int boneIndex, const PhysicsAssetBodyDef &body) const;
    physx::PxTransform BodyLocalTransform(const PhysicsAssetBodyDef &body) const;
    void FindRootBody();
    void DestroyActors();
    void SnapToAnimationPose();
    void SetSimulationEnabled(bool enabled);
    void SetBodyKinematicState(size_t bodyIndex, bool kinematic, bool inheritAnimationVelocity);
    bool IsBodyDescendantOf(size_t bodyIndex, int ancestorIndex) const;
    bool CanFreeze() const;
    void CalculateAnimationVelocities(float dt);
    void InvalidateAnimationHistory(bool resetSequenceState = false);
    physx::PxVec3 FallbackLinearVelocity() const;
    physx::PxVec3 FallbackAngularVelocity() const;

    CBaseEntity *m_owner;
    std::shared_ptr<PhysicsAssetDef> m_def;
    std::vector<PhysicsAssetBodyRuntime> m_bodies;
    std::vector<physx::PxJoint *> m_joints;
    float m_previousSampleTime = 0.0f;
    float m_currentSampleTime = 0.0f;
    bool m_hasCurrentSampleTime = false;
    int m_modelIndex = 0;
    int m_lastSequence = 0;
    bool m_hasSequenceSample = false;
    physx::PxVec3 m_lastOwnerOrigin{ 0.0f };
    bool m_hasOwnerOrigin = false;
    performance::AnimationPoseStamp m_animationPoseStamp;
    float m_rigidScale = 1.0f;
    int m_rootBodyIndex = -1;
    physx::PxBounds3 m_simulationBounds;
    bool m_hasSimulationBounds = false;
    bool m_hasSimulationPose = false;
    bool m_actorsCreated = false;
    bool m_ragdoll = false;
    bool m_frozen = false;
    float m_sleepStartTime = -1.0f;
    bool m_diagnosticsStarted = false;
    double m_diagnosticsBindSpan = 0;
    ragdolldiag::Cadence m_diagnosticsCadence;
    ragdolldiag::Window m_diagnosticsWindow;
};

std::shared_ptr<PhysicsAssetDef> LoadPhysicsAssetDefinition(const char *modelName);
