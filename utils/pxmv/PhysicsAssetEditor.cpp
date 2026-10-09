#include "PhysicsAssetEditor.h"
#include "LegacyCpStudioValidator.h"
#include "physics_asset_scale.h"
#include "physics_asset_sidecar_removal.h"

#include "StudioModel.h"
#include "ViewerSettings.h"
#include "GlWindow.h"
#include "mdlviewer.h"
#include "app_info.h"
#include "matrix.h"
#include "mathlib.h"
#include "stringlib.h"

#include <mx.h>
#include <gl.h>
#include <mx/mxButton.h>
#include <mx/mxChoice.h>
#include <mx/mxLabel.h>
#include <mx/mxLineEdit.h>
#include <mx/mxListBox.h>
#include <mx/mxMessageBox.h>

#include <algorithm>
#include <array>
#include <map>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <limits>
#include <vector>

#ifdef PHYSICS_ASSET_EDITOR_PHYSX
#include <PxPhysicsAPI.h>
#include "../../server/physx/physics_hinge_frame.h"
using namespace physx;
#endif

namespace
{
static void SetFloat(mxLineEdit *edit, float value)
{
    if(edit) edit->setLabel(va("%.9g", value));
}

// Reject malformed interactive values rather than silently falling back to
// their previous values. This is important for hinge limits and break thresholds.
static bool ReadStrictFloat(mxLineEdit *edit, float &value)
{
    if(!edit || !edit->getLabel() || !edit->getLabel()[0]) return false;
    char *end = nullptr;
    const float parsed = std::strtof(edit->getLabel(), &end);
    if(end == edit->getLabel() || !end || *end != '\0' || !std::isfinite(parsed)) return false;
    value = parsed;
    return true;
}

static bool ReadStrictInt(mxLineEdit *edit, int &value)
{
    if(!edit || !edit->getLabel() || !edit->getLabel()[0]) return false;
    char *end = nullptr;
    const char *text = edit->getLabel();
    const long parsed = std::strtol(text, &end, 10);
    if(end == text || !end || *end != '\0' ||
       parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max())
        return false;
    value = static_cast<int>(parsed);
    return true;
}

static std::string Lower(const char *name);

static int SuggestedHitGroup(const char *boneName)
{
    const std::string n = Lower(boneName);
    if(n.find("head") != std::string::npos) return 1;
    if(n.find("chest") != std::string::npos || n.find("spine") != std::string::npos) return 2;
    if(n.find("pelvis") != std::string::npos || n.find("stomach") != std::string::npos || n.find("abdomen") != std::string::npos || n.find("hip") != std::string::npos) return 3;
    const bool left = n.find("left") != std::string::npos || n.find(" l ") != std::string::npos || n.find("_l") != std::string::npos || n.find(" l_") != std::string::npos;
    const bool right = n.find("right") != std::string::npos || n.find(" r ") != std::string::npos || n.find("_r") != std::string::npos || n.find(" r_") != std::string::npos;
    if(n.find("arm") != std::string::npos || n.find("forearm") != std::string::npos || n.find("hand") != std::string::npos) return left ? 4 : right ? 5 : 0;
    if(n.find("leg") != std::string::npos || n.find("thigh") != std::string::npos || n.find("calf") != std::string::npos || n.find("foot") != std::string::npos) return left ? 6 : right ? 7 : 0;
    return 0;
}

static Vector ToVector(const physicsasset::Vec3 &v) { return Vector(v.x, v.y, v.z); }
static physicsasset::Vec3 ToAssetVec(const Vector &v) { return { v.x, v.y, v.z }; }

static matrix3x4 BodyLocalMatrix(const physicsasset::BodyDef &body)
{
    return matrix3x4(ToVector(body.offset), ToVector(body.angles), 1.0f);
}

static std::string Lower(const char *name)
{
    std::string value = name ? name : "";
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return value;
}

static bool ContainsAny(const std::string &value, const char *const *needles, size_t count)
{
    for(size_t i = 0; i < count; ++i)
        if(value.find(needles[i]) != std::string::npos) return true;
    return false;
}

static bool ShouldSkipAutoBone(const char *name)
{
    const std::string n = Lower(name);
    static const char *const skip[] = {
        "finger", "thumb", "toe", "twist", "helper", "nub", "attachment",
        "weapon", "muzzle", "camera", "eye", "jaw", "tongue", "cloth", "jiggle"
    };
    return ContainsAny(n, skip, sizeof(skip) / sizeof(skip[0]));
}

static bool IsLeafPhysicsBone(const char *name)
{
    const std::string n = Lower(name);
    static const char *const keep[] = { "head", "hand", "foot" };
    return ContainsAny(n, keep, sizeof(keep) / sizeof(keep[0]));
}

static float ShapeVolume(const physicsasset::BodyDef &body)
{
    constexpr float pi = 3.14159265358979323846f;
    switch(body.shape.type)
    {
    case physicsasset::ShapeType::Box:
        return std::max(8.0f * body.shape.halfExtents.x * body.shape.halfExtents.y * body.shape.halfExtents.z, 0.001f);
    case physicsasset::ShapeType::Sphere:
        return std::max((4.0f / 3.0f) * pi * body.shape.radius * body.shape.radius * body.shape.radius, 0.001f);
    case physicsasset::ShapeType::Capsule:
    default:
    {
        const float r = body.shape.radius;
        const float cylinder = pi * r * r * (body.shape.halfHeight * 2.0f);
        const float sphere = (4.0f / 3.0f) * pi * r * r * r;
        return std::max(cylinder + sphere, 0.001f);
    }
    }
}

static std::string LegacyCollisionProxyPath(const std::string &modelPath)
{
    const size_t dot = modelPath.find_last_of('.');
    if(dot == std::string::npos) return modelPath + "_cp.mdl";
    return modelPath.substr(0, dot) + "_cp.mdl";
}

static bool BuildLegacyReferenceBones(studiohdr_t *hdr, std::vector<matrix3x4> &out, std::string &error)
{
    if(!hdr || hdr->numbones <= 0 || hdr->numbones > MAXSTUDIOBONES)
    {
        error = "Legacy collision proxy has an invalid bone table.";
        return false;
    }

    mstudiobone_t *bones = reinterpret_cast<mstudiobone_t *>(reinterpret_cast<byte *>(hdr) + hdr->boneindex);
    mstudioanim_t *anim = nullptr;
    if(hdr->numseq > 0)
    {
        mstudioseqdesc_t *seq = reinterpret_cast<mstudioseqdesc_t *>(reinterpret_cast<byte *>(hdr) + hdr->seqindex);
        if(seq->seqgroup != 0)
        {
            error = "Legacy _cp.mdl sequence 0 is external; importer requires sequence group 0.";
            return false;
        }
        mstudioseqgroup_t *group = reinterpret_cast<mstudioseqgroup_t *>(reinterpret_cast<byte *>(hdr) + hdr->seqgroupindex) + seq->seqgroup;
        anim = reinterpret_cast<mstudioanim_t *>(reinterpret_cast<byte *>(hdr) + group->data + seq->animindex);
    }

    out.resize(hdr->numbones);
    for(int i = 0; i < hdr->numbones; ++i)
    {
        Vector pos;
        Radian angle;
        for(int axis = 0; axis < 3; ++axis)
        {
            pos[axis] = bones[i].value[axis];
            angle[axis] = bones[i].value[axis + 3];
            if(anim)
            {
                if(anim[i].offset[axis] != 0)
                {
                    mstudioanimvalue_t *value = reinterpret_cast<mstudioanimvalue_t *>(reinterpret_cast<byte *>(&anim[i]) + anim[i].offset[axis]);
                    pos[axis] += value[1].value * bones[i].scale[axis];
                }
                if(anim[i].offset[axis + 3] != 0)
                {
                    mstudioanimvalue_t *value = reinterpret_cast<mstudioanimvalue_t *>(reinterpret_cast<byte *>(&anim[i]) + anim[i].offset[axis + 3]);
                    angle[axis] += value[1].value * bones[i].scale[axis + 3];
                }
            }
        }
        Vector4D q;
        AngleQuaternion(angle, q);
        const matrix3x4 local(pos, q);
        if(bones[i].parent >= 0)
        {
            if(bones[i].parent >= i)
            {
                error = "Legacy _cp.mdl has a non-parent-first bone hierarchy.";
                return false;
            }
            out[i] = out[bones[i].parent].ConcatTransforms(local);
        }
        else out[i] = local;
    }
    return true;
}

static bool ExtractLegacyCollisionProxy(const char *path, studiohdr_t *renderHdr, std::vector<physicsasset::CollisionDef> &out, std::string &error)
{
    std::ifstream file(path, std::ios::binary);
    if(!file)
    {
        error = std::string("Cannot open legacy collision proxy: ") + path;
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff length = file.tellg();
    file.seekg(0, std::ios::beg);
    // Hard ceiling avoids a corrupted / hostile _cp.mdl exhausting editor RAM.
    constexpr std::streamoff kMaxProxyBytes = 256 * 1024 * 1024;
    if(length < static_cast<std::streamoff>(sizeof(studiohdr_t)) || length > kMaxProxyBytes)
    {
        error = "Legacy collision proxy has an invalid or excessive file size.";
        return false;
    }
    std::vector<byte> bytes(static_cast<size_t>(length));
    if(!file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(length)))
    {
        error = "Failed reading complete legacy collision proxy.";
        return false;
    }

    // No Studio pointer arithmetic below is permitted until all accessed
    // tables, animation samples and terminated triangle streams are checked.
    if(!legacycp::Validate(bytes.data(), bytes.size(), error)) return false;
    studiohdr_t *hdr = reinterpret_cast<studiohdr_t *>(bytes.data());

    std::vector<matrix3x4> referenceBones;
    if(!BuildLegacyReferenceBones(hdr, referenceBones, error)) return false;
    mstudiobone_t *proxyBones = reinterpret_cast<mstudiobone_t *>(bytes.data() + hdr->boneindex);
    mstudiobone_t *renderBones = renderHdr ? reinterpret_cast<mstudiobone_t *>(reinterpret_cast<byte *>(renderHdr) + renderHdr->boneindex) : nullptr;

    auto renderHasBone = [&](const char *name) -> bool
    {
        if(!renderHdr || !renderBones || !name || !name[0]) return false;
        for(int i = 0; i < renderHdr->numbones; ++i)
            if(!Q_stricmp(renderBones[i].name, name)) return true;
        return false;
    };

    out.clear();
    mstudiobodyparts_t *bodyParts = reinterpret_cast<mstudiobodyparts_t *>(bytes.data() + hdr->bodypartindex);
    for(int bodyPartIndex = 0; bodyPartIndex < hdr->numbodyparts; ++bodyPartIndex)
    {
        mstudiobodyparts_t &part = bodyParts[bodyPartIndex];
        if(part.nummodels <= 0) continue;
        mstudiomodel_t *models = reinterpret_cast<mstudiomodel_t *>(bytes.data() + part.modelindex);
        for(int variant = 0; variant < part.nummodels; ++variant)
        {
            mstudiomodel_t &submodel = models[variant];
            const std::string baseName = part.name[0] ? std::string(part.name) + "_" + std::to_string(variant)
                                                       : "collision_" + std::to_string(bodyPartIndex) + "_" + std::to_string(variant);
            if(submodel.numverts <= 0 || submodel.nummesh <= 0)
            {
                if(part.nummodels > 1)
                {
                    physicsasset::CollisionDef disabled;
                    disabled.name = baseName;
                    disabled.type = physicsasset::CollisionGeometryType::TriangleMesh;
                    disabled.bodyGroup = part.name;
                    disabled.variant = variant;
                    disabled.disabled = true;
                    out.push_back(std::move(disabled));
                }
                continue;
            }

            Vector *studioVerts = reinterpret_cast<Vector *>(bytes.data() + submodel.vertindex);
            byte *vertBone = bytes.data() + submodel.vertinfoindex;
            for(int i = 0; i < submodel.numverts; ++i)
            {
                if(vertBone[i] >= referenceBones.size())
                {
                    error = "Legacy collision proxy has an invalid vertex bone index.";
                    return false;
                }
            }

            std::vector<std::array<std::uint32_t, 3>> triangles;
            mstudiomesh_t *meshes = reinterpret_cast<mstudiomesh_t *>(bytes.data() + submodel.meshindex);
            for(int meshIndex = 0; meshIndex < submodel.nummesh; ++meshIndex)
            {
                short *commands = reinterpret_cast<short *>(bytes.data() + meshes[meshIndex].triindex);
                for(;;)
                {
                    int count = *commands++;
                    if(count == 0) break;
                    const bool strip = count > 0;
                    if(count < 0) count = -count;
                    std::vector<std::uint32_t> commandVertices;
                    commandVertices.reserve(count);
                    for(int i = 0; i < count; ++i, commands += 4)
                    {
                        const int index = commands[0];
                        if(index < 0 || index >= submodel.numverts)
                        {
                            error = "Legacy collision proxy contains an invalid triangle-command vertex.";
                            return false;
                        }
                        commandVertices.push_back(static_cast<std::uint32_t>(index));
                    }
                    if(commandVertices.size() < 3) continue;
                    triangles.push_back({ commandVertices[0], commandVertices[1], commandVertices[2] });
                    for(size_t i = 3; i < commandVertices.size(); ++i)
                    {
                        if(strip)
                        {
                            if((i & 1u) != 0u) triangles.push_back({ commandVertices[i - 1], commandVertices[i - 2], commandVertices[i] });
                            else triangles.push_back({ commandVertices[i - 2], commandVertices[i - 1], commandVertices[i] });
                        }
                        else triangles.push_back({ commandVertices[0], commandVertices[i - 1], commandVertices[i] });
                    }
                }
            }

            if(triangles.empty())
            {
                if(part.nummodels > 1)
                {
                    physicsasset::CollisionDef disabled;
                    disabled.name = baseName;
                    disabled.type = physicsasset::CollisionGeometryType::TriangleMesh;
                    disabled.bodyGroup = part.name;
                    disabled.variant = variant;
                    disabled.disabled = true;
                    out.push_back(std::move(disabled));
                }
                continue;
            }

            // A Studio v10 proxy assigns one bone per vertex. Rigid triangles
            // whose three vertices use the same bone can be preserved exactly
            // as v3 bone-local collision. Mixed-bone triangles cannot be a
            // rigid kinematic piece, so keep those in model/reference space.
            std::map<int, std::vector<std::array<std::uint32_t, 3>>> groups;
            for(const auto &tri : triangles)
            {
                const int b0 = vertBone[tri[0]];
                const int b1 = vertBone[tri[1]];
                const int b2 = vertBone[tri[2]];
                int groupBone = -1;
                // Keep root-bone geometry entity/model-space: it is the prop's
                // chassis/static collision and should stay on the normal root actor.
                // Non-root rigid bones become v3 kinematic collision pieces.
                if(b0 == b1 && b0 == b2 && b0 >= 0 && b0 < hdr->numbones &&
                    proxyBones[b0].parent >= 0 && renderHasBone(proxyBones[b0].name))
                    groupBone = b0;
                groups[groupBone].push_back(tri);
            }

            for(const auto &group : groups)
            {
                const int boneIndex = group.first;
                physicsasset::CollisionDef collision;
                collision.name = baseName;
                if(boneIndex >= 0) collision.name += "_" + std::string(proxyBones[boneIndex].name);
                else if(groups.size() > 1) collision.name += "_root";
                collision.type = physicsasset::CollisionGeometryType::TriangleMesh;
                if(part.nummodels > 1)
                {
                    collision.bodyGroup = part.name;
                    collision.variant = variant;
                }
                if(boneIndex >= 0) collision.bone = proxyBones[boneIndex].name;

                std::map<std::uint32_t, std::uint32_t> remap;
                for(const auto &tri : group.second)
                {
                    for(int corner = 0; corner < 3; ++corner)
                    {
                        const std::uint32_t oldIndex = tri[corner];
                        auto found = remap.find(oldIndex);
                        std::uint32_t newIndex;
                        if(found == remap.end())
                        {
                            newIndex = static_cast<std::uint32_t>(collision.vertices.size());
                            remap.emplace(oldIndex, newIndex);
                            Vector v = studioVerts[oldIndex];
                            if(boneIndex < 0)
                                v = referenceBones[vertBone[oldIndex]].VectorTransform(v);
                            collision.vertices.push_back({ v.x, v.y, v.z });
                        }
                        else newIndex = found->second;
                        collision.indices.push_back(newIndex);
                    }
                }
                if(!collision.indices.empty()) out.push_back(std::move(collision));
            }
        }
    }

    if(out.empty())
    {
        error = "Legacy collision proxy contains no triangle geometry.";
        return false;
    }
    return true;
}

static void DrawUnitCircle(int axisA, int axisB, float centerX, float radius, int segments = 24)
{
    glBegin(GL_LINE_LOOP);
    for(int i = 0; i < segments; ++i)
    {
        const float a = (float)i / (float)segments * (float)(M_PI * 2.0);
        float v[3] = { centerX, 0.0f, 0.0f };
        v[axisA] = std::cos(a) * radius;
        v[axisB] = std::sin(a) * radius;
        glVertex3fv(v);
    }
    glEnd();
}

static void DrawBox(const physicsasset::Vec3 &h)
{
    const float x = h.x, y = h.y, z = h.z;
    const float p[8][3] = {
        {-x,-y,-z},{x,-y,-z},{x,y,-z},{-x,y,-z},
        {-x,-y,z},{x,-y,z},{x,y,z},{-x,y,z}
    };
    const int e[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    };
    glBegin(GL_LINES);
    for(const auto &edge : e) { glVertex3fv(p[edge[0]]); glVertex3fv(p[edge[1]]); }
    glEnd();
}

static void DrawSphere(float radius)
{
    DrawUnitCircle(1, 2, 0.0f, radius);
    DrawUnitCircle(0, 2, 0.0f, radius);
    DrawUnitCircle(0, 1, 0.0f, radius);
}

static void DrawCapsule(float radius, float halfHeight)
{
    DrawUnitCircle(1, 2, -halfHeight, radius);
    DrawUnitCircle(1, 2, halfHeight, radius);
    glBegin(GL_LINES);
    glVertex3f(-halfHeight, radius, 0); glVertex3f(halfHeight, radius, 0);
    glVertex3f(-halfHeight,-radius, 0); glVertex3f(halfHeight,-radius, 0);
    glVertex3f(-halfHeight,0, radius); glVertex3f(halfHeight,0, radius);
    glVertex3f(-halfHeight,0,-radius); glVertex3f(halfHeight,0,-radius);
    glEnd();

    const int segments = 12;
    for(int plane = 0; plane < 2; ++plane)
    {
        glBegin(GL_LINE_STRIP);
        for(int i = 0; i <= segments; ++i)
        {
            const float a = -0.5f * (float)M_PI + (float)i / (float)segments * (float)M_PI;
            const float x = halfHeight + std::cos(a) * radius;
            const float side = std::sin(a) * radius;
            if(plane == 0) glVertex3f(x, side, 0); else glVertex3f(x, 0, side);
        }
        glEnd();
        glBegin(GL_LINE_STRIP);
        for(int i = 0; i <= segments; ++i)
        {
            const float a = 0.5f * (float)M_PI + (float)i / (float)segments * (float)M_PI;
            const float x = -halfHeight + std::cos(a) * radius;
            const float side = std::sin(a) * radius;
            if(plane == 0) glVertex3f(x, side, 0); else glVertex3f(x, 0, side);
        }
        glEnd();
    }
}
}

