#include "physics_asset.h"
#include "physics_asset_construction.h"
#include "physics_asset_contact_filter.h"
#include "physics_hinge_frame.h"
#include "physics_d6_frame.h"
#include "physics_asset_motion.h"
#include "physics_asset_scale.h"
#include "physics_asset_diagnostics.h"

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "physic.h"
#include "enginecallback.h"
#include "collision_filter_data.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstring>
#include <functional>
#include <chrono>

using namespace physx;

namespace
{
constexpr float kMinimumPoseDelta = 0.001f;
constexpr float kMaximumPoseDelta = 0.25f;
constexpr float kMaximumTrackedLinearSpeed = 4000.0f;
constexpr float kMaximumTrackedAngularSpeed = 50.0f; // radians/sec

bool FiniteVec3(const PxVec3 &v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

PxVec3 AngularVelocityBetween(const PxQuat &previous, const PxQuat &current, float dt)
{
    PxQuat delta = current * previous.getConjugate();
    delta.normalize();

    // q and -q encode the same orientation. Force the shortest arc so a model
    // crossing the quaternion sign boundary does not generate a huge velocity.
    if(delta.w < 0.0f)
        delta = PxQuat(-delta.x, -delta.y, -delta.z, -delta.w);

    const float w = std::max(-1.0f, std::min(1.0f, delta.w));
    const float angle = 2.0f * std::acos(w);
    const float sinHalf = std::sqrt(std::max(0.0f, 1.0f - w * w));
    if(sinHalf < 1e-5f || angle < 1e-5f)
        return PxVec3(0.0f);

    const PxVec3 axis(delta.x / sinHalf, delta.y / sinHalf, delta.z / sinHalf);
    return axis * (angle / dt);
}
}

std::shared_ptr<PhysicsAssetDef> LoadPhysicsAssetDefinition(const char *modelName)
{
    if(!modelName || !modelName[0]) return nullptr;

    const std::string path = physicsasset::BuildAssetPath(modelName);
    char *file = (char *)LOAD_FILE(path.c_str(), nullptr);
    if(!file) return nullptr;

    auto def = std::make_shared<PhysicsAssetDef>();
    std::string error;
    const bool ok = physicsasset::Parse(file, *def, &error);
    FREE_FILE(file);
    if(!ok)
    {
        ALERT(at_warning, "Failed to parse physics asset %s: %s\n", path.c_str(), error.c_str());
        return nullptr;
    }
    def->sourcePath = path;
    return def;
}

PhysicsAssetInstance::PhysicsAssetInstance(CBaseEntity *owner, std::shared_ptr<PhysicsAssetDef> def)
    : m_owner(owner), m_def(std::move(def))
{
}

PhysicsAssetInstance::~PhysicsAssetInstance()
{
    Destroy();
}

int PhysicsAssetInstance::FindBone(const char *name) const
{
    if(!m_owner || !name || !name[0]) return -1;
    studiohdr_t *hdr = (studiohdr_t *)GET_MODEL_PTR(m_owner->edict());
    if(!hdr || hdr->numbones <= 0) return -1;
    mstudiobone_t *bones = (mstudiobone_t *)((byte *)hdr + hdr->boneindex);
    for(int i = 0; i < hdr->numbones; ++i)
        if(!Q_stricmp(bones[i].name, name)) return i;
    return -1;
}

int PhysicsAssetInstance::FindBody(const char *name) const
{
    return physicsasset::FindBody(*m_def, name);
}

PxTransform PhysicsAssetInstance::BodyLocalTransform(const PhysicsAssetBodyDef &body) const
{
    matrix4x4 local(Vector(body.offset.x, body.offset.y, body.offset.z) * m_rigidScale,
        Vector(body.angles.x, body.angles.y, body.angles.z), 1.0f);
    float mat[16];
    local.CopyToArray(mat);
    return PxTransform(PxMat44(mat));
}

PxTransform PhysicsAssetInstance::BoneWorldTransform(int boneIndex, const PhysicsAssetBodyDef &body) const
{
    Vector origin, angles;
    GET_BONE_POSITION(m_owner->edict(), boneIndex, origin, angles);
    // Studio supplies the unscaled bone pivot. Scale about the entity origin
    // before composing the scaled body-local offset and unscaled orientation.
    const Vector ownerOrigin = m_owner->GetAbsOrigin();
    origin = ownerOrigin + (origin - ownerOrigin) * m_rigidScale;
    matrix4x4 bone(Vector(origin.x, origin.y, origin.z), angles, 1.0f);
    float mat[16];
    bone.CopyToArray(mat);
    return PxTransform(PxMat44(mat)) * BodyLocalTransform(body);
}

void PhysicsAssetInstance::FindRootBody()
{
    m_rootBodyIndex = -1;
    if(!m_def || m_def->bodies.empty()) return;

    std::vector<bool> isChild(m_def->bodies.size(), false);
    for(const auto &joint : m_def->joints)
    {
        const int child = FindBody(joint.child.c_str());
        if(child >= 0 && child < static_cast<int>(isChild.size()))
            isChild[child] = true;
    }

    int rootCount = 0;
    for(size_t i = 0; i < isChild.size(); ++i)
    {
        if(isChild[i]) continue;
        if(m_rootBodyIndex < 0) m_rootBodyIndex = static_cast<int>(i);
        ++rootCount;
    }

    // Shared v1 validation requires one articulated tree. Keep this defensive
    // check here as well because presentation/networking needs one deterministic
    // root and runtime definitions must never silently accept disconnected islands.
    if(rootCount != 1)
    {
        ALERT(at_warning, "%s: physics asset runtime expected exactly one root body, found %d\n",
            m_def->sourcePath.c_str(), rootCount);
        m_rootBodyIndex = -1;
    }
}

bool PhysicsAssetInstance::BuildReferencePoses()
{
    if(!m_owner || !m_def || m_bodies.empty()) return false;
    studiohdr_t *hdr = (studiohdr_t *)GET_MODEL_PTR(m_owner->edict());
    if(!hdr || hdr->numbones <= 0) return false;

    mstudiobone_t *bones = (mstudiobone_t *)((byte *)hdr + hdr->boneindex);
    std::vector<matrix3x4> referenceBones(hdr->numbones);
    std::vector<byte> state(hdr->numbones, 0);
    std::function<bool(int)> buildBone = [&](int i) -> bool
    {
        if(i < 0 || i >= hdr->numbones) return false;
        if(state[i] == 2) return true;
        if(state[i] == 1) return false; // corrupt cyclic skeleton
        state[i] = 1;

        const Vector position = Vector(bones[i].value[0], bones[i].value[1], bones[i].value[2]) * m_rigidScale;
        const Radian rotation(bones[i].value[3], bones[i].value[4], bones[i].value[5]);
        const matrix3x4 local(position, rotation);
        if(bones[i].parent >= 0)
        {
            if(!buildBone(bones[i].parent)) return false;
            referenceBones[i] = referenceBones[bones[i].parent].ConcatTransforms(local);
        }
        else
        {
            referenceBones[i] = local;
        }
        state[i] = 2;
        return true;
    };

    for(int i = 0; i < hdr->numbones; ++i)
    {
        if(!buildBone(i))
        {
            ALERT(at_warning, "%s: invalid/cyclic studio bone hierarchy near '%s'\n",
                m_def->sourcePath.c_str(), bones[i].name);
            return false;
        }
    }

    for(size_t i = 0; i < m_bodies.size(); ++i)
    {
        auto &runtime = m_bodies[i];
        const auto &def = m_def->bodies[i];

        float boneArray[16];
        referenceBones[runtime.boneIndex].CopyToArray(boneArray);
        runtime.referenceBonePose = PxTransform(PxMat44(boneArray));
        runtime.bodyLocalPose = BodyLocalTransform(def);
        runtime.bodyToBonePose = runtime.bodyLocalPose.getInverse();

        const matrix3x4 bodyLocal(Vector(def.offset.x, def.offset.y, def.offset.z) * m_rigidScale,
            Vector(def.angles.x, def.angles.y, def.angles.z), 1.0f);
        const matrix3x4 bodyReference = referenceBones[runtime.boneIndex].ConcatTransforms(bodyLocal);
        float bodyArray[16];
        bodyReference.CopyToArray(bodyArray);
        runtime.referenceBodyPose = PxTransform(PxMat44(bodyArray));
    }
    return true;
}

bool PhysicsAssetInstance::InitializePoseTracking()
{
    if(!m_owner || !m_def) return false;
    if(!m_bodies.empty()) return MatchesOwnerModel();

    m_modelIndex = m_owner->pev->modelindex;
    if(m_modelIndex <= 0) return false;
    const Vector entityScale = m_owner->GetScale();
    if(!physicsasset::RigidUniformScale({entityScale.x, entityScale.y, entityScale.z}, m_rigidScale))
    {
        ALERT(at_warning, "%s: .phys ragdoll requires positive uniform model scale (%.3f %.3f %.3f)\n",
            m_def->sourcePath.c_str(), entityScale.x, entityScale.y, entityScale.z);
        return false;
    }
    m_bodies.resize(m_def->bodies.size());
    for(size_t i = 0; i < m_def->bodies.size(); ++i)
    {
        const auto &def = m_def->bodies[i];
        const int bone = FindBone(def.bone.c_str());
        if(bone < 0)
        {
            ALERT(at_warning, "%s: body '%s' references missing bone '%s'\n", m_def->sourcePath.c_str(), def.name.c_str(), def.bone.c_str());
            m_bodies.clear();
            return false;
        }
        m_bodies[i].boneIndex = bone;
    }

    FindRootBody();
    if(m_rootBodyIndex < 0)
    {
        m_bodies.clear();
        return false;
    }

    if(!BuildReferencePoses())
    {
        m_bodies.clear();
        return false;
    }

    return true;
}

bool PhysicsAssetInstance::MatchesOwnerModel() const
{
    return m_owner && m_modelIndex > 0 && m_owner->pev->modelindex == m_modelIndex;
}

void PhysicsAssetInstance::InvalidateAnimationHistory(bool resetSequenceState)
{
    for(auto &runtime : m_bodies)
    {
        runtime.hasPreviousAnimationPose = false;
        runtime.hasCurrentAnimationPose = false;
        runtime.hasAnimationVelocity = false;
        runtime.animationLinearVelocity = FallbackLinearVelocity();
        runtime.animationAngularVelocity = FallbackAngularVelocity();
    }
    m_previousSampleTime = 0.0f;
    m_currentSampleTime = 0.0f;
    m_hasCurrentSampleTime = false;
    m_hasOwnerOrigin = false;
    if(resetSequenceState)
        m_hasSequenceSample = false;
}

void PhysicsAssetInstance::CalculateAnimationVelocities(float dt)
{
    const PxVec3 fallbackLinear = FallbackLinearVelocity();
    const PxVec3 fallbackAngular = FallbackAngularVelocity();

    const bool validDelta = std::isfinite(dt) && dt >= kMinimumPoseDelta && dt <= kMaximumPoseDelta;
    for(auto &runtime : m_bodies)
    {
        runtime.hasAnimationVelocity = false;
        runtime.animationLinearVelocity = fallbackLinear;
        runtime.animationAngularVelocity = fallbackAngular;

        if(!validDelta || !runtime.hasPreviousAnimationPose || !runtime.hasCurrentAnimationPose)
            continue;

        const PxVec3 linear = (runtime.currentAnimationPose.p - runtime.previousAnimationPose.p) / dt;
        const PxVec3 angular = AngularVelocityBetween(runtime.previousAnimationPose.q, runtime.currentAnimationPose.q, dt);
        if(!FiniteVec3(linear) || !FiniteVec3(angular))
            continue;
        if(linear.magnitude() > kMaximumTrackedLinearSpeed || angular.magnitude() > kMaximumTrackedAngularSpeed)
            continue; // most commonly a teleport or discontinuous sequence reset

        runtime.animationLinearVelocity = linear;
        runtime.animationAngularVelocity = angular;
        runtime.hasAnimationVelocity = true;
    }
}

void PhysicsAssetInstance::UpdateAnimationPose(float sampleTime)
{
    if(!m_owner || !m_def) return;
    if(m_ragdoll && !NeedsAnimationControl()) return;
    if(m_bodies.empty() && !InitializePoseTracking()) return;

    if(!std::isfinite(sampleTime)) sampleTime = 0.0f;
    if(!MatchesOwnerModel()) return;

    const int sequence = m_owner->pev->sequence;
    const bool sequenceChanged = m_hasSequenceSample && sequence != m_lastSequence;

    const Vector ownerOriginVector = m_owner->GetAbsOrigin();
    const PxVec3 ownerOrigin(ownerOriginVector.x, ownerOriginVector.y, ownerOriginVector.z);
    bool teleported = false;
    if(m_hasOwnerOrigin && m_hasCurrentSampleTime && sampleTime > m_currentSampleTime + 1e-6f)
    {
        const float ownerDt = sampleTime - m_currentSampleTime;
        const float displacement = (ownerOrigin - m_lastOwnerOrigin).magnitude();
        const float expectedTravel = FallbackLinearVelocity().magnitude() * ownerDt;
        const float teleportThreshold = std::max(128.0f, expectedTravel * 2.0f + 64.0f);
        teleported = displacement > teleportThreshold;
    }

    if(sequenceChanged || teleported)
    {
        // Sequence switches and teleports are discontinuities, not physical limb
        // motion. Never turn them into a launch velocity at the ragdoll handoff.
        InvalidateAnimationHistory(false);
    }
    m_lastSequence = sequence;
    m_hasSequenceSample = true;
    m_lastOwnerOrigin = ownerOrigin;
    m_hasOwnerOrigin = true;

    // Time can move backwards on level transitions/save restores. Reset rather
    // than deriving a nonsensical velocity from poses belonging to two timelines.
    if(m_hasCurrentSampleTime && sampleTime + 1e-6f < m_currentSampleTime)
    {
        InvalidateAnimationHistory(false);
    }

    const bool advanceHistory = !m_hasCurrentSampleTime || sampleTime > m_currentSampleTime + 1e-6f;
    if(advanceHistory && m_hasCurrentSampleTime)
    {
        m_previousSampleTime = m_currentSampleTime;
        for(auto &runtime : m_bodies)
        {
            if(runtime.hasCurrentAnimationPose)
            {
                runtime.previousAnimationPose = runtime.currentAnimationPose;
                runtime.hasPreviousAnimationPose = true;
            }
        }
    }

    for(size_t i = 0; i < m_bodies.size(); ++i)
    {
        auto &runtime = m_bodies[i];
        runtime.currentAnimationPose = BoneWorldTransform(runtime.boneIndex, m_def->bodies[i]);
        runtime.hasCurrentAnimationPose = true;
    }

    if(advanceHistory)
    {
        m_currentSampleTime = sampleTime;
        if(!m_hasCurrentSampleTime)
            m_previousSampleTime = sampleTime;
        m_hasCurrentSampleTime = true;
    }

    // Multiple animation operations may occur at the same server timestamp.
    // Recalculate against the previous distinct frame so the final pose of the
    // current timestamp is what contributes to the death velocity.
    if(m_hasCurrentSampleTime && m_currentSampleTime > m_previousSampleTime + 1e-6f)
        CalculateAnimationVelocities(m_currentSampleTime - m_previousSampleTime);
    else
        CalculateAnimationVelocities(0.0f);
}

bool PhysicsAssetInstance::CreateActors(PxPhysics *physics, PxScene *scene, PxMaterial *material)
{
    if(m_actorsCreated) return true;
    if(!physics || !scene || !material || !m_owner) return false;
    if(!InitializePoseTracking()) return false;

    // A ragdoll must scale bone/reference pivots, shape sizes and joint frames
    // together. Reject anisotropic/mirrored scale rather than silently leaving
    // ragdoll and rendered mesh in different coordinate systems.
    const Vector entityScale = m_owner->GetScale();
    float scale = 1.0f;
    if(!physicsasset::RigidUniformScale({entityScale.x, entityScale.y, entityScale.z}, scale))
    {
        ALERT(at_warning, "%s: articulated .phys requires positive uniform entity scale (got %.3f %.3f %.3f)\n",
            m_def->sourcePath.c_str(), entityScale.x, entityScale.y, entityScale.z);
        return false;
    }
    // InitializePoseTracking() may have been called earlier for velocity
    // history. Recompute reference frames when the first actor is created.
    m_rigidScale = scale;
    if(!BuildReferencePoses()) return false;

    UpdateAnimationPose(gpGlobals ? gpGlobals->time : 0.0f);

    std::string settingsError;
    if(!physicsasset::Validate(*m_def,&settingsError)) {ALERT(at_warning,"%s\n",settingsError.c_str());return false;}
    m_contactGroup.reset(new physicsasset::ContactGroup(*m_def));
    for(size_t i = 0; i < m_def->bodies.size(); ++i)
    {
        const auto &def = m_def->bodies[i];
        auto &runtime = m_bodies[i];
        const PxTransform initialPose = runtime.hasCurrentAnimationPose
            ? runtime.currentAnimationPose
            : BoneWorldTransform(runtime.boneIndex, def);

        PxRigidDynamic *actor=nullptr;PxShape *shape=nullptr;
        if(!physicsasset::CreatePrimitiveBody(*physics,*material,initialPose,*m_def,i,scale,actor,shape)) {DestroyActors();return false;}
        actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);

        // Existing PrimeXT PhysX callbacks interpret actor userData as edict_t*.
        actor->userData = m_owner->edict();

        // A .phys body is a simulation representation, not an implicit query
        // collider. It remains collision-disabled while animation owns it.
        shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
        shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);

