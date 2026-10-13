#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace physicsasset
{
constexpr int kCurrentVersion = 5;
constexpr size_t kMaxAssetBodies = 256;
constexpr float kPhysicsLength=1.0f/0.0254f, kPhysicsSpeed=800.0f;
constexpr float kDefaultContactOffset=0.02f*kPhysicsLength;

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

enum class ShapeType
{
    Box,
    Sphere,
    Capsule,
};

struct ShapeDef
{
    ShapeType type = ShapeType::Capsule;
    Vec3 halfExtents{ 4.0f, 4.0f, 4.0f };
    float radius = 4.0f;
    float halfHeight = 8.0f;
};

// Authored collision geometry replaces the old sibling _cp.mdl runtime convention.
// Sections with an empty bodyGroup are always active. Sections with a named
// bodyGroup are active only when the render MDL selects the matching variant.
enum class CollisionGeometryType
{
    TriangleMesh,
    Convex,
};

struct CollisionDef
{
    std::string name;
    CollisionGeometryType type = CollisionGeometryType::TriangleMesh;
    std::string bodyGroup;
    int variant = -1;
    // Optional rigid Studio-bone binding (v3). Empty means model/entity space.
    // Bone-bound collision vertices are authored in that bone's local space.
    std::string bone;
    bool disabled = false;
    std::vector<Vec3> vertices;
    std::vector<std::uint32_t> indices; // triangle-list indices
};

enum class PropDriveMode { None, Animated, Driven, Free };
enum class JointKind { D6, Hinge };
enum class AngularMotion { Locked, Limited, Free };

enum class ProjectionMode { Inherit=-1, Off, On };
enum class SelfCollisionMode { Off, Selective, Custom };
struct SimulationSettings
{
    float linearDamping=0.05f, angularDamping=0.05f;
    int positionIterations=16, velocityIterations=4;
    float maxLinearVelocity=2000, maxAngularVelocity=20, maxDepenetrationVelocity=200;
    float sleepThreshold=0.5f, contactOffset=kDefaultContactOffset, restOffset=0;
    bool ccd=true;
    float angularResistance=0, maxTorque=1000;
    ProjectionMode projection=ProjectionMode::Off;
    float projectionLinearTolerance=0.25f, projectionAngularTolerance=10;
};
struct BodyTuning
{
    int positionIterations=-1, velocityIterations=-1;
    float maxLinearVelocity=-1, maxAngularVelocity=-1, maxDepenetrationVelocity=-1, sleepThreshold=-1;
    int ccd=-1;
    float contactOffset=-1, restOffset=-1;
};
struct JointTuning
{
    float angularResistance=-1, maxTorque=-1;
    ProjectionMode projection=ProjectionMode::Inherit;
    float projectionLinearTolerance=-1, projectionAngularTolerance=-1;
};
struct CollisionPair { std::string first,second; bool enabled=false; };
struct BodyDef
{
    std::string name;
    std::string bone;
    ShapeDef shape;
    Vec3 offset;
    Vec3 angles;
    float mass = 1.0f;
    float linearDamping = -1;
    float angularDamping = -1;
    BodyTuning tuning;
    // Gameplay damage zone associated with this physical body. 0 is generic;
    // values 1..7 match the standard Half-Life hitgroups. Mods may use higher
    // values, so the file format preserves the full byte range.
    int hitGroup = 0;
    // Opt-in articulated prop metadata. Ordinary bodies use None.
    PropDriveMode propDrive = PropDriveMode::None;
    std::string part;
    float driveStrength = 0.0f;
    float driveDamping = 0.0f;
    // Maximum motor torque. 0 selects the server tuning default.
    float driveMaxForce = 0.0f;
};

struct JointDef
{
    std::string name;
    std::string parent;
    std::string child;
    float swing1 = 45.0f;
    float swing2 = 45.0f;
    float twistLow = -45.0f;
    float twistHigh = 45.0f;
    AngularMotion twistMotion = AngularMotion::Limited;
    AngularMotion swing1Motion = AngularMotion::Limited;
    AngularMotion swing2Motion = AngularMotion::Limited;
    // F2: resolved D6 frame in the CHILD reference bone's local space.
    // Euler pitch/yaw/roll in degrees, matching BodyDef::angles. Translation
    // remains locked; the parent frame is derived from this same bind pivot.
    Vec3 frameAnchor{};
    Vec3 frameAngles{};
    JointTuning tuning;
    JointKind kind = JointKind::D6;
    float hingeMin = 0.0f;
    float hingeMax = 90.0f;
    // Pivot and axis in the CHILD Studio bone's local space.
    // PhysX revolute joints rotate around local X, so the runtime constructs
    // an oriented joint frame from this normalized (non-zero) axis.
    Vec3 hingeAnchor{};
    Vec3 hingeAxis{ 1.0f, 0.0f, 0.0f };
    float breakImpulse = 0.0f; // single impact impulse magnitude; 0 = disabled
    float releaseDamage = 0.0f; // accumulated gameplay damage to release motor; 0 = disabled
    float breakDamage = 0.0f; // accumulated gameplay damage to detach hinge; 0 = disabled
};

struct AssetDef
{
    int version = kCurrentVersion;
    SimulationSettings simulation;
    SelfCollisionMode selfCollision=SelfCollisionMode::Off;
    std::vector<CollisionPair> collisionPairs;
    std::string sourcePath;
    std::uint64_t collisionFingerprint = 0; // complete .phys file contents; 0 = legacy/no asset
    std::vector<CollisionDef> collisions;
    std::vector<BodyDef> bodies;
    std::vector<JointDef> joints;
};

std::string BuildAssetPath(const std::string &modelPath);
bool Parse(std::string_view text, AssetDef &out, std::string *error = nullptr);
bool Validate(const AssetDef &asset, std::string *error = nullptr);
std::string Serialize(const AssetDef &asset);
bool LoadFile(const char *path, AssetDef &out, std::string *error = nullptr);
bool SaveFile(const char *path, const AssetDef &asset, std::string *error = nullptr);
int FindBody(const AssetDef &asset, const char *name);
const char *ShapeTypeName(ShapeType type);
bool ParseShapeType(const char *name, ShapeType &type);
const char *CollisionGeometryTypeName(CollisionGeometryType type);
bool ParseCollisionGeometryType(const char *name, CollisionGeometryType &type);
bool HasCollisionGeometry(const AssetDef &asset);
bool HasArticulatedBodies(const AssetDef &asset);
const char *AngularMotionName(AngularMotion motion);
bool ParseAngularMotion(const char *name, AngularMotion &motion);
const char *ProjectionModeName(ProjectionMode);
bool ParseProjectionMode(const char *,ProjectionMode &);
const char *SelfCollisionModeName(SelfCollisionMode);
bool ParseSelfCollisionMode(const char *,SelfCollisionMode &);
}