struct PhysicsAssetEditor::PreviewImpl
{
#ifdef PHYSICS_ASSET_EDITOR_PHYSX
    PxDefaultAllocator allocator;
    PxDefaultErrorCallback errorCallback;
    PxFoundation *foundation = nullptr;
    PxPhysics *physics = nullptr;
    PxDefaultCpuDispatcher *dispatcher = nullptr;
    PxScene *scene = nullptr;
    PxMaterial *material = nullptr;
    PxRigidStatic *ground = nullptr;
    bool extensionsInitialized = false;
    std::vector<PxRigidDynamic *> bodies;
    std::vector<PxJoint *> joints;
    float accumulator = 0.0f;

    ~PreviewImpl() { Shutdown(); }

    bool Initialize()
    {
        if(physics) return true;
        foundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator, errorCallback);
        if(!foundation) return false;
        physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, PxTolerancesScale(), false, nullptr);
        if(!physics) { Shutdown(); return false; }
        if(!PxInitExtensions(*physics, nullptr)) { Shutdown(); return false; }
        extensionsInitialized = true;
        dispatcher = PxDefaultCpuDispatcherCreate(1);
        if(!dispatcher) { Shutdown(); return false; }
        material = physics->createMaterial(0.55f, 0.45f, 0.1f);
        if(!material) { Shutdown(); return false; }
        return true;
    }

    void ClearScene()
    {
        for(PxJoint *joint : joints) if(joint) joint->release();
        joints.clear();
        for(PxRigidDynamic *body : bodies) if(body) body->release();
        bodies.clear();
        if(ground) { ground->release(); ground = nullptr; }
        if(scene) { scene->release(); scene = nullptr; }
        accumulator = 0.0f;
    }

    void Shutdown()
    {
        ClearScene();
        if(material) { material->release(); material = nullptr; }
        if(dispatcher) { dispatcher->release(); dispatcher = nullptr; }
        if(physics)
        {
            if(extensionsInitialized) PxCloseExtensions();
            extensionsInitialized = false;
            physics->release();
            physics = nullptr;
        }
        if(foundation) { foundation->release(); foundation = nullptr; }
    }
#else
    ~PreviewImpl() = default;
#endif
};

PhysicsAssetEditor::PhysicsAssetEditor(mxWindow *parent, StudioModel &model, ViewerSettings &settings)
    : mxWindow(parent, 40, 40, 730, 705, "Physics Asset Editor", mxWindow::Dialog),
      m_model(model), m_settings(settings), m_preview(new PreviewImpl)
{
    BuildUI();
    setVisible(false);
}

PhysicsAssetEditor::~PhysicsAssetEditor()
{
    StopSimulation();
}