        shape->setSimulationFilterData(physicsasset::AssetBodyFilterData(static_cast<PxU32>(ENTINDEX(m_owner->edict())),i,*m_contactGroup));
        shape->userData=const_cast<physicsasset::BodyContactToken *>(m_contactGroup->Token(i));

        scene->addActor(*actor);
        runtime.actor = actor;
        runtime.shape = shape;
    }

    for(const auto &def : m_def->joints)
    {
        const int parentIndex = FindBody(def.parent.c_str());
        const int childIndex = FindBody(def.child.c_str());
        if(parentIndex < 0 || childIndex < 0)
        {
            ALERT(at_warning, "%s: joint '%s' references unknown bodies\n", m_def->sourcePath.c_str(), def.name.c_str());
            DestroyActors(); return false;
        }

        PxRigidDynamic *parent = m_bodies[parentIndex].actor;
        PxRigidDynamic *child = m_bodies[childIndex].actor;

        PxJoint *joint=physicsasset::CreateAssetJoint(*physics,*parent,*child,m_bodies[parentIndex].referenceBodyPose,
            m_bodies[childIndex].referenceBodyPose,m_bodies[childIndex].referenceBonePose,*m_def,def,scale);
        if(!joint) {DestroyActors();return false;}m_joints.push_back(joint);
    }

    m_actorsCreated = true;
    SnapToAnimationPose();
    return true;
}

