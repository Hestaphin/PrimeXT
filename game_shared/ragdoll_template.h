#pragma once
#include "physics_asset_format.h"
#include <string>
#include <string_view>
#include <vector>

namespace ragtemplate
{
constexpr int kVersion = 1;
constexpr size_t kMaxFileBytes = 1024 * 1024;
enum class FrameMode { Bone, Segment, Flexion, Explicit };
struct MassRegion { std::string id; float weight = 1; };
struct JointProfile
{
    physicsasset::AngularMotion twistMotion = physicsasset::AngularMotion::Limited;
    physicsasset::AngularMotion swing1Motion = physicsasset::AngularMotion::Limited;
    physicsasset::AngularMotion swing2Motion = physicsasset::AngularMotion::Limited;
    float swing1 = 45, swing2 = 45, twistLow = -45, twistHigh = 45;
    FrameMode frameMode = FrameMode::Bone;
    physicsasset::Vec3 frameAnchor{}, frameAngles{};
    physicsasset::JointTuning tuning;
    int flexionSign = 1; // flips inferred X, keeping positive travel semantics
};
struct Role
{
    std::string id, parent, boneOverride;
    std::vector<std::string> aliases;
    bool required = true;
    int minimumDetail = 0, hitGroup = 0;
    int chainDepth = 0; // bounded descendant edges; zero keeps explicit roles only
    bool chainBranches = false; // branching expansion is an explicit author choice
    physicsasset::ShapeType shape = physicsasset::ShapeType::Capsule;
    float massWeight = 1;
    physicsasset::BodyTuning bodyTuning;
    float linearDamping=-1,angularDamping=-1;
    JointProfile joint;
    std::string massRegion;
};
struct Template
{
    int version = kVersion;
    std::string id, name, description, anatomy = "custom";
    float massBudget = 70;
    physicsasset::SimulationSettings simulation;
    physicsasset::SelfCollisionMode selfCollision=physicsasset::SelfCollisionMode::Off;
    int collisionNeighborDepth=2;
    float collisionOverlapMargin=.1f;
    std::vector<physicsasset::CollisionPair> pairRules;
    std::vector<Role> roles;
    std::vector<MassRegion> massRegions;
    std::vector<std::string> excludedSections{};
    // Whole-bone names/tokens skipped by automatic chains; explicit roles still win.
    std::vector<std::string> chainExcludedBones{};
    float radiusRatio = 0.22f, endTrim = 0.06f, massRatioWarning = 10;
};
struct Bone
{
    std::string name;
    int parent = -1;
    physicsasset::Vec3 localOrigin, childOffset, boundsMin, boundsMax;
    bool hasBounds = false;
    physicsasset::Vec3 localAngles{}; // Studio value[3..5]: radians about X/Y/Z
    std::vector<physicsasset::Vec3> vertices{}; // filtered bind bone-local samples
};
struct Mapping
{
    std::string role, bone, source;
    int boneIndex = -1;
    float confidence = 0;
};
struct Candidate
{
    physicsasset::AssetDef generated, asset;
    std::vector<Mapping> mapping;
    std::vector<std::string> observations;
    unsigned addedBodies = 0, removedBodies = 0, reusedBodies = 0;
    unsigned addedJoints = 0, removedJoints = 0, preservedCollisions = 0, protectedBodies = 0;
    unsigned axisReviews = 0, chainReviews = 0, massRatioWarnings = 0, overlapWarnings = 0;
};
enum class ComposeMode { ReplaceRagdoll, Merge };
bool Validate(const Template &, std::string *error = nullptr);
bool Parse(std::string_view, Template &, std::string *error = nullptr);
std::string Serialize(const Template &);
bool LoadFile(const char *, Template &, std::string *error = nullptr);
bool SaveFile(const char *, const Template &, bool overwrite, std::string *error = nullptr);
const std::vector<Template> &Builtins(); // immutable compiled presets
bool Resolve(const Template &, const std::vector<Bone> &, int detail, std::vector<Mapping> &, std::string *error = nullptr);
bool Generate(const Template &, const std::vector<Bone> &, int detail, const physicsasset::AssetDef &,
    ComposeMode, Candidate &, std::string *error = nullptr);
bool Compose(const physicsasset::AssetDef &, const physicsasset::AssetDef &, ComposeMode, Candidate &, std::string *error = nullptr);
// No live asset mutation until Apply. A stale baseline cannot overwrite later edits.
class Transaction
{
public:
    void Begin(const physicsasset::AssetDef &);
    void Cancel();
    bool SetCandidate(const Candidate &, std::string *error = nullptr);
    bool Apply(physicsasset::AssetDef &, std::string *error = nullptr);
    bool Undo(physicsasset::AssetDef &, std::string *error = nullptr);
    bool HasCandidate() const { return m_active && m_ready; }
    const Candidate &Preview() const { return m_candidate; }
private:
    bool m_active = false, m_ready = false, m_undo = false;
    physicsasset::AssetDef m_before, m_undoBefore;
    Candidate m_candidate;
    std::string m_applied;
};
}