void PhysicsAssetEditor::BuildUI()
{
    new mxLabel(this, 10, 8, 180, 18, "Bodies");
    m_bodyList = new mxListBox(this, 10, 26, 180, 245, IDC_PHYS_BODY_LIST);
    new mxButton(this, 10, 278, 56, 22, "Add", IDC_PHYS_BODY_ADD);
    new mxButton(this, 70, 278, 56, 22, "Remove", IDC_PHYS_BODY_REMOVE);
    new mxButton(this, 130, 278, 60, 22, "Apply", IDC_PHYS_BODY_APPLY);

    int x = 205;
    new mxLabel(this, x, 8, 75, 18, "Body name");
    m_bodyName = new mxLineEdit(this, x + 80, 6, 155, 20, "");
    new mxLabel(this, x, 35, 75, 18, "Bone");
    m_boneChoice = new mxChoice(this, x + 80, 32, 155, 22, IDC_PHYS_BONE);
    new mxLabel(this, x, 62, 75, 18, "Shape");
    m_shapeChoice = new mxChoice(this, x + 80, 59, 155, 22, IDC_PHYS_SHAPE);
    m_shapeChoice->add("Capsule"); m_shapeChoice->add("Box"); m_shapeChoice->add("Sphere");

    const char *axes[] = { "X", "Y", "Z" };
    for(int i = 0; i < 3; ++i)
    {
        new mxLabel(this, x + i * 80, 91, 20, 18, axes[i]);
        m_offset[i] = new mxLineEdit(this, x + 18 + i * 80, 88, 55, 20, "0");
        new mxLabel(this, x + i * 80, 116, 20, 18, axes[i]);
        m_angles[i] = new mxLineEdit(this, x + 18 + i * 80, 113, 55, 20, "0");
        m_dimensionLabels[i] = new mxLabel(this, x + i * 80, 141, 20, 18, axes[i]);
        m_dimensions[i] = new mxLineEdit(this, x + 18 + i * 80, 138, 55, 20, "1");
    }
    new mxLabel(this, x, 72 + 14, 70, 18, "Offset");
    new mxLabel(this, x, 97 + 14, 70, 18, "Angles");
    new mxLabel(this, x, 122 + 14, 70, 18, "Dimensions");

    new mxLabel(this, x, 171, 70, 18, "Mass");
    m_mass = new mxLineEdit(this, x + 80, 168, 70, 20, "1");
    new mxLabel(this, x, 198, 70, 18, "Linear damp");
    m_linearDamping = new mxLineEdit(this, x + 80, 195, 70, 20, "0.05");
    new mxLabel(this, x, 225, 70, 18, "Angular damp");
    m_angularDamping = new mxLineEdit(this, x + 80, 222, 70, 20, "0.05");
    new mxLabel(this, x, 252, 70, 18, "Hitgroup");
    m_hitGroup = new mxLineEdit(this, x + 80, 249, 70, 20, "0");

    new mxButton(this, 205, 280, 112, 23, "Make Part", IDC_PHYS_MAKE_PART);
    new mxLabel(this, 205, 317, 240, 18, "Interactive part (v4)");
    new mxLabel(this, 205, 342, 75, 18, "Part name");
    m_partName = new mxLineEdit(this, 285, 339, 155, 20, "");
    new mxLabel(this, 205, 369, 75, 18, "Drive mode");
    m_propDriveChoice = new mxChoice(this, 285, 366, 155, 22);
    m_propDriveChoice->add("None / ragdoll");
    m_propDriveChoice->add("Animated");
    m_propDriveChoice->add("Driven");
    m_propDriveChoice->add("Free");
    new mxLabel(this, 205, 396, 80, 18, "Strength");
    m_driveStrength = new mxLineEdit(this, 285, 393, 100, 20, "0");
    new mxLabel(this, 205, 423, 80, 18, "Damping");
    m_driveDamping = new mxLineEdit(this, 285, 420, 100, 20, "0");
    new mxLabel(this, 205, 450, 80, 18, "Max force");
    m_driveMaxForce = new mxLineEdit(this, 285, 447, 100, 20, "0");

    new mxLabel(this, 465, 8, 180, 18, "Joints");
    m_jointList = new mxListBox(this, 465, 26, 240, 125, IDC_PHYS_JOINT_LIST);
    new mxButton(this, 465, 157, 56, 22, "Add", IDC_PHYS_JOINT_ADD);
    new mxButton(this, 525, 157, 56, 22, "Remove", IDC_PHYS_JOINT_REMOVE);
    new mxButton(this, 585, 157, 56, 22, "Apply", IDC_PHYS_JOINT_APPLY);

    new mxLabel(this, 465, 190, 65, 18, "Name");
    m_jointName = new mxLineEdit(this, 530, 187, 175, 20, "");
    new mxLabel(this, 465, 217, 65, 18, "Parent");
    m_jointParent = new mxChoice(this, 530, 214, 175, 22, IDC_PHYS_JOINT_PARENT);
    new mxLabel(this, 465, 244, 65, 18, "Child");
    m_jointChild = new mxChoice(this, 530, 241, 175, 22, IDC_PHYS_JOINT_CHILD);
    new mxLabel(this, 465, 271, 65, 18, "Swing Y/Z");
    m_swing1 = new mxLineEdit(this, 530, 268, 70, 20, "45");
    m_swing2 = new mxLineEdit(this, 605, 268, 70, 20, "45");
    new mxLabel(this, 465, 298, 65, 18, "Twist low/high");
    m_twistLow = new mxLineEdit(this, 545, 295, 60, 20, "-45");
    m_twistHigh = new mxLineEdit(this, 610, 295, 60, 20, "45");

    new mxLabel(this, 465, 323, 65, 18, "Joint type");
    m_jointKindChoice = new mxChoice(this, 530, 320, 175, 22);
    m_jointKindChoice->add("D6 / ragdoll");
    m_jointKindChoice->add("Hinge / prop");
    new mxButton(this, 465, 348, 94, 22, "Make Hinge", IDC_PHYS_MAKE_HINGE);
    new mxLabel(this, 465, 379, 65, 18, "Hinge min/max");
    m_hingeMin = new mxLineEdit(this, 545, 376, 60, 20, "0");
    m_hingeMax = new mxLineEdit(this, 610, 376, 60, 20, "90");
    new mxLabel(this, 465, 405, 70, 18, "Anchor XYZ");
    new mxLabel(this, 465, 431, 70, 18, "Axis XYZ");
    for(int i = 0; i < 3; ++i)
    {
        m_hingeAnchor[i] = new mxLineEdit(this, 535 + i * 56, 402, 52, 20, "0");
        m_hingeAxis[i] = new mxLineEdit(this, 535 + i * 56, 428, 52, 20, i == 0 ? "1" : "0");
    }
    new mxLabel(this, 465, 458, 78, 18, "Release dmg");
    m_releaseDamage = new mxLineEdit(this, 545, 455, 115, 20, "0");
    new mxLabel(this, 465, 485, 78, 18, "Break dmg");
    m_breakDamage = new mxLineEdit(this, 545, 482, 115, 20, "0");
    new mxLabel(this, 465, 512, 78, 18, "Break impulse");
    m_breakImpulse = new mxLineEdit(this, 545, 509, 115, 20, "0");

    new mxButton(this, 10, 562, 110, 24, "Auto Generate", IDC_PHYS_AUTOGENERATE);
    new mxButton(this, 125, 562, 75, 24, "Reload", IDC_PHYS_RELOAD);
    new mxButton(this, 205, 562, 75, 24, "Save", IDC_PHYS_SAVE);
    new mxButton(this, 285, 562, 85, 24, "Import _cp", IDC_PHYS_IMPORT_CP);
    new mxButton(this, 375, 562, 80, 24, "Clear Col", IDC_PHYS_CLEAR_COLLISION);
    m_simulateButton = new mxButton(this, 465, 562, 95, 24, "Simulate", IDC_PHYS_SIMULATE);
    new mxButton(this, 565, 562, 75, 24, "Reset", IDC_PHYS_RESET);

    new mxLabel(this, 10, 594, 82, 18, "Asset resize");
    m_geometryScale = new mxLineEdit(this, 95, 592, 70, 22, "1");
    new mxButton(this, 172, 592, 130, 23, "Scale .phys", IDC_PHYS_SCALE_ASSET);
    new mxLabel(this, 315, 594, 386, 18, "For permanently re-exported MDL only (NOT runtime scale)");
    m_status = new mxLabel(this, 10, 633, 695, 60, "Load a model to edit its .phys asset.");
}

int PhysicsAssetEditor::handleEvent(mxEvent *event)
{
    if(!event || event->event != mxEvent::Action) return 1;
    switch(event->action)
    {
    case IDC_PHYS_BODY_LIST:
    {
        const int requested = m_bodyList->getSelectedIndex();
        // Applying a body refreshes the joint list. Preserve both pending
        // panels BEFORE that refresh or selecting another body loses edits.
        if(m_selectedJoint >= 0 && !ApplyJointFields())
        {
            m_bodyList->select(m_selectedBody);
            break;
        }
        if(m_selectedBody >= 0 && requested != m_selectedBody && !ApplyBodyFields())
        {
            m_bodyList->select(m_selectedBody);
            break;
        }
        if(requested >= 0 && requested < (int)m_asset.bodies.size())
        {
            m_bodyList->select(requested);
            ShowBody(requested);
        }
        break;
    }
    case IDC_PHYS_JOINT_LIST:
    {
        const int requested = m_jointList->getSelectedIndex();
        if(m_selectedJoint >= 0 && requested != m_selectedJoint && !ApplyJointFields())
        {
            m_jointList->select(m_selectedJoint);
            break;
        }
        if(requested >= 0 && requested < (int)m_asset.joints.size())
        {
            m_jointList->select(requested);
            ShowJoint(requested);
        }
        break;
    }
    case IDC_PHYS_BODY_ADD:
        if((m_selectedJoint < 0 || ApplyJointFields()) &&
           (m_selectedBody < 0 || ApplyBodyFields())) AddBody();
        break;
    case IDC_PHYS_MAKE_PART:
        if((m_selectedJoint < 0 || ApplyJointFields()) &&
           (m_selectedBody < 0 || ApplyBodyFields())) MakeInteractivePart();
        break;
    case IDC_PHYS_MAKE_HINGE:
        if(m_selectedJoint < 0 || ApplyJointFields()) MakeHinge();
        break;
    case IDC_PHYS_BODY_REMOVE:
        if(m_selectedJoint < 0 || ApplyJointFields()) RemoveBody();
        break;
    case IDC_PHYS_BODY_APPLY:
        if(m_selectedJoint < 0 || ApplyJointFields()) ApplyBodyFields();
        break;
    case IDC_PHYS_JOINT_ADD:
        if((m_selectedJoint < 0 || ApplyJointFields()) &&
           (m_selectedBody < 0 || ApplyBodyFields())) AddJoint();
        break;
    case IDC_PHYS_JOINT_REMOVE: RemoveJoint(); break;
    case IDC_PHYS_JOINT_APPLY: ApplyJointFields(); break;
    case IDC_PHYS_AUTOGENERATE:
        if(m_selectedJoint >= 0 && !ApplyJointFields()) break;
        if(m_selectedBody >= 0 && !ApplyBodyFields()) break;
        if(!m_asset.bodies.empty() &&
           mxMessageBox(this, "Replace the current authored bodies and joints? This cannot be undone.",
               "Auto Generate .phys", MX_MB_YESNO | MX_MB_QUESTION) != 0) break;
        AutoGenerate();
        break;
    case IDC_PHYS_SAVE: SaveAsset(); break;
    case IDC_PHYS_RELOAD:
        if((m_dirty || m_selectedBody >= 0 || m_selectedJoint >= 0) &&
           mxMessageBox(this, "Reload discards unapplied fields and unsaved physics asset edits. Continue?", "Reload .phys", MX_MB_YESNO | MX_MB_QUESTION) != 0)
            break;
        ReloadAsset();
        break;
    case IDC_PHYS_IMPORT_CP: ImportLegacyCollisionProxy(); break;
    case IDC_PHYS_SCALE_ASSET:
        if(m_selectedJoint >= 0 && !ApplyJointFields()) break;
        if(m_selectedBody >= 0 && !ApplyBodyFields()) break;
        ScaleAuthoredGeometry();
        break;
    case IDC_PHYS_CLEAR_COLLISION: ClearCollisionGeometry(); break;
    case IDC_PHYS_SIMULATE:
        if(m_simulating) StopSimulation(); else StartSimulation();
        break;
    case IDC_PHYS_RESET:
        StopSimulation();
        if(g_GlWindow) g_GlWindow->redraw();
        break;
    case IDC_PHYS_SHAPE:
        if(m_selectedBody >= 0 && m_selectedBody < (int)m_asset.bodies.size())
        {
            // The shape selector changes before we receive the action event.
            // Apply the numeric inputs as the NEW type; never reset the other
            // body fields (name, mass, offset or drive parameters) silently.
            if(m_selectedJoint >= 0 && !ApplyJointFields()) break;
            const int requestedShape = m_shapeChoice->getSelectedIndex();
            const int required = requestedShape == 1 ? 3 : requestedShape == 0 ? 2 : 1;
            for(int axis = 0; axis < required; ++axis)
                if(!m_dimensions[axis]->getLabel() || !m_dimensions[axis]->getLabel()[0])
                    SetFloat(m_dimensions[axis], 1.0f);
            ApplyBodyFields();
        }
        break;
    }
    return 1;
}