void PhysicsAssetInstance::CollectActors(std::vector<PxActor *> &out) const
{
    for(const auto &body : m_bodies)
        if(body.actor) out.push_back(body.actor);
}

void PhysicsAssetInstance::DestroyActors()
{
    m_diagnosticsStarted = false;
    m_diagnosticsBindSpan = 0;
    m_diagnosticsCadence.Reset();
    m_diagnosticsWindow.Reset();
    for(PxJoint *joint : m_joints) if(joint) joint->release();
    m_joints.clear();
    for(auto &body : m_bodies)
    {
        if(body.actor) body.actor->release();
        body.actor = nullptr;
        body.shape = nullptr;
    }
    m_contactGroup.reset(); // every actor/shape released before immutable tokens
    m_actorsCreated = false;
    m_ragdoll = false;
    m_frozen = false;
    m_sleepStartTime = -1.0f;
    m_hasSimulationPose = false;
    m_hasSimulationBounds = false;
    for(auto &body : m_bodies) body.hasSimulationPose = false;
}

void PhysicsAssetInstance::Destroy()
{
    DestroyActors();
    m_bodies.clear();
    m_hasCurrentSampleTime = false;
    m_hasSequenceSample = false;
    m_hasOwnerOrigin = false;
    m_modelIndex = 0;
    m_rootBodyIndex = -1;
    m_hasSimulationPose = false;
    m_hasSimulationBounds = false;
    m_previousSampleTime = m_currentSampleTime = 0.0f;
}