void PhysicsAssetEditor::OnModelLoaded(const char *modelPath)
{
    StopSimulation();
    m_modelPath = modelPath ? modelPath : "";
    m_assetPath = physicsasset::BuildAssetPath(m_modelPath);
    PopulateBones();
    ReloadAsset();
}

void PhysicsAssetEditor::PopulateBones()
{
    m_boneChoice->removeAll();
    studiohdr_t *hdr = m_model.getStudioHeader();
    if(!hdr) return;
    auto *bones = reinterpret_cast<mstudiobone_t *>(reinterpret_cast<byte *>(hdr) + hdr->boneindex);
    for(int i = 0; i < hdr->numbones; ++i) m_boneChoice->add(bones[i].name);
}

void PhysicsAssetEditor::RefreshBodyList(int select)
{
    m_bodyList->removeAll();
    for(const auto &body : m_asset.bodies)
        m_bodyList->add(va("%s [%s]%s", body.name.c_str(), body.bone.c_str(), body.part.empty() ? "" : " [PART]"));
    if(select < 0 && !m_asset.bodies.empty()) select = std::min(m_selectedBody, (int)m_asset.bodies.size() - 1);
    if(select >= 0 && select < (int)m_asset.bodies.size())
    {
        m_bodyList->select(select);
        ShowBody(select);
    }
    else m_selectedBody = -1;
    RefreshBodySelectors();
}

void PhysicsAssetEditor::RefreshJointList(int select)
{
    m_jointList->removeAll();
    for(const auto &joint : m_asset.joints)
        m_jointList->add(va("%s%s: %s -> %s", joint.kind == physicsasset::JointKind::Hinge ? "[H] " : "", joint.name.c_str(), joint.parent.c_str(), joint.child.c_str()));
    if(select < 0 && !m_asset.joints.empty()) select = std::min(m_selectedJoint, (int)m_asset.joints.size() - 1);
    if(select >= 0 && select < (int)m_asset.joints.size())
    {
        m_jointList->select(select);
        ShowJoint(select);
    }
    else m_selectedJoint = -1;
}

void PhysicsAssetEditor::RefreshBodySelectors()
{
    m_jointParent->removeAll(); m_jointChild->removeAll();
    for(const auto &body : m_asset.bodies)
    {
        m_jointParent->add(body.name.c_str());
        m_jointChild->add(body.name.c_str());
    }
}

void PhysicsAssetEditor::ShowBody(int index)
{
    if(index < 0 || index >= (int)m_asset.bodies.size()) return;
    m_selectedBody = index;
    const auto &body = m_asset.bodies[index];
    m_bodyName->setLabel(body.name.c_str());
    const int bone = FindBone(body.bone.c_str());
    if(bone >= 0) m_boneChoice->select(bone);

    int shape = 0;
    if(body.shape.type == physicsasset::ShapeType::Box) shape = 1;
    else if(body.shape.type == physicsasset::ShapeType::Sphere) shape = 2;
    m_shapeChoice->select(shape);

    SetFloat(m_offset[0], body.offset.x); SetFloat(m_offset[1], body.offset.y); SetFloat(m_offset[2], body.offset.z);
    SetFloat(m_angles[0], body.angles.x); SetFloat(m_angles[1], body.angles.y); SetFloat(m_angles[2], body.angles.z);
    if(shape == 0)
    {
        m_dimensionLabels[0]->setLabel("R"); m_dimensionLabels[1]->setLabel("HH"); m_dimensionLabels[2]->setLabel("-");
        SetFloat(m_dimensions[0], body.shape.radius); SetFloat(m_dimensions[1], body.shape.halfHeight); m_dimensions[2]->setLabel("");
    }
    else if(shape == 1)
    {
        m_dimensionLabels[0]->setLabel("HX"); m_dimensionLabels[1]->setLabel("HY"); m_dimensionLabels[2]->setLabel("HZ");
        SetFloat(m_dimensions[0], body.shape.halfExtents.x); SetFloat(m_dimensions[1], body.shape.halfExtents.y); SetFloat(m_dimensions[2], body.shape.halfExtents.z);
    }
    else
    {
        m_dimensionLabels[0]->setLabel("R"); m_dimensionLabels[1]->setLabel("-"); m_dimensionLabels[2]->setLabel("-");
        SetFloat(m_dimensions[0], body.shape.radius); m_dimensions[1]->setLabel(""); m_dimensions[2]->setLabel("");
    }
    SetFloat(m_mass, body.mass);
    SetFloat(m_linearDamping, body.linearDamping);
    SetFloat(m_angularDamping, body.angularDamping);
    if(m_hitGroup) m_hitGroup->setLabel(va("%d", body.hitGroup));
    m_partName->setLabel(body.part.c_str());
    m_propDriveChoice->select(body.propDrive == physicsasset::PropDriveMode::Animated ? 1 :
        body.propDrive == physicsasset::PropDriveMode::Driven ? 2 :
        body.propDrive == physicsasset::PropDriveMode::Free ? 3 : 0);
    SetFloat(m_driveStrength, body.driveStrength);
    SetFloat(m_driveDamping, body.driveDamping);
    SetFloat(m_driveMaxForce, body.driveMaxForce);
    if(g_GlWindow) g_GlWindow->redraw();
}

void PhysicsAssetEditor::ShowJoint(int index)
{
    if(index < 0 || index >= (int)m_asset.joints.size()) return;
    m_selectedJoint = index;
    const auto &joint = m_asset.joints[index];
    m_jointName->setLabel(joint.name.c_str());
    const int parent = physicsasset::FindBody(m_asset, joint.parent.c_str());
    const int child = physicsasset::FindBody(m_asset, joint.child.c_str());
    if(parent >= 0) m_jointParent->select(parent);
    if(child >= 0) m_jointChild->select(child);
    SetFloat(m_swing1, joint.swing1); SetFloat(m_swing2, joint.swing2);
    SetFloat(m_twistLow, joint.twistLow); SetFloat(m_twistHigh, joint.twistHigh);
    m_jointKindChoice->select(joint.kind == physicsasset::JointKind::Hinge ? 1 : 0);
    SetFloat(m_hingeMin, joint.hingeMin); SetFloat(m_hingeMax, joint.hingeMax);
    SetFloat(m_hingeAnchor[0], joint.hingeAnchor.x); SetFloat(m_hingeAnchor[1], joint.hingeAnchor.y); SetFloat(m_hingeAnchor[2], joint.hingeAnchor.z);
    SetFloat(m_hingeAxis[0], joint.hingeAxis.x); SetFloat(m_hingeAxis[1], joint.hingeAxis.y); SetFloat(m_hingeAxis[2], joint.hingeAxis.z);
    SetFloat(m_releaseDamage, joint.releaseDamage); SetFloat(m_breakDamage, joint.breakDamage);
    SetFloat(m_breakImpulse, joint.breakImpulse);
}

bool PhysicsAssetEditor::ApplyBodyFields()
{
    if(m_selectedBody < 0 || m_selectedBody >= (int)m_asset.bodies.size()) return false;
    StopSimulation();
    const std::string before = physicsasset::Serialize(m_asset);
    physicsasset::BodyDef body = m_asset.bodies[m_selectedBody];
    const std::string oldName = body.name;
    float strength, damping, force;
    if(!ReadStrictFloat(m_driveStrength, strength) || !ReadStrictFloat(m_driveDamping, damping) ||
       !ReadStrictFloat(m_driveMaxForce, force) || strength < 0 || damping < 0 || force < 0)
    {
        UpdateStatus("Invalid interactive drive parameters (use finite, non-negative numbers).");
        return false;
    }
    body.name = m_bodyName->getLabel() ? m_bodyName->getLabel() : "";
    const int bone = m_boneChoice->getSelectedIndex();
    studiohdr_t *hdr = m_model.getStudioHeader();
    if(hdr && bone >= 0 && bone < hdr->numbones)
    {
        auto *bones = reinterpret_cast<mstudiobone_t *>(reinterpret_cast<byte *>(hdr) + hdr->boneindex);
        body.bone = bones[bone].name;
    }
    const int shape = m_shapeChoice->getSelectedIndex();
    body.shape.type = shape == 1 ? physicsasset::ShapeType::Box : shape == 2 ? physicsasset::ShapeType::Sphere : physicsasset::ShapeType::Capsule;
    float offset[3], angles[3], dimension[3]{}, mass, linearDamping, angularDamping;
    int hitGroup = 0;
    for(int axis = 0; axis < 3; ++axis)
        if(!ReadStrictFloat(m_offset[axis], offset[axis]) || !ReadStrictFloat(m_angles[axis], angles[axis]))
        {
            UpdateStatus("Offsets/angles must contain finite numbers; body not applied.");
            return false;
        }
    const int requiredDimensions = body.shape.type == physicsasset::ShapeType::Box ? 3 :
        body.shape.type == physicsasset::ShapeType::Capsule ? 2 : 1;
    for(int axis = 0; axis < requiredDimensions; ++axis)
        if(!ReadStrictFloat(m_dimensions[axis], dimension[axis]) ||
           dimension[axis] < 0.0f ||
           (dimension[axis] == 0.0f &&
            !(body.shape.type == physicsasset::ShapeType::Capsule && axis == 1)))
        {
            UpdateStatus("Shape dimensions must be positive (capsule half-height may be zero).");
            return false;
        }
    if(!ReadStrictFloat(m_mass, mass) || mass <= 0.0f ||
       !ReadStrictFloat(m_linearDamping, linearDamping) || linearDamping < 0.0f ||
       !ReadStrictFloat(m_angularDamping, angularDamping) || angularDamping < 0.0f ||
       !ReadStrictInt(m_hitGroup, hitGroup) || hitGroup < 0 || hitGroup > 255)
    {
        UpdateStatus("Mass must be positive; damping non-negative; hitgroup 0..255.");
        return false;
    }
    body.offset = { offset[0], offset[1], offset[2] };
    body.angles = { angles[0], angles[1], angles[2] };
    if(body.shape.type == physicsasset::ShapeType::Box)
        body.shape.halfExtents = { dimension[0], dimension[1], dimension[2] };
    else
    {
        body.shape.radius = dimension[0];
        if(body.shape.type == physicsasset::ShapeType::Capsule)
            body.shape.halfHeight = dimension[1];
    }
    body.mass = mass;
    body.linearDamping = linearDamping;
    body.angularDamping = angularDamping;
    body.hitGroup = hitGroup;
    const int mode = m_propDriveChoice->getSelectedIndex();
    body.propDrive = mode == 1 ? physicsasset::PropDriveMode::Animated :
        mode == 2 ? physicsasset::PropDriveMode::Driven :
        mode == 3 ? physicsasset::PropDriveMode::Free : physicsasset::PropDriveMode::None;
    body.part = m_partName->getLabel() ? m_partName->getLabel() : "";
    body.driveStrength = strength;
    body.driveDamping = damping;
    body.driveMaxForce = force;
    if(body.propDrive == physicsasset::PropDriveMode::Driven && strength <= 0.0f)
    {
        UpdateStatus("Driven mode requires positive drive strength.");
        return false;
    }
    if(body.propDrive != physicsasset::PropDriveMode::None && body.part.empty())
    {
        UpdateStatus("Set a stable part name for an interactive body.");
        return false;
    }
    m_asset.bodies[m_selectedBody] = body;
    if(body.propDrive != physicsasset::PropDriveMode::None || !body.part.empty() || strength || damping || force)
        m_asset.version = physicsasset::kCurrentVersion;

    if(oldName != body.name)
    {
        for(auto &joint : m_asset.joints)
        {
            if(joint.parent == oldName) joint.parent = body.name;
            if(joint.child == oldName) joint.child = body.name;
        }
    }
    if(physicsasset::Serialize(m_asset) != before) MarkDirty();
    RefreshBodyList(m_selectedBody); RefreshJointList(m_selectedJoint);
    return true;
}

bool PhysicsAssetEditor::ApplyJointFields()
{
    if(m_selectedJoint < 0 || m_selectedJoint >= (int)m_asset.joints.size()) return false;
    StopSimulation();
    const std::string before = physicsasset::Serialize(m_asset);
    physicsasset::JointDef joint = m_asset.joints[m_selectedJoint];
    float hmin, hmax, release, damage, impulse, anchor[3], axis[3];
    if(!ReadStrictFloat(m_hingeMin, hmin) || !ReadStrictFloat(m_hingeMax, hmax) ||
       !ReadStrictFloat(m_releaseDamage, release) || !ReadStrictFloat(m_breakDamage, damage) ||
       !ReadStrictFloat(m_breakImpulse, impulse) || release < 0 || damage < 0 || impulse < 0)
    {
        UpdateStatus("Invalid hinge limits or damage thresholds.");
        return false;
    }
    for(int i = 0; i < 3; ++i)
    {
        if(!ReadStrictFloat(m_hingeAnchor[i], anchor[i]) || !ReadStrictFloat(m_hingeAxis[i], axis[i]))
        {
            UpdateStatus("Hinge anchor and axis require finite XYZ values.");
            return false;
        }
    }
    joint.name = m_jointName->getLabel() ? m_jointName->getLabel() : "";
    const int parent = m_jointParent->getSelectedIndex();
    const int child = m_jointChild->getSelectedIndex();
    if(parent >= 0 && parent < (int)m_asset.bodies.size()) joint.parent = m_asset.bodies[parent].name;
    if(child >= 0 && child < (int)m_asset.bodies.size()) joint.child = m_asset.bodies[child].name;
    if(!ReadStrictFloat(m_swing1, joint.swing1) || !ReadStrictFloat(m_swing2, joint.swing2) ||
       !ReadStrictFloat(m_twistLow, joint.twistLow) || !ReadStrictFloat(m_twistHigh, joint.twistHigh) ||
       joint.swing1 <= 0.0f || joint.swing2 <= 0.0f || joint.twistLow >= joint.twistHigh)
    {
        UpdateStatus("D6 limits must be finite, positive swings and twist low < high.");
        return false;
    }
    joint.kind = m_jointKindChoice->getSelectedIndex() == 1 ? physicsasset::JointKind::Hinge : physicsasset::JointKind::D6;
    joint.hingeMin = hmin; joint.hingeMax = hmax;
    joint.hingeAnchor = { anchor[0], anchor[1], anchor[2] };
    joint.hingeAxis = { axis[0], axis[1], axis[2] };
    joint.releaseDamage = release; joint.breakDamage = damage; joint.breakImpulse = impulse;
    const double axisSq = double(axis[0]) * axis[0] + double(axis[1]) * axis[1] + double(axis[2]) * axis[2];
    if(joint.kind == physicsasset::JointKind::Hinge &&
       (hmin < -180.0f || hmax > 180.0f || hmin >= hmax || !(axisSq > 1e-12 && axisSq < 1e12)))
    {
        UpdateStatus("Hinge needs -180 <= min < max <= 180 and a nonzero finite axis.");
        return false;
    }
    if(joint.kind == physicsasset::JointKind::D6 && (release > 0 || damage > 0))
    {
        UpdateStatus("Release/break damage thresholds require a hinge joint.");
        return false;
    }
    m_asset.joints[m_selectedJoint] = joint;
    if(joint.kind == physicsasset::JointKind::Hinge || release || damage || impulse)
        m_asset.version = physicsasset::kCurrentVersion;
    if(physicsasset::Serialize(m_asset) != before) MarkDirty();
    RefreshJointList(m_selectedJoint);
    return true;
}

void PhysicsAssetEditor::MakeInteractivePart()
{
    if(m_selectedBody < 0 || m_selectedBody >= (int)m_asset.bodies.size()) return;
    StopSimulation();
    auto &body = m_asset.bodies[m_selectedBody];
    if(body.part.empty()) body.part = body.name;
    body.propDrive = physicsasset::PropDriveMode::Driven;
    if(body.driveStrength <= 0.0f) body.driveStrength = 120.0f;
    if(body.driveDamping <= 0.0f) body.driveDamping = 15.0f;
    if(body.driveMaxForce <= 0.0f) body.driveMaxForce = 850.0f;
    m_asset.version = physicsasset::kCurrentVersion;
    MarkDirty(); RefreshBodyList(m_selectedBody);
    UpdateStatus("Interactive part prepared. Add/select its joint, then use Make Hinge; verify pivot/axis and collision in the model.");
}

void PhysicsAssetEditor::MakeHinge()
{
    if(m_selectedJoint < 0 || m_selectedJoint >= (int)m_asset.joints.size()) return;
    StopSimulation();
    auto &joint = m_asset.joints[m_selectedJoint];
    joint.kind = physicsasset::JointKind::Hinge;
    joint.hingeMin = -5.0f;
    joint.hingeMax = 75.0f;
    joint.hingeAnchor = {};
    joint.hingeAxis = { 0.0f, 0.0f, 1.0f };
    m_asset.version = physicsasset::kCurrentVersion;
    MarkDirty(); RefreshJointList(m_selectedJoint);
    UpdateStatus("Hinge template set. Anchor/axis are CHILD-bone local; change for your actual model before saving.");
}

void PhysicsAssetEditor::AddBody()
{
    StopSimulation();
    studiohdr_t *hdr = m_model.getStudioHeader();
    if(!hdr || hdr->numbones <= 0) return;
    int boneIndex = m_boneChoice->getSelectedIndex();
    if(boneIndex < 0) boneIndex = 0;
    auto *bones = reinterpret_cast<mstudiobone_t *>(reinterpret_cast<byte *>(hdr) + hdr->boneindex);
    physicsasset::BodyDef body;
    body.name = bones[boneIndex].name;
    body.bone = bones[boneIndex].name;
    int suffix = 2;
    const std::string base = body.name;
    while(physicsasset::FindBody(m_asset, body.name.c_str()) >= 0) body.name = base + "_" + std::to_string(suffix++);
    m_asset.bodies.push_back(body);
    MarkDirty(); RefreshBodyList((int)m_asset.bodies.size() - 1); RefreshJointList();
}

void PhysicsAssetEditor::RemoveBody()
{
    if(m_selectedBody < 0 || m_selectedBody >= (int)m_asset.bodies.size()) return;
    StopSimulation();
    const std::string name = m_asset.bodies[m_selectedBody].name;
    m_asset.bodies.erase(m_asset.bodies.begin() + m_selectedBody);
    m_asset.joints.erase(std::remove_if(m_asset.joints.begin(), m_asset.joints.end(), [&](const physicsasset::JointDef &j){ return j.parent == name || j.child == name; }), m_asset.joints.end());
    MarkDirty(); RefreshBodyList(std::min(m_selectedBody, (int)m_asset.bodies.size() - 1)); RefreshJointList();
}

void PhysicsAssetEditor::AddJoint()
{
    if(m_asset.bodies.size() < 2) return;
    StopSimulation();
    physicsasset::JointDef joint;
    int child = m_selectedBody >= 0 ? m_selectedBody : 1;
    int parent = FindNearestParentBody(FindBone(m_asset.bodies[child].bone.c_str()));
    if(parent < 0 || parent == child) parent = child == 0 ? 1 : 0;
    joint.parent = m_asset.bodies[parent].name;
    joint.child = m_asset.bodies[child].name;
    joint.name = joint.parent + "_to_" + joint.child;
    if(m_asset.bodies[child].propDrive != physicsasset::PropDriveMode::None)
    {
        joint.kind = physicsasset::JointKind::Hinge;
        joint.hingeMin = -5.0f;
        joint.hingeMax = 75.0f;
        joint.hingeAxis = { 0.0f, 0.0f, 1.0f };
        m_asset.version = physicsasset::kCurrentVersion;
    }
    m_asset.joints.push_back(joint);
    MarkDirty(); RefreshJointList((int)m_asset.joints.size() - 1);
}

void PhysicsAssetEditor::RemoveJoint()
{
    if(m_selectedJoint < 0 || m_selectedJoint >= (int)m_asset.joints.size()) return;
    StopSimulation();
    m_asset.joints.erase(m_asset.joints.begin() + m_selectedJoint);
    MarkDirty(); RefreshJointList(std::min(m_selectedJoint, (int)m_asset.joints.size() - 1));
}

int PhysicsAssetEditor::FindBone(const char *name) const
{
    studiohdr_t *hdr = m_model.getStudioHeader();
    if(!hdr || !name) return -1;
    auto *bones = reinterpret_cast<mstudiobone_t *>(reinterpret_cast<byte *>(hdr) + hdr->boneindex);
    for(int i = 0; i < hdr->numbones; ++i) if(!Q_stricmp(bones[i].name, name)) return i;
    return -1;
}