void PhysicsAssetInstance::SnapToAnimationPose()
{
    if(!m_actorsCreated) return;
    for(size_t i = 0; i < m_bodies.size(); ++i)
    {
        auto &runtime = m_bodies[i];
        if(!runtime.actor) continue;
        const PxTransform pose = runtime.hasCurrentAnimationPose
            ? runtime.currentAnimationPose
            : BoneWorldTransform(runtime.boneIndex, m_def->bodies[i]);
        // setGlobalPose is intentional at ownership boundaries. Kinematic
        // targets are consumed by a future simulation step and can otherwise
        // leave the dynamic ragdoll one pose behind the death animation.
        runtime.actor->setGlobalPose(pose, false);
    }
}

void PhysicsAssetInstance::SetSimulationEnabled(bool enabled)
{
    for(auto &runtime : m_bodies)
    {
        if(!runtime.shape) continue;
        runtime.shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, enabled);
        runtime.shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, false);
    }
}

void PhysicsAssetInstance::FollowAnimation()
{
    if(m_ragdoll || !m_actorsCreated) return;

    // Do not advance velocity history here. PhysX can tick much faster than
    // GoldSrc StudioFrameAdvance(), which would turn a 10 Hz animation pose
    // change into a 100 Hz velocity spike followed by zero samples. Kinematic
    // bodies may still follow the exact current pose without recording it.
    for(size_t i = 0; i < m_bodies.size(); ++i)
    {
        auto &runtime = m_bodies[i];
        if(!runtime.actor) continue;
        runtime.actor->setKinematicTarget(BoneWorldTransform(runtime.boneIndex, m_def->bodies[i]));
    }
}

bool PhysicsAssetInstance::NeedsAnimationControl() const
{
    if(!m_ragdoll) return false;
    for(const auto &body : m_bodies)
        if(body.physicsWeight < 0.999f) return true;
    return false;
}

void PhysicsAssetInstance::SetBodyKinematicState(size_t bodyIndex, bool kinematic, bool inheritAnimationVelocity)
{
    if(bodyIndex >= m_bodies.size()) return;
    auto &body = m_bodies[bodyIndex];
    if(!body.actor) return;

    const bool isKinematic = body.actor->getRigidBodyFlags().isSet(PxRigidBodyFlag::eKINEMATIC);
    if(isKinematic == kinematic) return;

    if(kinematic)
    {
        body.actor->setLinearVelocity(PxVec3(0.0f));
        body.actor->setAngularVelocity(PxVec3(0.0f));
        body.actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
        const PxTransform pose = body.hasCurrentAnimationPose
            ? body.currentAnimationPose
            : BoneWorldTransform(body.boneIndex, m_def->bodies[bodyIndex]);
        if(pose.isValid()) body.actor->setGlobalPose(pose, false);
    }
    else
    {
        body.actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
        body.actor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD,physicsasset::ResolveBodySettings(*m_def,m_def->bodies[bodyIndex]).ccd);
        if(inheritAnimationVelocity)
        {
            body.actor->setLinearVelocity(body.hasAnimationVelocity ? body.animationLinearVelocity : FallbackLinearVelocity());
            body.actor->setAngularVelocity(body.hasAnimationVelocity ? body.animationAngularVelocity : FallbackAngularVelocity());
        }
        else
        {
            body.actor->setLinearVelocity(PxVec3(0.0f));
            body.actor->setAngularVelocity(PxVec3(0.0f));
        }
        body.actor->wakeUp();
    }
}

bool PhysicsAssetInstance::IsBodyDescendantOf(size_t bodyIndex, int ancestorIndex) const
{
    if(ancestorIndex < 0 || bodyIndex >= m_bodies.size()) return false;
    int current = static_cast<int>(bodyIndex);
    for(size_t guard = 0; guard <= m_bodies.size(); ++guard)
    {
        if(current == ancestorIndex) return true;
        int parent = -1;
        for(const auto &joint : m_def->joints)
        {
            if(FindBody(joint.child.c_str()) != current) continue;
            parent = FindBody(joint.parent.c_str());
            break;
        }
        if(parent < 0 || parent == current) break;
        current = parent;
    }
    return false;
}

bool PhysicsAssetInstance::SetBodyPhysicsWeight(const char *bodyName, float weight, bool includeChildren)
{
    if(!m_ragdoll || !m_actorsCreated || !std::isfinite(weight)) return false;
    weight = std::max(0.0f, std::min(1.0f, weight));

    int selected = -1;
    if(bodyName && bodyName[0])
    {
        selected = FindBody(bodyName);
        if(selected < 0) return false;
    }

    std::vector<size_t> changed;
    for(size_t i = 0; i < m_bodies.size(); ++i)
    {
        const bool match = selected < 0 || static_cast<int>(i) == selected || (includeChildren && IsBodyDescendantOf(i, selected));
        if(!match) continue;
        if(std::fabs(m_bodies[i].physicsWeight - weight) <= 1e-4f) continue;
        m_bodies[i].physicsWeight = weight;
        changed.push_back(i);
    }
    if(changed.empty()) return true;

    Thaw();
    // Once an animated anchor/drive is requested, refresh target poses in the
    // same entity frame before changing kinematic state.
    UpdateAnimationPose(gpGlobals ? gpGlobals->time : m_currentSampleTime);
    for(size_t i : changed)
        SetBodyKinematicState(i, m_bodies[i].physicsWeight <= 0.001f, true);
    m_sleepStartTime = -1.0f;
    return true;
}

void PhysicsAssetInstance::ApplyAnimationControl(float dt, float strength, float damping, float maxForce)
{
    if(!m_ragdoll || !m_actorsCreated || !NeedsAnimationControl()) return;
    if(!std::isfinite(dt) || dt <= 0.0f) return;
    if(m_frozen) Thaw();

    strength = std::max(0.0f, std::min(strength, 100000.0f));
    damping = std::max(0.0f, std::min(damping, 10000.0f));
    maxForce = std::max(0.0f, std::min(maxForce, 1000000.0f));

    for(size_t i = 0; i < m_bodies.size(); ++i)
    {
        auto &body = m_bodies[i];
        if(!body.actor) continue;
        const float physicsWeight = std::max(0.0f, std::min(1.0f, body.physicsWeight));
        const PxTransform target = body.hasCurrentAnimationPose
            ? body.currentAnimationPose
            : BoneWorldTransform(body.boneIndex, m_def->bodies[i]);
        if(!target.isValid()) continue;

        if(physicsWeight <= 0.001f)
        {
            SetBodyKinematicState(i, true, false);
            body.actor->setKinematicTarget(target);
            continue;
        }

        SetBodyKinematicState(i, false, true);
        if(physicsWeight >= 0.999f || strength <= 0.0f || maxForce <= 0.0f) continue;

        const float drive = 1.0f - physicsWeight;
        const PxTransform current = body.actor->getGlobalPose();
        if(!current.isValid()) continue;

        const PxVec3 desiredLinear = body.hasAnimationVelocity ? body.animationLinearVelocity : FallbackLinearVelocity();
        PxVec3 force = (target.p - current.p) * (strength * drive)
            + (desiredLinear - body.actor->getLinearVelocity()) * (damping * drive);
        const float forceMag = force.magnitude();
        const float bodyForceLimit = maxForce * drive;
        if(std::isfinite(forceMag) && forceMag > bodyForceLimit && forceMag > 1e-5f)
            force *= bodyForceLimit / forceMag;
        if(FiniteVec3(force)) body.actor->addForce(force, PxForceMode::eFORCE);

        PxQuat error = target.q * current.q.getConjugate();
        error.normalize();
        if(error.w < 0.0f) error = PxQuat(-error.x, -error.y, -error.z, -error.w);
        const float w = std::max(-1.0f, std::min(1.0f, error.w));
        const float angle = 2.0f * std::acos(w);
        const float sinHalf = std::sqrt(std::max(0.0f, 1.0f - w * w));
        PxVec3 angularError(0.0f);
        if(sinHalf > 1e-5f && angle > 1e-5f)
            angularError = PxVec3(error.x, error.y, error.z) * (angle / sinHalf);
        const PxVec3 desiredAngular = body.hasAnimationVelocity ? body.animationAngularVelocity : FallbackAngularVelocity();
        PxVec3 torque = angularError * (strength * drive)
            + (desiredAngular - body.actor->getAngularVelocity()) * (damping * drive);
        const float torqueMag = torque.magnitude();
        const float torqueLimit = bodyForceLimit * 0.5f;
        if(std::isfinite(torqueMag) && torqueMag > torqueLimit && torqueMag > 1e-5f)
            torque *= torqueLimit / torqueMag;
        if(FiniteVec3(torque)) body.actor->addTorque(torque, PxForceMode::eFORCE);
    }
}

void PhysicsAssetInstance::CaptureSimulationPose()
{
    if(!m_ragdoll || !m_actorsCreated)
    {
        m_hasSimulationPose = false;
        return;
    }
    // Frozen actors are immutable until Thaw or an explicit restore. Keep
    // their completed bone poses/bounds instead of rereading every body.
    if(m_frozen && m_hasSimulationPose && m_hasSimulationBounds) return;

    bool haveBounds = false;
    PxBounds3 bounds;
    bool havePose = false;

    for(size_t i = 0; i < m_bodies.size(); ++i)
    {
        auto &runtime = m_bodies[i];
        runtime.hasSimulationPose = false;
        if(!runtime.actor) continue;

        const PxTransform bodyWorld = runtime.actor->getGlobalPose();
        if(!bodyWorld.isValid()) continue;

        // actor = bone * authoredBodyLocal, therefore
        // bone = actor * inverse(authoredBodyLocal).
        const PxTransform boneWorld = bodyWorld * runtime.bodyToBonePose;
        if(!boneWorld.isValid()) continue;

        runtime.simulatedBonePose = boneWorld;
        runtime.hasSimulationPose = true;
        havePose = true;

        const PxBounds3 actorBounds = runtime.actor->getWorldBounds(1.02f);
        if(actorBounds.isValid())
        {
            if(!haveBounds)
            {
                bounds = actorBounds;
                haveBounds = true;
            }
            else
            {
                bounds.minimum.x = std::min(bounds.minimum.x, actorBounds.minimum.x);
                bounds.minimum.y = std::min(bounds.minimum.y, actorBounds.minimum.y);
                bounds.minimum.z = std::min(bounds.minimum.z, actorBounds.minimum.z);
                bounds.maximum.x = std::max(bounds.maximum.x, actorBounds.maximum.x);
                bounds.maximum.y = std::max(bounds.maximum.y, actorBounds.maximum.y);
                bounds.maximum.z = std::max(bounds.maximum.z, actorBounds.maximum.z);
            }
        }
    }

    m_hasSimulationPose = havePose;
    m_hasSimulationBounds = haveBounds;
    if(haveBounds) m_simulationBounds = bounds;
}

void PhysicsAssetInstance::RecordDiagnostics(float step, const PxSimulationStatistics *sceneStats, float mode,
    float inputFrameDelta, float accumulator)
{
    if(!std::isfinite(mode) || mode <= 0 || !m_ragdoll || !m_actorsCreated)
    {
        if(m_diagnosticsStarted)
        {
            m_diagnosticsStarted = false;
            m_diagnosticsCadence.Reset();
            m_diagnosticsWindow.Reset();
        }
        return;
    }
    if(!std::isfinite(step) || step <= 0) return;
    if(!m_diagnosticsStarted)
    {
        m_diagnosticsStarted = true;
        ragdolldiag::ReferenceSpan span;
        for(const auto &body : m_bodies) span.Add(ragdolldiag::DiagnosticVector(body.referenceBonePose.p));
        m_diagnosticsBindSpan = span.Value();
        ALERT(at_console,"[ragdoll_f0] ent=%d asset=%s root_body=%d schema=1 contacts=scene_non_ccd pose=server_capture\n",
            m_owner ? m_owner->entindex() : -1,m_def->sourcePath.c_str(),m_rootBodyIndex);
        ALERT(at_console,"[ragdoll_f0] ent=%d header=%s\n",m_owner ? m_owner->entindex() : -1,ragdolldiag::CsvHeader());
    }
    const auto start = std::chrono::steady_clock::now();
    ragdolldiag::Snapshot sample;
    sample.stepSeconds = step;
    sample.inputFrameSeconds = std::isfinite(inputFrameDelta) ? inputFrameDelta : -1;
    sample.accumulatorSeconds = std::isfinite(accumulator) ? accumulator : -1;
    if(!std::isfinite(inputFrameDelta) || !std::isfinite(accumulator)) ++sample.invalidSamples;
    sample.bindSpan = m_diagnosticsBindSpan;
    sample.pose.valid = true;
    sample.pose.bones = static_cast<unsigned>(m_bodies.size());
    for(size_t i = 0; i < m_bodies.size(); ++i)
    {
        const auto &body = m_bodies[i];
        ragdolldiag::MeasureBody(body.actor,sample,static_cast<int>(i) == m_rootBodyIndex);
        if(!body.actor || !body.hasSimulationPose) { ++sample.pose.invalidBones; continue; }
        const PxTransform expected = body.actor->getGlobalPose() * body.bodyToBonePose;
        if(!expected.isValid() || !body.simulatedBonePose.isValid()) { ++sample.pose.invalidBones; continue; }
        ++sample.pose.drivenCompared;
        ragdolldiag::ComparePose(sample.pose,ragdolldiag::DiagnosticPose(body.simulatedBonePose),
            ragdolldiag::DiagnosticPose(expected),body.boneIndex,sample.pose.drivenPositionDelta,
            sample.pose.drivenAngleDelta,sample.pose.worstDrivenBone);
    }
    for(size_t i = 0; i < m_joints.size(); ++i) ragdolldiag::MeasureJoint(m_joints[i],sample,static_cast<int>(i));
    ragdolldiag::MeasureScene(sceneStats,sample);
    sample.samplingMilliseconds = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    m_diagnosticsWindow.Push(sample);
    if(m_diagnosticsCadence.Advance(step,mode >= 2 ? 0 : 1.0))
    {
        sample = m_diagnosticsWindow.Take();
        sample.simulationSeconds = m_diagnosticsCadence.Elapsed();
        const std::string row = ragdolldiag::CsvRow(sample);
        ALERT(at_console,"[ragdoll_f0] ent=%d data=%s\n",m_owner ? m_owner->entindex() : -1,row.c_str());
    }
}