int PhysicsAssetEditor::FindBoneBody(int boneIndex) const
{
    studiohdr_t *hdr = m_model.getStudioHeader();
    if(!hdr || boneIndex < 0 || boneIndex >= hdr->numbones) return -1;
    auto *bones = reinterpret_cast<mstudiobone_t *>(reinterpret_cast<byte *>(hdr) + hdr->boneindex);
    for(size_t i = 0; i < m_asset.bodies.size(); ++i)
        if(!Q_stricmp(m_asset.bodies[i].bone.c_str(), bones[boneIndex].name)) return (int)i;
    return -1;
}

int PhysicsAssetEditor::FindNearestParentBody(int boneIndex) const
{
    studiohdr_t *hdr = m_model.getStudioHeader();
    if(!hdr || boneIndex < 0) return -1;
    auto *bones = reinterpret_cast<mstudiobone_t *>(reinterpret_cast<byte *>(hdr) + hdr->boneindex);
    for(int parent = bones[boneIndex].parent; parent >= 0; parent = bones[parent].parent)
    {
        const int body = FindBoneBody(parent);
        if(body >= 0) return body;
    }
    return -1;
}

void PhysicsAssetEditor::AutoGenerate()
{
    // Auto-generation replaces bodies and joints. Never silently discard
    // manually authored interactive parts or their breakable hinges.
    for(const auto &body : m_asset.bodies)
        if(body.propDrive != physicsasset::PropDriveMode::None || !body.part.empty())
        {
            UpdateStatus("Auto Generate blocked: this asset has interactive parts. Back up/remove them explicitly before regenerating.");
            return;
        }
    for(const auto &joint : m_asset.joints)
        if(joint.kind == physicsasset::JointKind::Hinge)
        {
            UpdateStatus("Auto Generate blocked: authored interactive hinges must be removed explicitly before regenerating.");
            return;
        }
    StopSimulation();
    studiohdr_t *hdr = m_model.getStudioHeader();
    if(!hdr) return;
    auto *bones = reinterpret_cast<mstudiobone_t *>(reinterpret_cast<byte *>(hdr) + hdr->boneindex);

    physicsasset::AssetDef generated;
    generated.sourcePath = m_assetPath;
    generated.collisions = m_asset.collisions;
    std::vector<int> boneToBody(hdr->numbones, -1);

    for(int i = 0; i < hdr->numbones; ++i)
    {
        if(ShouldSkipAutoBone(bones[i].name)) continue;
        int bestChild = -1;
        float bestLength = 0.0f;
        Vector childOffset;
        for(int c = 0; c < hdr->numbones; ++c)
        {
            if(bones[c].parent != i) continue;
            const Vector offset(bones[c].value[0], bones[c].value[1], bones[c].value[2]);
            const float length = offset.Length();
            if(length > bestLength) { bestLength = length; bestChild = c; childOffset = offset; }
        }

        physicsasset::BodyDef body;
        body.name = bones[i].name;
        body.bone = bones[i].name;
        body.hitGroup = SuggestedHitGroup(bones[i].name);

        // Prefer geometry-aware fitting. Vertices are assigned to the bone with
        // the strongest skin weight and transformed back into that bone's local
        // space. This creates a much better first pass for torso/pelvis/limbs
        // than estimating every body from bone length alone.
        Vector vertexMins, vertexMaxs;
        const bool haveGeometry = m_model.GetBoneVertexBounds(i, vertexMins, vertexMaxs);
        if(haveGeometry)
        {
            const Vector center = (vertexMins + vertexMaxs) * 0.5f;
            Vector half = (vertexMaxs - vertexMins) * 0.5f;
            half.x = std::max(half.x, 0.25f);
            half.y = std::max(half.y, 0.25f);
            half.z = std::max(half.z, 0.25f);
            body.offset = ToAssetVec(center);

            const std::string lowerName = Lower(bones[i].name);
            const bool torso = lowerName.find("pelvis") != std::string::npos || lowerName.find("hip") != std::string::npos ||
                lowerName.find("spine") != std::string::npos || lowerName.find("chest") != std::string::npos;
            const bool head = lowerName.find("head") != std::string::npos;

            if(torso)
            {
                body.shape.type = physicsasset::ShapeType::Box;
                body.shape.halfExtents = ToAssetVec(half);
            }
            else if(head)
            {
                body.shape.type = physicsasset::ShapeType::Sphere;
                body.shape.radius = std::max({ half.x, half.y, half.z });
            }
            else
            {
                int axis = 0;
                if(half.y > half.x) axis = 1;
                if(half.z > half[axis]) axis = 2;
                Vector direction(0, 0, 0); direction[axis] = 1.0f;
                matrix3x3 basis; basis.FromVector(direction);
                body.angles = ToAssetVec(basis.GetAngles());
                const float axisHalf = half[axis];
                const float radius = axis == 0 ? std::max(half.y, half.z) : axis == 1 ? std::max(half.x, half.z) : std::max(half.x, half.y);
                body.shape.type = physicsasset::ShapeType::Capsule;
                body.shape.radius = std::max(radius, 0.5f);
                body.shape.halfHeight = std::max(axisHalf - body.shape.radius, 0.1f);
            }
        }
        else if(bestChild >= 0 && bestLength >= 2.0f)
        {
            const Vector dir = childOffset.Normalize();
            matrix3x3 basis;
            basis.FromVector(dir);
            const Vector angles = basis.GetAngles();
            body.shape.type = physicsasset::ShapeType::Capsule;
            body.shape.radius = std::clamp(bestLength * 0.18f, 1.5f, 6.0f);
            body.shape.halfHeight = std::max(bestLength * 0.5f - body.shape.radius, 0.25f);
            body.offset = ToAssetVec(childOffset * 0.5f);
            body.angles = ToAssetVec(angles);
        }
        else if(IsLeafPhysicsBone(bones[i].name))
        {
            const float parentLength = Vector(bones[i].value[0], bones[i].value[1], bones[i].value[2]).Length();
            body.shape.type = physicsasset::ShapeType::Sphere;
            body.shape.radius = std::clamp(parentLength * 0.28f, 2.0f, 6.0f);
        }
        else continue;

        boneToBody[i] = (int)generated.bodies.size();
        generated.bodies.push_back(body);
    }

    for(int i = 0; i < hdr->numbones; ++i)
    {
        const int childBody = boneToBody[i];
        if(childBody < 0) continue;
        int parentBody = -1;
        for(int parent = bones[i].parent; parent >= 0; parent = bones[parent].parent)
        {
            if(boneToBody[parent] >= 0) { parentBody = boneToBody[parent]; break; }
        }
        if(parentBody < 0) continue;
        physicsasset::JointDef joint;
        joint.parent = generated.bodies[parentBody].name;
        joint.child = generated.bodies[childBody].name;
        joint.name = joint.parent + "_to_" + joint.child;
        generated.joints.push_back(joint);
    }

    float totalVolume = 0.0f;
    for(const auto &body : generated.bodies) totalVolume += ShapeVolume(body);
    if(totalVolume > 0.0f)
        for(auto &body : generated.bodies) body.mass = std::max(0.2f, 70.0f * ShapeVolume(body) / totalVolume);

    if(generated.bodies.empty())
    {
        mxMessageBox(this, "Could not generate physics bodies from this skeleton.", APP_TITLE_STR, MX_MB_OK | MX_MB_ERROR);
        return;
    }
    m_asset = std::move(generated);
    MarkDirty(); RefreshBodyList(0); RefreshJointList(0);
    UpdateStatus(va("Generated %d bodies and %d joints; preserved %d collision sections. Review then Save.", (int)m_asset.bodies.size(), (int)m_asset.joints.size(), (int)m_asset.collisions.size()));
}

void PhysicsAssetEditor::ImportLegacyCollisionProxy()
{
    if(m_modelPath.empty())
    {
        UpdateStatus("Load the render model before importing a legacy collision proxy.");
        return;
    }
    StopSimulation();
    const std::string proxyPath = LegacyCollisionProxyPath(m_modelPath);
    std::vector<physicsasset::CollisionDef> collisions;
    std::string error;
    if(!ExtractLegacyCollisionProxy(proxyPath.c_str(), m_model.getStudioHeader(), collisions, error))
    {
        mxMessageBox(this, error.c_str(), "Import _cp.mdl", MX_MB_OK | MX_MB_ERROR);
        return;
    }
    m_asset.version = physicsasset::kCurrentVersion;
    m_asset.collisions = std::move(collisions);
    MarkDirty();
    int boneBound = 0;
    for(const auto &collision : m_asset.collisions) if(!collision.bone.empty()) ++boneBound;
    UpdateStatus(va("Imported %d collision sections (%d bone-bound) from %s. Save the .phys, then _cp.mdl is no longer needed at runtime.",
        (int)m_asset.collisions.size(), boneBound, proxyPath.c_str()));
}

void PhysicsAssetEditor::ScaleAuthoredGeometry()
{
    float factor = 0.0f;
    if(!ReadStrictFloat(m_geometryScale, factor) || factor <= 0.0f)
    {
        UpdateStatus("Scale .phys requires a positive finite multiplier.");
        return;
    }
    if(factor == 1.0f) return;
    if(m_asset.bodies.empty() && m_asset.collisions.empty())
    {
        UpdateStatus("No authored .phys geometry to resize.");
        return;
    }
    if(mxMessageBox(this,
        "Permanently resize the authored .phys geometry and joint pivots? Only use this when the underlying MDL has ALSO been permanently rescaled. Runtime entity scale already scales physics automatically. Continue?",
        "Scale authored .phys", MX_MB_YESNO | MX_MB_QUESTION) != 0) return;
    StopSimulation();
    physicsasset::AssetDef resized = m_asset;
    std::string error;
    if(!physicsasset::ScaleAuthoredAsset(resized, factor) ||
       !physicsasset::Validate(resized, &error))
    {
        UpdateStatus("Scale rejected: geometry overflow or invalid .phys output.");
        return;
    }
    m_asset = std::move(resized);
    MarkDirty();
    RefreshBodyList(m_selectedBody);
    RefreshJointList(m_selectedJoint);
    UpdateStatus("Authored .phys resized; verify visual MDL alignment, then Save.");
}

void PhysicsAssetEditor::ClearCollisionGeometry()
{
    if(m_asset.collisions.empty()) return;
    StopSimulation();
    m_asset.collisions.clear();
    MarkDirty();
    if(m_asset.bodies.empty() && m_asset.joints.empty())
        UpdateStatus("Collision cleared. Save will ask to DELETE the now-empty .phys sidecar; a remaining _cp.mdl may still provide legacy collision.");
    else
        UpdateStatus("Collision cleared. Save will preserve the remaining authored physics bodies and joints.");
}

bool PhysicsAssetEditor::Validate(std::string &error) const
{
    if(m_modelPath.empty()) { error = "No model loaded."; return false; }
    if(!physicsasset::Validate(m_asset, &error)) return false;

    // The shared validator owns all format/graph/numeric rules. pxmv adds the
    // model-specific rule that every referenced bone must actually exist in the
    // currently loaded MDL.
    for(const auto &body : m_asset.bodies)
    {
        if(FindBone(body.bone.c_str()) < 0)
        {
            error = "Missing model bone: " + body.bone;
            return false;
        }
    }

    studiohdr_t *hdr = m_model.getStudioHeader();
    if(!hdr && !m_asset.collisions.empty())
    {
        error = "Cannot validate collision bodygroups without the render model.";
        return false;
    }
    for(const auto &collision : m_asset.collisions)
    {
        if(!collision.bone.empty() && FindBone(collision.bone.c_str()) < 0)
        {
            error = "Collision '" + collision.name + "' references missing model bone '" + collision.bone + "'";
            return false;
        }
        if(collision.bodyGroup.empty()) continue;
        auto *parts = reinterpret_cast<mstudiobodyparts_t *>(reinterpret_cast<byte *>(hdr) + hdr->bodypartindex);
        mstudiobodyparts_t *found = nullptr;
        for(int i = 0; i < hdr->numbodyparts; ++i)
            if(!Q_stricmp(parts[i].name, collision.bodyGroup.c_str())) { found = &parts[i]; break; }
        if(!found || collision.variant < 0 || collision.variant >= found->nummodels)
        {
            error = "Collision '" + collision.name + "' references invalid render bodygroup '" + collision.bodyGroup + "' variant " + std::to_string(collision.variant);
            return false;
        }
    }
    // An interactive hinged door needs one joint connected to its physical
    // body. Other free-standing/static props are still accepted by the shared
    // v4 parser; only the pxmv interactive authoring workflow adds this check.
    for(const auto &body : m_asset.bodies)
    {
        if(body.propDrive != physicsasset::PropDriveMode::Driven &&
           body.propDrive != physicsasset::PropDriveMode::Free) continue;
        bool hasHinge = false;
        for(const auto &joint : m_asset.joints)
            if(joint.kind == physicsasset::JointKind::Hinge && !Q_stricmp(joint.child.c_str(), body.name.c_str()))
            {
                hasHinge = true;
                break;
            }
        if(!hasHinge)
        {
            error = "Interactive part '" + body.name + "' needs a child-side hinge joint in pxmv.";
            return false;
        }
    }
    return true;
}

bool PhysicsAssetEditor::SaveAsset()
{
    // Joint Apply refreshes only the joint list, whereas Body Apply refreshes
    // both lists. Apply the joint first or unsaved joint edits are lost.
    if(m_selectedJoint >= 0 && !ApplyJointFields()) return false;
    if(m_selectedBody >= 0 && !ApplyBodyFields()) return false;
    std::string error;
    // Empty .phys documents are invalid by design. Saving an empty asset
    // explicitly removes its sidecar, rather than trying to serialize it and
    // leaving the previously saved collision active on disk.
    if(m_asset.bodies.empty() && m_asset.joints.empty() && m_asset.collisions.empty())
    {
        if(m_modelPath.empty() || m_assetPath.empty())
        {
            mxMessageBox(this, "Load a render model before removing a .phys sidecar.",
                "Physics Asset save failed", MX_MB_OK | MX_MB_ERROR);
            return false;
        }
        if(mxMessageBox(this,
            "The physics asset is empty. Save cannot write an empty .phys.\n\n"
            "Remove its .phys sidecar from disk instead? This cannot be undone.\n"
            "Legacy model collision will be used after a reload; an existing _cp.mdl may still provide collision.",
            "Remove empty .phys sidecar", MX_MB_YESNO | MX_MB_QUESTION | MX_MB_WARNING) != 0)
        {
            UpdateStatus("Empty .phys removal cancelled; on-disk asset unchanged.");
            return false;
        }
        StopSimulation();
        bool removed = false;
        if(!physicsasset::RemoveEmptyAssetSidecar(m_assetPath, m_asset, removed, &error))
        {
            mxMessageBox(this, error.c_str(), "Physics Asset removal failed", MX_MB_OK | MX_MB_ERROR);
            UpdateStatus("Could not remove .phys sidecar; on-disk asset may still contain old collision.");
            return false;
        }
        m_dirty = false;
        RefreshBodyList();
        RefreshJointList();
        if(removed)
            UpdateStatus("Removed empty .phys sidecar. Reload model/map to refresh collision; _cp.mdl may still apply.");
        else
            UpdateStatus("No .phys sidecar exists. Legacy model/_cp collision fallback remains available.");
        return true;
    }
    if(!Validate(error))
    {
        mxMessageBox(this, error.c_str(), "Physics Asset validation", MX_MB_OK | MX_MB_ERROR);
        return false;
    }
    m_asset.sourcePath = m_assetPath;
    if(!physicsasset::SaveFile(m_assetPath.c_str(), m_asset, &error))
    {
        mxMessageBox(this, error.c_str(), "Physics Asset save failed", MX_MB_OK | MX_MB_ERROR);
        return false;
    }
    m_dirty = false;
    const int interactiveCount = (int)std::count_if(m_asset.bodies.begin(), m_asset.bodies.end(),
        [](const physicsasset::BodyDef &body) { return body.propDrive != physicsasset::PropDriveMode::None; });
    if(interactiveCount > 8)
        UpdateStatus(va("Saved %s. WARNING: I.8 savegames persist at most 8 parts per entity!", m_assetPath.c_str()));
    else UpdateStatus(va("Saved %s", m_assetPath.c_str()));
    return true;
}

void PhysicsAssetEditor::ReloadAsset()
{
    StopSimulation();
    m_asset = physicsasset::AssetDef{};
    m_asset.sourcePath = m_assetPath;
    if(m_assetPath.empty())
    {
        RefreshBodyList(); RefreshJointList(); UpdateStatus("Load a model to edit its .phys asset."); return;
    }

    std::ifstream exists(m_assetPath, std::ios::binary);
    if(exists.good())
    {
        exists.close();
        std::string error;
        if(!physicsasset::LoadFile(m_assetPath.c_str(), m_asset, &error))
        {
            mxMessageBox(this, error.c_str(), "Physics Asset parse error", MX_MB_OK | MX_MB_ERROR);
            m_asset = physicsasset::AssetDef{}; m_asset.sourcePath = m_assetPath;
        }
        else UpdateStatus(va("Loaded %s (%d collision sections, %d bodies, %d joints)", m_assetPath.c_str(), (int)m_asset.collisions.size(), (int)m_asset.bodies.size(), (int)m_asset.joints.size()));
    }
    else UpdateStatus("No .phys file yet. Use Import _cp for legacy prop collision, or add/auto-generate ragdoll bodies.");
    m_dirty = false;
    RefreshBodyList(m_asset.bodies.empty() ? -1 : 0);
    RefreshJointList(m_asset.joints.empty() ? -1 : 0);
}

void PhysicsAssetEditor::UpdateStatus(const char *message)
{
    std::string text = message ? message : "";
    if(m_dirty) text += "  [modified]";
#ifndef PHYSICS_ASSET_EDITOR_PHYSX
    text += "  (PhysX preview unavailable in this build; editing/saving still works.)";
#endif
    m_status->setLabel(text.c_str());
}

void PhysicsAssetEditor::MarkDirty()
{
    m_dirty = true;
    UpdateStatus("Physics asset modified.");
    if(g_GlWindow) g_GlWindow->redraw();
}

#ifdef PHYSICS_ASSET_EDITOR_PHYSX
static PxTransform MatrixToPx(const matrix3x4 &matrix)
{
    float values[16]; matrix.CopyToArray(values);
    return PxTransform(PxMat44(values));
}

static matrix3x4 PxToMatrix(const PxTransform &pose)
{
    const PxMat44 m(pose);
    return matrix3x4(
        m.column0.x, m.column0.y, m.column0.z,
        m.column1.x, m.column1.y, m.column1.z,
        m.column2.x, m.column2.y, m.column2.z,
        m.column3.x, m.column3.y, m.column3.z);
}

static bool BuildReferenceBoneMatrices(studiohdr_t *hdr, std::vector<matrix3x4> &out)
{
    if(!hdr || hdr->numbones <= 0) return false;
    auto *bones = reinterpret_cast<mstudiobone_t *>(reinterpret_cast<byte *>(hdr) + hdr->boneindex);
    out.resize(hdr->numbones);
    std::vector<byte> state(hdr->numbones, 0);
    std::function<bool(int)> buildBone = [&](int i) -> bool
    {
        if(i < 0 || i >= hdr->numbones) return false;
        if(state[i] == 2) return true;
        if(state[i] == 1) return false;
        state[i] = 1;
        const Vector position(bones[i].value[0], bones[i].value[1], bones[i].value[2]);
        const Radian rotation(bones[i].value[3], bones[i].value[4], bones[i].value[5]);
        const matrix3x4 local(position, rotation);
        if(bones[i].parent >= 0)
        {
            if(!buildBone(bones[i].parent)) return false;
            out[i] = out[bones[i].parent].ConcatTransforms(local);
        }
        else out[i] = local;
        state[i] = 2;
        return true;
    };
    for(int i = 0; i < hdr->numbones; ++i) if(!buildBone(i)) return false;
    return true;
}
#endif