bool PhysicsAssetInstance::IsSleeping() const
{
    if(!m_ragdoll || !m_actorsCreated) return false;
    if(m_frozen) return true;

    bool haveDynamicActor = false;
    for(const auto &body : m_bodies)
    {
        if(!body.actor || body.physicsWeight <= 0.001f) continue;
        haveDynamicActor = true;
        if(!body.actor->isSleeping()) return false;
    }
    return haveDynamicActor;
}

bool PhysicsAssetInstance::CanFreeze() const
{
    if(!m_ragdoll || !m_actorsCreated || NeedsAnimationControl()) return false;
    for(const auto &body : m_bodies)
        if(body.actor && body.physicsWeight < 0.999f) return false;
    return true;
}

void PhysicsAssetInstance::Thaw()
{
    if(!m_frozen) return;
    m_frozen = false;
    m_sleepStartTime = -1.0f;
    for(size_t i = 0; i < m_bodies.size(); ++i)
    {
        if(!m_bodies[i].actor) continue;
        SetBodyKinematicState(i, m_bodies[i].physicsWeight <= 0.001f, false);
    }
}

bool PhysicsAssetInstance::UpdateSleepFreeze(float now, bool enableFreeze, float freezeDelay)
{
    if(!m_ragdoll || !m_actorsCreated) return false;
    if(!std::isfinite(now)) now = 0.0f;
    freezeDelay = std::max(0.0f, std::min(freezeDelay, 60.0f));

    if(!enableFreeze || !CanFreeze())
    {
        if(m_frozen) Thaw();
        m_sleepStartTime = -1.0f;
        return m_frozen;
    }
    if(m_frozen) return true;

    if(!IsSleeping())
    {
        m_sleepStartTime = -1.0f;
        return false;
    }
    if(m_sleepStartTime < 0.0f || now + 1e-6f < m_sleepStartTime)
    {
        m_sleepStartTime = now;
        return false;
    }
    if(now - m_sleepStartTime + 1e-6f < freezeDelay) return false;

    // Sleeping dynamic bodies have already converged. Kinematic-locking them in
    // place removes them from the active solver while preserving exact body poses
    // for direct bullet queries. Any later impulse calls Thaw() first.
    for(size_t i = 0; i < m_bodies.size(); ++i)
    {
        auto &body = m_bodies[i];
        if(!body.actor || body.physicsWeight <= 0.001f) continue;
        body.actor->setLinearVelocity(PxVec3(0.0f));
        body.actor->setAngularVelocity(PxVec3(0.0f));
        body.actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
    }
    m_frozen = true;
    return true;
}

bool PhysicsAssetInstance::BuildDecalAttachment(const PxVec3 &worldPosition, const PxVec3 &worldNormal, int preferredHitGroup, int &boneIndex, PxVec3 &localPosition, PxVec3 &localNormal) const
{
    boneIndex = -1;
    if(!m_ragdoll || !m_actorsCreated || !FiniteVec3(worldPosition) || !FiniteVec3(worldNormal)) return false;

    int bestBody = -1;
    float bestDistSq = FLT_MAX;
    for(size_t pass = 0; pass < 2 && bestBody < 0; ++pass)
    {
        for(size_t i = 0; i < m_bodies.size(); ++i)
        {
            const auto &body = m_bodies[i];
            if(!body.actor) continue;
            if(pass == 0 && preferredHitGroup != 0 && m_def->bodies[i].hitGroup != preferredHitGroup) continue;
            const float distSq = (body.actor->getGlobalPose().p - worldPosition).magnitudeSquared();
            if(!std::isfinite(distSq) || distSq >= bestDistSq) continue;
            bestDistSq = distSq;
            bestBody = static_cast<int>(i);
        }
        if(preferredHitGroup == 0) break;
    }
    if(bestBody < 0) return false;

    const auto &runtime = m_bodies[bestBody];
    PxTransform boneWorld = runtime.hasSimulationPose
        ? runtime.simulatedBonePose
        : runtime.actor->getGlobalPose() * runtime.bodyToBonePose;
    if(!boneWorld.isValid()) return false;
    const PxTransform worldToBone = boneWorld.getInverse();
    localPosition = worldToBone.transform(worldPosition);
    localNormal = worldToBone.q.rotate(worldNormal);
    if(!FiniteVec3(localPosition) || !FiniteVec3(localNormal) || localNormal.normalize() <= 1e-5f) return false;
    boneIndex = runtime.boneIndex;
    return boneIndex >= 0;
}