bool PhysicsAssetEditor::RebuildPreview()
{
#ifdef PHYSICS_ASSET_EDITOR_PHYSX
    std::string error;
    if(!Validate(error)) { UpdateStatus(error.c_str()); return false; }
    if(!m_preview->Initialize()) { UpdateStatus("Failed to initialize PhysX preview."); return false; }
    m_preview->ClearScene();

    PxSceneDesc sceneDesc(m_preview->physics->getTolerancesScale());
    sceneDesc.gravity = PxVec3(0.0f, 0.0f, -800.0f);
    sceneDesc.flags = PxSceneFlag::eENABLE_CCD;
    sceneDesc.cpuDispatcher = m_preview->dispatcher;
    sceneDesc.filterShader = [](PxFilterObjectAttributes attributes0, PxFilterData filterData0,
        PxFilterObjectAttributes attributes1, PxFilterData filterData1, PxPairFlags &pairFlags,
        const void *, PxU32) -> PxFilterFlags
    {
        if(PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
        {
            pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
            return PxFilterFlag::eDEFAULT;
        }
        // Match the runtime's safe default: bodies from the same physics asset
        // do not self-collide. Ground/other actors use owner 0 and still collide.
        constexpr PxU32 kPhysicsAssetBodyFlag = (1u << 1);
        if((filterData0.word0 & kPhysicsAssetBodyFlag) && (filterData1.word0 & kPhysicsAssetBodyFlag) &&
           filterData0.word1 != 0 && filterData0.word1 == filterData1.word1)
            return PxFilterFlag::eSUPPRESS;
        pairFlags = PxPairFlag::eCONTACT_DEFAULT;
        return PxFilterFlag::eDEFAULT;
    };
    m_preview->scene = m_preview->physics->createScene(sceneDesc);
    if(!m_preview->scene) { m_preview->ClearScene(); return false; }

    m_preview->ground = PxCreatePlane(*m_preview->physics, PxPlane(0, 0, 1, 0), *m_preview->material);
    if(m_preview->ground) m_preview->scene->addActor(*m_preview->ground);
    m_preview->bodies.resize(m_asset.bodies.size(), nullptr);

    studiohdr_t *hdr = m_model.getStudioHeader();
    std::vector<matrix3x4> referenceBones;
    if(!BuildReferenceBoneMatrices(hdr, referenceBones))
    {
        UpdateStatus("Invalid studio reference skeleton; cannot build physics preview.");
        m_preview->ClearScene();
        return false;
    }
    std::vector<int> previewBoneIndices(m_asset.bodies.size(), -1);
    std::vector<PxTransform> referenceBodyPoses(m_asset.bodies.size(), PxTransform(PxIdentity));

    for(size_t i = 0; i < m_asset.bodies.size(); ++i)
    {
        const auto &body = m_asset.bodies[i];
        const int bone = FindBone(body.bone.c_str());
        if(bone < 0) { m_preview->ClearScene(); return false; }
        previewBoneIndices[i] = bone;
        referenceBodyPoses[i] = MatrixToPx(referenceBones[bone].ConcatTransforms(BodyLocalMatrix(body)));
        const matrix3x4 world = m_model.GetToolBoneTransform(bone).ConcatTransforms(BodyLocalMatrix(body));
        PxRigidDynamic *actor = m_preview->physics->createRigidDynamic(MatrixToPx(world));
        if(!actor) { m_preview->ClearScene(); return false; }
        PxShape *shape = nullptr;
        if(body.shape.type == physicsasset::ShapeType::Box)
            shape = PxRigidActorExt::createExclusiveShape(*actor, PxBoxGeometry(PxVec3(body.shape.halfExtents.x, body.shape.halfExtents.y, body.shape.halfExtents.z)), *m_preview->material);
        else if(body.shape.type == physicsasset::ShapeType::Sphere)
            shape = PxRigidActorExt::createExclusiveShape(*actor, PxSphereGeometry(body.shape.radius), *m_preview->material);
        else
            shape = PxRigidActorExt::createExclusiveShape(*actor, PxCapsuleGeometry(body.shape.radius, body.shape.halfHeight), *m_preview->material);
        if(!shape) { actor->release(); m_preview->ClearScene(); return false; }
        shape->setSimulationFilterData(PxFilterData((1u << 1), 1u, 0u, 0u));
        actor->setLinearDamping(body.linearDamping);
        actor->setAngularDamping(body.angularDamping);
        actor->setSolverIterationCounts(8, 2);
        actor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
        if(!PxRigidBodyExt::setMassAndUpdateInertia(*actor, body.mass))
        {
            actor->release();
            m_preview->ClearScene();
            return false;
        }
        m_preview->scene->addActor(*actor);
        m_preview->bodies[i] = actor;
    }

    for(const auto &jointDef : m_asset.joints)
    {
        const int parentIndex = physicsasset::FindBody(m_asset, jointDef.parent.c_str());
        const int childIndex = physicsasset::FindBody(m_asset, jointDef.child.c_str());
        if(parentIndex < 0 || childIndex < 0) { m_preview->ClearScene(); return false; }
        PxRigidDynamic *parent = m_preview->bodies[parentIndex];
        PxRigidDynamic *child = m_preview->bodies[childIndex];
        const int childBone = previewBoneIndices[childIndex];
        if(childBone < 0) { m_preview->ClearScene(); return false; }
        // Bind-pose joints must use the SAME type, authored pivot, axis and
        // limits as runtime. A D6 swing joint is not a hinge preview.
        const PxTransform jointReference = MatrixToPx(referenceBones[childBone]) *
            (jointDef.kind == physicsasset::JointKind::Hinge ?
                physicsasset::ChildHingeFrame(jointDef) : PxTransform(PxIdentity));
        const PxTransform parentFrame = referenceBodyPoses[parentIndex].getInverse() * jointReference;
        const PxTransform childFrame = referenceBodyPoses[childIndex].getInverse() * jointReference;
        if(jointDef.kind == physicsasset::JointKind::Hinge)
        {
            PxRevoluteJoint *hinge = physicsasset::CreateRevoluteJoint(*m_preview->physics,
                *parent, *child, parentFrame, childFrame, jointDef);
            if(!hinge) { m_preview->ClearScene(); return false; }
            m_preview->joints.push_back(hinge);
        }
        else
        {
            PxD6Joint *joint = PxD6JointCreate(*m_preview->physics, parent, parentFrame, child, childFrame);
            if(!joint) { m_preview->ClearScene(); return false; }
            joint->setMotion(PxD6Axis::eX, PxD6Motion::eLOCKED);
            joint->setMotion(PxD6Axis::eY, PxD6Motion::eLOCKED);
            joint->setMotion(PxD6Axis::eZ, PxD6Motion::eLOCKED);
            joint->setMotion(PxD6Axis::eTWIST, PxD6Motion::eLIMITED);
            joint->setMotion(PxD6Axis::eSWING1, PxD6Motion::eLIMITED);
            joint->setMotion(PxD6Axis::eSWING2, PxD6Motion::eLIMITED);
            joint->setTwistLimit(PxJointAngularLimitPair(PxPi * jointDef.twistLow / 180.0f, PxPi * jointDef.twistHigh / 180.0f));
            joint->setSwingLimit(PxJointLimitCone(PxPi * std::max(jointDef.swing1, 0.01f) / 180.0f, PxPi * std::max(jointDef.swing2, 0.01f) / 180.0f));
            m_preview->joints.push_back(joint);
        }
    }
    return true;
#else
    return false;
#endif
}

void PhysicsAssetEditor::StartSimulation()
{
    if(m_simulating || m_asset.bodies.empty()) return;
    if(m_selectedJoint >= 0 && !ApplyJointFields()) return;
    if(m_selectedBody >= 0 && !ApplyBodyFields()) return;
    if(!RebuildPreview())
    {
#ifndef PHYSICS_ASSET_EDITOR_PHYSX
        mxMessageBox(this, "This pxmv build does not include PhysX preview support. The asset can still be authored and saved.", APP_TITLE_STR, MX_MB_OK | MX_MB_INFORMATION);
#endif
        return;
    }
    m_simulating = true;
    m_simulateButton->setLabel("Stop");
    UpdateStatus("PhysX ragdoll preview running. Reset returns to animation pose.");
}

void PhysicsAssetEditor::StopSimulation()
{
    if(!m_simulating)
    {
        m_model.ClearToolBoneOverrides();
        return;
    }
    m_simulating = false;
    m_simulateButton->setLabel("Simulate");
    m_model.ClearToolBoneOverrides();
#ifdef PHYSICS_ASSET_EDITOR_PHYSX
    if(m_preview) m_preview->ClearScene();
#endif
    if(g_GlWindow) g_GlWindow->redraw();
}

void PhysicsAssetEditor::Update(float dt)
{
    if(!m_simulating) return;
#ifdef PHYSICS_ASSET_EDITOR_PHYSX
    if(!m_preview || !m_preview->scene) return;
    dt = std::clamp(dt, 0.0f, 0.1f);
    m_preview->accumulator += dt;
    constexpr float step = 1.0f / 60.0f;
    int steps = 0;
    while(m_preview->accumulator >= step && steps++ < 4)
    {
        m_preview->scene->simulate(step);
        m_preview->scene->fetchResults(true);
        m_preview->accumulator -= step;
    }
    SyncSimulationPoseToModel();
#endif
}

void PhysicsAssetEditor::SyncSimulationPoseToModel()
{
#ifdef PHYSICS_ASSET_EDITOR_PHYSX
    if(!m_simulating || !m_preview) return;
    m_model.ClearToolBoneOverrides();
    for(size_t i = 0; i < m_asset.bodies.size() && i < m_preview->bodies.size(); ++i)
    {
        PxRigidDynamic *actor = m_preview->bodies[i];
        if(!actor) continue;
        const int bone = FindBone(m_asset.bodies[i].bone.c_str());
        if(bone < 0) continue;
        const PxTransform local = MatrixToPx(BodyLocalMatrix(m_asset.bodies[i]));
        const PxTransform boneWorld = actor->getGlobalPose() * local.getInverse();
        m_model.SetToolBoneOverride(bone, PxToMatrix(boneWorld));
    }
#endif
}

void PhysicsAssetEditor::DrawOverlay()
{
    if(!isVisible() || (m_asset.bodies.empty() && m_asset.collisions.empty())) return;
    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.5f);

    // Root collision vertices are model-local. v3 bone-bound vertices are
    // stored in bone-local space, so preview them through the currently shown
    // Studio pose; an animated door therefore carries its green wireframe.
    glColor3f(0.55f, 1.0f, 0.25f);
    glBegin(GL_LINES);
    for(const auto &collision : m_asset.collisions)
    {
        matrix3x4 boneMatrix;
        const bool boneBound = !collision.bone.empty();
        if(boneBound)
        {
            const int bone = FindBone(collision.bone.c_str());
            if(bone < 0) continue;
            boneMatrix = m_model.GetToolBoneTransform(bone);
        }
        auto drawVertex = [&](const physicsasset::Vec3 &v)
        {
            Vector point(v.x, v.y, v.z);
            if(boneBound) point = boneMatrix.VectorTransform(point);
            glVertex3f(point.x, point.y, point.z);
        };
        for(size_t i = 0; i + 2 < collision.indices.size(); i += 3)
        {
            const std::uint32_t ia = collision.indices[i];
            const std::uint32_t ib = collision.indices[i + 1];
            const std::uint32_t ic = collision.indices[i + 2];
            if(ia >= collision.vertices.size() || ib >= collision.vertices.size() || ic >= collision.vertices.size()) continue;
            const auto &a = collision.vertices[ia]; const auto &b = collision.vertices[ib]; const auto &c = collision.vertices[ic];
            drawVertex(a); drawVertex(b);
            drawVertex(b); drawVertex(c);
            drawVertex(c); drawVertex(a);
        }
    }
    glEnd();

    std::vector<matrix3x4> bodyMatrices(m_asset.bodies.size());
    for(size_t i = 0; i < m_asset.bodies.size(); ++i)
    {
#ifdef PHYSICS_ASSET_EDITOR_PHYSX
        if(m_simulating && m_preview && i < m_preview->bodies.size() && m_preview->bodies[i])
            bodyMatrices[i] = PxToMatrix(m_preview->bodies[i]->getGlobalPose());
        else
#endif
        {
            const int bone = FindBone(m_asset.bodies[i].bone.c_str());
            if(bone < 0) continue;
            bodyMatrices[i] = m_model.GetToolBoneTransform(bone).ConcatTransforms(BodyLocalMatrix(m_asset.bodies[i]));
        }

        float matrix[16]; bodyMatrices[i].CopyToArray(matrix);
        glPushMatrix(); glMultMatrixf(matrix);
        if((int)i == m_selectedBody) glColor3f(1.0f, 0.75f, 0.1f); else glColor3f(0.15f, 0.85f, 1.0f);
        const auto &shape = m_asset.bodies[i].shape;
        if(shape.type == physicsasset::ShapeType::Box) DrawBox(shape.halfExtents);
        else if(shape.type == physicsasset::ShapeType::Sphere) DrawSphere(shape.radius);
        else DrawCapsule(shape.radius, shape.halfHeight);
        glPopMatrix();
    }

    glColor3f(1.0f, 0.25f, 0.4f);
    glBegin(GL_LINES);
    for(const auto &joint : m_asset.joints)
    {
        const int a = physicsasset::FindBody(m_asset, joint.parent.c_str());
        const int b = physicsasset::FindBody(m_asset, joint.child.c_str());
        if(a < 0 || b < 0) continue;
        const Vector pa = bodyMatrices[a].GetOrigin();
        const Vector pb = bodyMatrices[b].GetOrigin();
        glVertex3fv(pa); glVertex3fv(pb);
    }
    glEnd();
    glPopAttrib();
}