bool PhysicsAssetInstance::Raycast(const PxVec3 &origin, const PxVec3 &unitDir, float maxDistance, PhysicsAssetRayHit &out) const
{
    if(!m_ragdoll || !m_actorsCreated || maxDistance <= 0.0f || !FiniteVec3(origin) || !FiniteVec3(unitDir))
        return false;

    bool found = false;
    float closest = maxDistance;
    for(size_t i = 0; i < m_bodies.size(); ++i)
    {
        const auto &runtime = m_bodies[i];
        if(!runtime.actor || !runtime.shape) continue;

        const PxGeometryHolder geometry = runtime.shape->getGeometry();
        const PxTransform shapePose = runtime.actor->getGlobalPose() * runtime.shape->getLocalPose();
        if(!shapePose.isValid()) continue;

        PxRaycastHit hit;
        const PxHitFlags flags = PxHitFlag::ePOSITION | PxHitFlag::eNORMAL;
        if(PxGeometryQuery::raycast(origin, unitDir, geometry.any(), shapePose, closest, flags, 1, &hit) == 0)
            continue;
        if(!std::isfinite(hit.distance) || hit.distance < 0.0f || hit.distance > closest)
            continue;
        if(found && hit.distance + 0.0001f >= closest)
            continue;
        if(!FiniteVec3(hit.position) || !FiniteVec3(hit.normal))
            continue;

        closest = hit.distance;
        out.distance = hit.distance;
        out.position = hit.position;
        out.normal = hit.normal;
        out.bodyIndex = static_cast<int>(i);
        out.boneIndex = runtime.boneIndex;
        out.hitGroup = m_def->bodies[i].hitGroup;
        found = true;
    }
    return found;
}

bool PhysicsAssetInstance::RaycastBodyExit(int bodyIndex, const PxVec3 &entry,
    const PxVec3 &direction, float depth, PxVec3 &outExit, PxVec3 &outNormal) const
{
    if(!m_ragdoll || !m_actorsCreated || bodyIndex < 0 ||
       bodyIndex >= static_cast<int>(m_bodies.size()) ||
       !FiniteVec3(entry) || !FiniteVec3(direction) ||
       !std::isfinite(depth) || depth <= 1.0f) return false;
    const auto &body = m_bodies[bodyIndex];
    if(!body.actor || !body.shape) return false;
    const PxTransform pose = body.actor->getGlobalPose() * body.shape->getLocalPose();
    if(!pose.isValid()) return false;
    const PxGeometryHolder geometry = body.shape->getGeometry();
    const PxHitFlags flags = PxHitFlag::ePOSITION | PxHitFlag::eNORMAL;
    PxRaycastHit first, back;
    // Verify that this exact shape is present at the incoming contact.
    if(!PxGeometryQuery::raycast(entry-direction*0.5f, direction, geometry.any(),
            pose, 1.0f, flags, 1, &first) ||
        !FiniteVec3(first.position) || (first.position-entry).magnitude()>0.75f)
        return false;
    // Raycast the same shape from behind, rather than a scene-wide trace,
    // which could silently substitute a different ragdoll limb.
    if(!PxGeometryQuery::raycast(entry+direction*depth, -direction, geometry.any(),
            pose, depth-0.5f, flags, 1, &back) ||
        !FiniteVec3(back.position) || !FiniteVec3(back.normal) ||
        back.normal.dot(direction)<0.15f) return false;
    const float along=(back.position-entry).dot(direction);
    if(!std::isfinite(along) || along<=0.5f || along>=depth-0.5f ||
        (back.position-entry-direction*along).magnitude()>0.75f) return false;
    outExit=back.position;
    outNormal=back.normal;
    return true;
}

bool PhysicsAssetInstance::AddImpulseToBody(int bodyIndex, const PxVec3 &impulse, const PxVec3 &position)
{
    if(!m_ragdoll || !m_actorsCreated || bodyIndex < 0 || bodyIndex >= static_cast<int>(m_bodies.size()))
        return false;
    auto &runtime = m_bodies[bodyIndex];
    if(!runtime.actor || runtime.physicsWeight <= 0.001f || !FiniteVec3(impulse) || !FiniteVec3(position))
        return false;
    Thaw();
    SetBodyKinematicState(static_cast<size_t>(bodyIndex), false, false);
    PxRigidBodyExt::addForceAtPos(*runtime.actor, impulse, position, PxForceMode::eIMPULSE);
    runtime.actor->wakeUp();
    m_sleepStartTime = -1.0f;
    return true;
}

void PhysicsAssetInstance::AddRadialImpulse(const PxVec3 &origin, float radius, float totalImpulse)
{
    if(!m_ragdoll || !m_actorsCreated || radius <= 0.0f || totalImpulse <= 0.0f || !FiniteVec3(origin) || !std::isfinite(radius) || !std::isfinite(totalImpulse))
        return;

    Thaw();
    float totalMass = 0.0f;
    for(const auto &runtime : m_bodies)
        if(runtime.actor && runtime.physicsWeight > 0.001f) totalMass += runtime.actor->getMass();
    if(totalMass <= 0.0f || !std::isfinite(totalMass)) return;

    for(auto &runtime : m_bodies)
    {
        if(!runtime.actor || runtime.physicsWeight <= 0.001f) continue;
        const PxVec3 center = runtime.actor->getGlobalPose().p;
        PxVec3 delta = center - origin;
        const float distance = delta.magnitude();
        if(!std::isfinite(distance) || distance >= radius) continue;
        if(distance > 1e-4f) delta /= distance;
        else delta = PxVec3(0.0f, 0.0f, 1.0f);

        const float falloff = 1.0f - distance / radius;
        const float massShare = runtime.actor->getMass() / totalMass;
        const PxVec3 impulse = delta * (totalImpulse * falloff * massShare);
        PxRigidBodyExt::addForceAtPos(*runtime.actor, impulse, center, PxForceMode::eIMPULSE);
        runtime.actor->wakeUp();
    }
    m_sleepStartTime = -1.0f;
}

bool PhysicsAssetInstance::GetRootBonePose(PxTransform &out) const
{
    if(!m_hasSimulationPose || m_rootBodyIndex < 0 || m_rootBodyIndex >= static_cast<int>(m_bodies.size()))
        return false;
    const auto &root = m_bodies[m_rootBodyIndex];
    if(!root.hasSimulationPose) return false;
    out = root.simulatedBonePose;
    return true;
}

bool PhysicsAssetInstance::GetSimulationBounds(PxBounds3 &out) const
{
    if(!m_hasSimulationPose || !m_hasSimulationBounds || !m_simulationBounds.isValid()) return false;
    out = m_simulationBounds;
    return true;
}

PxVec3 PhysicsAssetInstance::FallbackLinearVelocity() const
{
    if(!m_owner) return PxVec3(0.0f);
    const Vector v = m_owner->GetAbsVelocity();
    return PxVec3(v.x, v.y, v.z);
}

PxVec3 PhysicsAssetInstance::FallbackAngularVelocity() const
{
    if(!m_owner) return PxVec3(0.0f);
    const Vector v = m_owner->GetAbsAvelocity();
    return PxVec3(PxPi * v.x / 180.0f, PxPi * v.y / 180.0f, PxPi * v.z / 180.0f);
}

bool PhysicsAssetInstance::SaveArticulatedBodies(PhysicsRagdollBodySaveRecord *out, int capacity) const
{
    if(!m_ragdoll || !m_actorsCreated || !out || capacity <= 0 ||
        m_bodies.size() > static_cast<size_t>(capacity)) return false;
    for(size_t i=0; i<m_bodies.size(); ++i)
    {
        const auto &body=m_bodies[i];
        const auto &def=m_def->bodies[i];
        if(!body.actor || def.name.empty() || def.name.size() >= sizeof(out[i].name)) return false;
        const PxTransform pose=body.actor->getGlobalPose();
        if(!pose.isValid()) return false;
        PhysicsRagdollBodySaveRecord &rec=out[i];
        std::memset(&rec,0,sizeof(rec));
        std::memcpy(rec.name,def.name.c_str(),def.name.size());
        rec.pose[0]=pose.p.x; rec.pose[1]=pose.p.y; rec.pose[2]=pose.p.z;
        rec.pose[3]=pose.q.x; rec.pose[4]=pose.q.y;
        rec.pose[5]=pose.q.z; rec.pose[6]=pose.q.w;
        const PxVec3 linear=body.actor->getLinearVelocity();
        const PxVec3 angular=body.actor->getAngularVelocity();
        rec.linear[0]=linear.x; rec.linear[1]=linear.y; rec.linear[2]=linear.z;
        rec.angular[0]=angular.x; rec.angular[1]=angular.y; rec.angular[2]=angular.z;
        rec.weight=body.physicsWeight;
        rec.sleeping=body.actor->isSleeping() ? 1 : 0;
    }
    return true;
}

bool PhysicsAssetInstance::RestoreArticulatedBodies(
    const PhysicsRagdollBodySaveRecord *input,int count,bool frozen,
    const PxVec3 &originOffset)
{
    if(!m_actorsCreated || !m_ragdoll || !input || count<=0 ||
        static_cast<size_t>(count)!=m_bodies.size() || !originOffset.isFinite()) return false;
    // Validate the ENTIRE snapshot before changing any actor or constraints.
    for(int i=0;i<count;++i)
    {
        const auto &rec=input[i];
        if(!std::memchr(rec.name,0,sizeof(rec.name)) ||
            m_def->bodies[i].name != rec.name || !m_bodies[i].actor ||
            !std::isfinite(rec.weight) || rec.weight<0.0f || rec.weight>1.0f ||
            (rec.sleeping!=0 && rec.sleeping!=1)) return false;
        const PxTransform pose(PxVec3(rec.pose[0],rec.pose[1],rec.pose[2]),
            PxQuat(rec.pose[3],rec.pose[4],rec.pose[5],rec.pose[6]));
        const PxVec3 linear(rec.linear[0],rec.linear[1],rec.linear[2]);
        const PxVec3 angular(rec.angular[0],rec.angular[1],rec.angular[2]);
        if(!pose.isValid() || !linear.isFinite() || !angular.isFinite()) return false;
    }
    for(int i=0;i<count;++i)
    {
        const auto &rec=input[i];
        auto &body=m_bodies[i];
        const PxTransform pose(PxVec3(rec.pose[0],rec.pose[1],rec.pose[2]),
            PxQuat(rec.pose[3],rec.pose[4],rec.pose[5],rec.pose[6]));
        body.physicsWeight=rec.weight;
        const bool kinematic=(rec.weight<=0.001f || (frozen && rec.weight>=0.999f));
        body.actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC,kinematic);
        body.actor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD,!kinematic&&physicsasset::ResolveBodySettings(*m_def,m_def->bodies[size_t(i)]).ccd);
        body.actor->setGlobalPose(PxTransform(pose.p+originOffset,pose.q),false);
        if(!kinematic)
        {
            body.actor->setLinearVelocity(PxVec3(rec.linear[0],rec.linear[1],rec.linear[2]));
            body.actor->setAngularVelocity(PxVec3(rec.angular[0],rec.angular[1],rec.angular[2]));
            if(rec.sleeping) body.actor->putToSleep();
            else body.actor->wakeUp();
        }
    }
    m_frozen=frozen && !NeedsAnimationControl();
    m_sleepStartTime=-1.0f;
    m_hasSimulationPose = m_hasSimulationBounds = false;
    CaptureSimulationPose();
    return true;
}

bool PhysicsAssetInstance::SetRagdoll(bool enabled)
{
    if(m_ragdoll == enabled) return true;
    if(!m_actorsCreated) return false;

    if(enabled)
    {
        // Capture the exact current animation pose at the transition, including
        // changes made later in the same server timestamp as the previous sample.
        UpdateAnimationPose(gpGlobals ? gpGlobals->time : m_currentSampleTime);
        SnapToAnimationPose();
        SetSimulationEnabled(true);

        const PxVec3 fallbackLinear = FallbackLinearVelocity();
        const PxVec3 fallbackAngular = FallbackAngularVelocity();
        m_frozen = false;
        m_sleepStartTime = -1.0f;
        for(size_t i = 0; i < m_bodies.size(); ++i)
        {
            auto &body = m_bodies[i];
            body.physicsWeight = 1.0f;
            if(!body.actor) continue;
            body.actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
            // Animation ownership cleared CCD when these actors became kinematic.
            body.actor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD,
                physicsasset::ResolveBodySettings(*m_def, m_def->bodies[i]).ccd);
            body.actor->setLinearVelocity(body.hasAnimationVelocity ? body.animationLinearVelocity : fallbackLinear);
            body.actor->setAngularVelocity(body.hasAnimationVelocity ? body.animationAngularVelocity : fallbackAngular);
            body.actor->wakeUp();
        }
        m_ragdoll = true;
    }
    else
    {
        // Stop contacts first, then return ownership to animation. Zero dynamic
        // velocities before making bodies kinematic so stale ragdoll momentum is
        // never reused if the asset is activated again later.
        SetSimulationEnabled(false);
        for(auto &body : m_bodies)
        {
            if(!body.actor) continue;
            body.actor->setLinearVelocity(PxVec3(0.0f));
            body.actor->setAngularVelocity(PxVec3(0.0f));
            body.actor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
        }
        m_ragdoll = false;
        m_frozen = false;
        m_sleepStartTime = -1.0f;
        for(auto &body : m_bodies) body.physicsWeight = 1.0f;
        m_hasSimulationPose = false;
        m_hasSimulationBounds = false;
        for(auto &body : m_bodies) body.hasSimulationPose = false;

        // The pre-ragdoll animation samples belong to an old ownership epoch.
        // If we immediately ragdoll again, never derive limb velocity across the
        // entire simulated-ragdoll interval or across a restarted get-up clip.
        InvalidateAnimationHistory(true);
        UpdateAnimationPose(gpGlobals ? gpGlobals->time : 0.0f);
        SnapToAnimationPose();
    }
    return true;
}
