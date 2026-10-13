#include "PhysicsAssetEditor.h"
#include "physics_asset_constraint_guide.h"
#include "RagdollTemplateDialog.h"
#include "PhysicsSettingsDialog.h"
#include "PhysicsAssetHelpDialog.h"
#include "PhysicsAssetUiGuide.h"
#include "LegacyCpStudioValidator.h"
#include "physics_asset_scale.h"
#include "physics_asset_settings.h"
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
#include <mx/mxCheckBox.h>
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
#include <chrono>
#include <cerrno>

#ifdef PHYSICS_ASSET_EDITOR_PHYSX
#include <PxPhysicsAPI.h>
#include "../../server/physx/physics_asset_construction.h"
#include "../../server/physx/physics_asset_contact_filter.h"
#include "../../server/physx/physics_hinge_frame.h"
#include "../../server/physx/physics_d6_frame.h"
#include "../../server/physx/physics_asset_motion.h"
#include "../../server/physx/physics_asset_diagnostics.h"
using namespace physx;
#endif

namespace
{
static void SetFloat(mxLineEdit *edit, float value)
{
    if(edit) edit->setLabel(PhysicsUiNumber(value).c_str());
}

// Reject malformed interactive values rather than silently falling back to
// their previous values. This is important for hinge limits and break thresholds.
static bool ReadStrictFloat(mxLineEdit *edit, float &value)
{
    if(!edit) return false;
    // Toolkit label bytes are borrowed until the next getter call. Own the
    // text before parsing so a later Qt getter cannot invalidate end.
    const char *label = edit->getLabel();
    const std::string text = label ? label : "";
    if(text.empty()) return false;
    char *end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    if(end == text.c_str() || !end || *end != '\0' || !std::isfinite(parsed)) return false;
    value = parsed;
    return true;
}

static bool ReadStrictInt(mxLineEdit *edit, int &value)
{
    if(!edit) return false;
    const char *label = edit->getLabel();
    const std::string text = label ? label : "";
    if(text.empty()) return false;
    char *end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if(end == text.c_str() || !end || *end != '\0' ||
       parsed < (std::numeric_limits<int>::min)() || parsed > (std::numeric_limits<int>::max)())
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
    physicsasset::AssetContactFilter contactFilter;
    std::unique_ptr<physicsasset::ContactGroup> contactGroup;
    bool extensionsInitialized = false;
    std::vector<PxRigidDynamic *> bodies;
    std::vector<PxJoint *> joints;
    float accumulator = 0.0f;
    ragdolldiag::Cadence diagnosticsCadence;
    ragdolldiag::Window diagnosticsWindow;
    double diagnosticsBindSpan = 0;
    int diagnosticsRootBody = -1;
    int diagnosticsRootBone = -1;

    ~PreviewImpl() { Shutdown(); }

    bool Initialize()
    {
        if(physics) return true;
        foundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator, errorCallback);
        if(!foundation) return false;
        physics = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, physicsasset::AssetTolerancesScale(), false, nullptr);
        if(!physics) { Shutdown(); return false; }
        if(!PxInitExtensions(*physics, nullptr)) { Shutdown(); return false; }
        extensionsInitialized = true;
        dispatcher = PxDefaultCpuDispatcherCreate(1);
        if(!dispatcher) { Shutdown(); return false; }
        material = physicsasset::CreateAssetMaterial(*physics);
        if(!material) { Shutdown(); return false; }
        return true;
    }

    void ClearScene()
    {
        for(PxJoint *joint : joints) if(joint) joint->release();
        joints.clear();
        for(PxRigidDynamic *body : bodies) if(body) body->release();
        bodies.clear();
        contactGroup.reset(); // all shapes released before token storage
        if(ground) { ground->release(); ground = nullptr; }
        if(scene) { scene->release(); scene = nullptr; }
        accumulator = 0.0f;
        diagnosticsCadence.Reset();
        diagnosticsWindow.Reset();
        diagnosticsBindSpan = 0;
        diagnosticsRootBody = diagnosticsRootBone = -1;
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
    : mxWindow(parent, 40, 40, 860, 670, "Physics Asset Editor K.11.4", mxWindow::Dialog),
      m_model(model), m_settings(settings), m_preview(new PreviewImpl)
{
    BuildUI();
    setVisible(false);
}

PhysicsAssetEditor::~PhysicsAssetEditor()
{
    StopSimulation();
    delete m_contextGuide;
    delete m_jointGuide;
}

void PhysicsAssetEditor::BuildUI()
{
    new mxLabel(this,10,12,80,24,"Section");
    m_page=new mxChoice(this,96,8,250,28,IDC_PHYS_PAGE);
    for(const char *name:{"Bodies","Joints","Interactive parts","Asset / preview","Help"}) m_page->add(name);
    m_page->select(0);
    m_advanced=new mxCheckBox(this,366,8,170,28,"Advanced fields",IDC_PHYS_ADVANCED);
    m_textSize=new mxChoice(this,546,8,170,28,IDC_PHYS_TEXT_SIZE);
    for(const char *name:{"Normal 100%","Large 125%","Extra large 150%"})m_textSize->add(name);
    m_textSize->select(physui::ChoiceFromPercent(physui::CurrentTextScale()));
    new mxButton(this,726,8,124,28,"Help / ?",IDC_PHYS_HELP);
    new mxLabel(this,10,44,840,26,"Select a body or joint, edit its fields, then Apply. Advanced fields keep their saved values when hidden.");
    auto label=[this](unsigned page,int y,const char *text,bool advanced=false) {
        m_pages.Add(new mxLabel(this,278,y+4,158,24,text),page,advanced);
    };
    auto field=[this](unsigned page,int y,const char *value,bool advanced=false) {
        return m_pages.Add(new mxLineEdit(this,448,y,340,28,value),page,advanced);
    };
    const unsigned bodies=1u, joints=2u, parts=4u, asset=8u, help=16u;
    m_pages.Add(new mxLabel(this,10,80,240,24,"Bodies / physical shapes"),bodies|parts);
    m_bodyList=m_pages.Add(new mxListBox(this,10,108,240,280,IDC_PHYS_BODY_LIST),bodies|parts);
    m_pages.Add(new mxButton(this,10,398,76,30,"Add",IDC_PHYS_BODY_ADD),bodies|parts);
    m_pages.Add(new mxButton(this,92,398,76,30,"Remove",IDC_PHYS_BODY_REMOVE),bodies|parts);
    m_pages.Add(new mxButton(this,174,398,76,30,"Apply",IDC_PHYS_BODY_APPLY),bodies|parts);
    label(bodies,80,"Body name");m_bodyName=field(bodies,80,"");
    label(bodies,116,"Bone");m_boneChoice=m_pages.Add(new mxChoice(this,448,116,340,28,IDC_PHYS_BONE),bodies);
    label(bodies,152,"Shape");m_shapeChoice=m_pages.Add(new mxChoice(this,448,152,340,28,IDC_PHYS_SHAPE),bodies);
    for(const char *name:{"Capsule","Box","Sphere"})m_shapeChoice->add(name);
    const char *axes[]={"X","Y","Z"};
    for(int i=0;i<3;++i)
    {
        const int x=448+i*114;
        m_pages.Add(new mxLabel(this,x,184,106,24,axes[i]),bodies);
        m_offset[i]=m_pages.Add(new mxLineEdit(this,x,210,106,28,"0"),bodies);
        m_angles[i]=m_pages.Add(new mxLineEdit(this,x,246,106,28,"0"),bodies,true);
        m_dimensionLabels[i]=m_pages.Add(new mxLabel(this,x,278,106,24,axes[i]),bodies);
        m_dimensions[i]=m_pages.Add(new mxLineEdit(this,x,304,106,28,"1"),bodies);
    }
    label(bodies,210,"Offset XYZ");label(bodies,246,"Rotation degrees",true);
    label(bodies,304,"Shape dimensions");label(bodies,340,"Mass");m_mass=field(bodies,340,"1");
    label(bodies,376,"Hitgroup");m_hitGroup=field(bodies,376,"0");
    label(bodies,412,"Linear damping",true);m_linearDamping=field(bodies,412,"0.05",true);
    label(bodies,448,"Angular damping",true);m_angularDamping=field(bodies,448,"0.05",true);
    m_pages.Add(new mxLabel(this,278,492,550,48,"Use a template for a full ragdoll. Offset and dimensions fit the shape to the selected bone."),bodies);

    m_pages.Add(new mxLabel(this,10,80,240,24,"Connections between bodies"),joints);
    m_jointList=m_pages.Add(new mxListBox(this,10,108,240,280,IDC_PHYS_JOINT_LIST),joints);
    m_pages.Add(new mxButton(this,10,398,76,30,"Add",IDC_PHYS_JOINT_ADD),joints);
    m_pages.Add(new mxButton(this,92,398,76,30,"Remove",IDC_PHYS_JOINT_REMOVE),joints);
    m_pages.Add(new mxButton(this,174,398,76,30,"Apply",IDC_PHYS_JOINT_APPLY),joints);
    label(joints,80,"Joint name");m_jointName=field(joints,80,"");
    label(joints,116,"Parent body");m_jointParent=m_pages.Add(new mxChoice(this,448,116,340,28,IDC_PHYS_JOINT_PARENT),joints);
    label(joints,152,"Child body");m_jointChild=m_pages.Add(new mxChoice(this,448,152,340,28,IDC_PHYS_JOINT_CHILD),joints);
    label(joints,188,"Joint type");m_jointKindChoice=m_pages.Add(new mxChoice(this,448,188,340,28),joints);
    m_jointKindChoice->add("D6 / ragdoll");m_jointKindChoice->add("Hinge / prop");
    m_pages.Add(new mxButton(this,448,224,190,28,"Make hinge",IDC_PHYS_MAKE_HINGE),joints);
    label(joints,264,"Swing Y / Z",true);m_swing1=m_pages.Add(new mxLineEdit(this,448,264,164,28,"45"),joints,true);m_swing2=m_pages.Add(new mxLineEdit(this,624,264,164,28,"45"),joints,true);
    label(joints,300,"Twist low / high",true);m_twistLow=m_pages.Add(new mxLineEdit(this,448,300,164,28,"-45"),joints,true);m_twistHigh=m_pages.Add(new mxLineEdit(this,624,300,164,28,"45"),joints,true);
    label(joints,336,"Twist / Y / Z mode",true);
    for(int i=0;i<3;++i)
    {
        m_jointMotion[i]=m_pages.Add(new mxChoice(this,448+i*114,336,106,28),joints,true);
        for(const char *name:{"Locked","Limited","Free"})m_jointMotion[i]->add(name);
        m_frameAnchor[i]=m_pages.Add(new mxLineEdit(this,448+i*114,372,106,28,"0"),joints,true);
        m_frameAngles[i]=m_pages.Add(new mxLineEdit(this,448+i*114,408,106,28,"0"),joints,true);
        m_hingeAnchor[i]=m_pages.Add(new mxLineEdit(this,448+i*114,480,106,28,"0"),joints,true);
        m_hingeAxis[i]=m_pages.Add(new mxLineEdit(this,448+i*114,516,106,28,i==0?"1":"0"),joints,true);
    }
    label(joints,372,"D6 anchor XYZ",true);label(joints,408,"D6 pitch / yaw / roll",true);
    label(joints,444,"Hinge low / high",true);m_hingeMin=m_pages.Add(new mxLineEdit(this,448,444,164,28,"0"),joints,true);m_hingeMax=m_pages.Add(new mxLineEdit(this,624,444,164,28,"90"),joints,true);
    label(joints,480,"Hinge anchor XYZ",true);label(joints,516,"Hinge axis XYZ",true);
    // The basic explanation yields its space to the advanced controls.
    m_jointBasicNote=new mxLabel(this,278,270,550,80,"Joint limits come from the selected template. Enable Advanced fields to tune angles, motion modes and pivots.");

    label(parts,80,"Part name");m_partName=field(parts,80,"");
    label(parts,116,"Drive mode");m_propDriveChoice=m_pages.Add(new mxChoice(this,448,116,340,28),parts);
    for(const char *name:{"None / ragdoll","Animated","Driven","Free"})m_propDriveChoice->add(name);
    m_pages.Add(new mxButton(this,448,152,190,28,"Make interactive part",IDC_PHYS_MAKE_PART),parts);
    label(parts,206,"Drive strength",true);m_driveStrength=field(parts,206,"0",true);
    label(parts,242,"Drive damping",true);m_driveDamping=field(parts,242,"0",true);
    label(parts,278,"Maximum force",true);m_driveMaxForce=field(parts,278,"0",true);
    m_pages.Add(new mxLabel(this,278,326,550,26,"Selected joint: damage / break thresholds"),parts,true);
    label(parts,362,"Release damage",true);m_releaseDamage=field(parts,362,"0",true);
    label(parts,398,"Break damage",true);m_breakDamage=field(parts,398,"0",true);
    label(parts,434,"Break impulse",true);m_breakImpulse=field(parts,434,"0",true);
    m_pages.Add(new mxButton(this,448,474,220,28,"Apply joint thresholds",IDC_PHYS_JOINT_APPLY),parts,true);

    m_pages.Add(new mxLabel(this,10,86,825,56,"Existing .phys: load its render model, then Reload or Open .phys. Existing .ragtemplate: open Templates and use Import template."),asset);
    m_pages.Add(new mxButton(this,10,160,190,32,"Open .phys...",IDC_PHYS_OPEN_ASSET),asset);
    m_pages.Add(new mxButton(this,218,160,190,32,"Physics settings...",IDC_PHYS_SETTINGS),asset);
    m_pages.Add(new mxButton(this,426,160,190,32,"Import _cp collision",IDC_PHYS_IMPORT_CP),asset);
    m_pages.Add(new mxButton(this,634,160,200,32,"Clear collision",IDC_PHYS_CLEAR_COLLISION),asset,true);
    m_pages.Add(new mxLabel(this,10,218,210,24,"Permanent asset resize"),asset,true);
    m_geometryScale=m_pages.Add(new mxLineEdit(this,228,214,140,28,"1"),asset,true);
    m_pages.Add(new mxButton(this,386,214,200,30,"Resize authored .phys",IDC_PHYS_SCALE_ASSET),asset,true);
    m_pages.Add(new mxLabel(this,10,254,825,48,"Resize only after permanently resizing the MDL. Runtime entity scale already scales physics automatically."),asset,true);
    m_diagnosticsButton=m_pages.Add(new mxButton(this,10,320,200,30,"Diagnostics: Off",IDC_PHYS_DIAGNOSTICS),asset,true);
    m_pages.Add(new mxButton(this,228,320,200,30,"Save diagnostic log",IDC_PHYS_SAVE_DIAGNOSTICS),asset,true);
    m_jointGuidesButton=m_pages.Add(new mxButton(this,446,320,300,30,"Joint guides: On",IDC_PHYS_JOINT_GUIDES),asset);
    m_pages.Add(new mxButton(this,10,380,200,30,"Undo template apply",IDC_PHYS_UNDO_GENERATION),asset);
    m_pages.Add(new mxLabel(this,10,430,190,24,"Simulation mode"),asset);
    m_simulationModeChoice=m_pages.Add(new mxChoice(this,210,426,640,28,IDC_PHYS_SIMULATION_MODE),asset);
    m_simulationModeChoice->add("Full ragdoll: freeze unboxed bones");
    m_simulationModeChoice->add("Partial ragdoll: animate unboxed bones");
    m_simulationModeChoice->select(0);
    m_pages.Add(new mxLabel(this,10,474,825,64,"Full captures unboxed fingers, hands and feet; Partial lets them animate while following their physical parents. Defined bodies use PhysX in both modes. Switch modes at any time; Stop / Reset restores animation."),asset);

    m_contextGuide=new PhysicsAssetContextGuide(this,84,850,"Bodies",IDC_PHYS_CONTEXT_TOPIC,IDC_PHYS_CONTEXT_DETAILS);
    m_jointGuide=new PhysicsAssetContextGuide(this,196,850,"Joints",IDC_PHYS_JOINT_CONTEXT_TOPIC,IDC_PHYS_JOINT_CONTEXT_DETAILS);
    for(auto *control:m_contextGuide->Controls())m_pages.Add(control,help);
    for(auto *control:m_jointGuide->Controls())m_pages.Add(control,help);
    m_contextGuide->Bind(m_bodyList,"Body list / Add / Remove / Apply");m_contextGuide->Bind(m_boneChoice,"Bone");
    m_contextGuide->Bind(m_shapeChoice,"Shape / Dimensions");m_contextGuide->Bind(m_mass,"Mass");
    m_contextGuide->Bind(m_linearDamping,"Linear / Angular damping");m_contextGuide->Bind(m_angularDamping,"Linear / Angular damping");m_contextGuide->Bind(m_hitGroup,"Hitgroup");
    m_jointGuide->Bind(m_jointList,"Joint list / Add / Remove / Apply");m_jointGuide->Bind(m_jointKindChoice,"Joint type");
    m_jointGuide->Bind(m_jointParent,"Parent / Child");m_jointGuide->Bind(m_jointChild,"Parent / Child");
    for(auto *field:m_jointMotion)m_jointGuide->Bind(field,"T/Y/Z motion");
    m_pages.Add(new mxButton(this,10,330,280,32,"Beginner walkthrough",IDC_PHYS_BEGINNER_GUIDE),help);
    m_pages.Add(new mxLabel(this,10,384,825,120,"1. Load a render model. 2. Templates: choose a preset, edit a copy or import a .ragtemplate. 3. Generate, inspect the Review page and acknowledge required checks. 4. Apply + save .phys transfers the candidate and saves it. Reload edits an existing model sidecar."),help);

    new mxButton(this,10,574,154,32,"Templates...",IDC_PHYS_AUTOGENERATE);
    m_simulateButton=new mxButton(this,174,574,154,32,"Simulate",IDC_PHYS_SIMULATE);
    new mxButton(this,338,574,154,32,"Reset",IDC_PHYS_RESET);
    new mxButton(this,502,574,154,32,"Save .phys",IDC_PHYS_SAVE);
    new mxButton(this,666,574,184,32,"Reload .phys",IDC_PHYS_RELOAD);
    m_status=new mxLabel(this,10,616,840,44,"Load a model to edit or create its physics asset.");
    physui::InstallScrollableScaling(this,860,670);
    ShowPage();
}

void PhysicsAssetEditor::ShowPage()
{
    const int page=m_page->getSelectedIndex();
    m_pages.Show(page,m_advanced->isChecked());
    m_jointBasicNote->setVisible(page==1&&!m_advanced->isChecked());
}

void PhysicsAssetEditor::SetSimulationMode(SimulationMode mode)
{
    if(mode!=SimulationMode::PartialRagdoll)mode=SimulationMode::FullRagdoll;
    const bool changed=mode!=m_simulationMode;
    m_simulationMode=mode;
    if(m_simulationModeChoice)m_simulationModeChoice->select(mode==SimulationMode::PartialRagdoll?1:0);
    if(m_templateDialog)m_templateDialog->SyncSimulationMode();
    if(!changed)return;
    if(m_simulating)
    {
        // Keep the same physical scene and velocities. Only the source pose
        // for bones without bodies changes; capture the currently drawn pose.
        if(mode==SimulationMode::FullRagdoll)m_model.CaptureToolAnimationPose();
        else m_model.ReleaseToolAnimationPose();
        if(g_GlWindow)g_GlWindow->redraw();
    }
    UpdateStatus(mode==SimulationMode::FullRagdoll
        ?"Full preview selected: unboxed bones keep their captured pose during simulation."
        :"Partial preview selected: unboxed bones can animate while defined bodies use PhysX.");
}

void PhysicsAssetEditor::OpenAsset()
{
    if(m_modelPath.empty()) {UpdateStatus("Load the matching render model before opening a .phys file.");return;}
    const char *selected=mxGetOpenFileName(this,"","Physics assets (*.phys)\0*.phys\0\0");
    if(!selected)return;
    const std::string path=selected;
    if(m_dirty&&mxMessageBox(this,"Discard unsaved asset edits and open the selected .phys?","Open physics asset",MX_MB_YESNO|MX_MB_QUESTION)!=0)return;
    physicsasset::AssetDef loaded;std::string error;
    if(!physicsasset::LoadFile(path.c_str(),loaded,&error)){UpdateStatus(error.c_str());return;}
    for(const auto &body:loaded.bodies)if(FindBone(body.bone.c_str())<0){UpdateStatus("This .phys uses a bone missing from the loaded render model.");return;}
    if(m_templateDialog)m_templateDialog->Cancel();
    if(m_settingsDialog)m_settingsDialog->Cancel();
    StopSimulation();m_generation=ragtemplate::Transaction{};m_asset=std::move(loaded);m_assetPath=path;m_asset.sourcePath=path;m_dirty=false;
    RefreshBodyList(m_asset.bodies.empty()?-1:0);RefreshJointList(m_asset.joints.empty()?-1:0);
    m_page->select(0);ShowPage();UpdateStatus("Opened existing .phys. Edit fields, Apply, then Save .phys.");
}


int PhysicsAssetEditor::handleEvent(mxEvent *event)
{
    if(!event || event->event != mxEvent::Action) return 1;
    switch(event->action)
    {
    case IDC_PHYS_PAGE:case IDC_PHYS_ADVANCED:ShowPage();break;
    case IDC_PHYS_SIMULATION_MODE:
        SetSimulationMode(m_simulationModeChoice->getSelectedIndex()==1?SimulationMode::PartialRagdoll:SimulationMode::FullRagdoll);break;
    case IDC_PHYS_OPEN_ASSET:OpenAsset();break;
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
    case IDC_PHYS_BEGINNER_GUIDE:
        if(!m_helpDialog)m_helpDialog=new PhysicsAssetHelpDialog(this);
        m_helpDialog->Open("Start here","First ragdoll walkthrough");break;
    case IDC_PHYS_JOINT_CONTEXT_TOPIC:m_jointGuide->SelectionChanged();break;
    case IDC_PHYS_JOINT_CONTEXT_DETAILS:m_jointGuide->ShowDetails(m_helpDialog);break;
    case IDC_PHYS_CONTEXT_TOPIC:m_contextGuide->SelectionChanged();break;
    case IDC_PHYS_CONTEXT_DETAILS:m_contextGuide->ShowDetails(m_helpDialog);break;
    case IDC_PHYS_TEXT_SIZE:
        physui::SetTextScale(physui::PercentFromChoice(m_textSize->getSelectedIndex()));
        physui::RefreshScaling(this);break;
    case IDC_PHYS_HELP:
        if(!m_helpDialog) m_helpDialog = new PhysicsAssetHelpDialog(this);
        m_helpDialog->Open("Start here");
        break;
    case IDC_PHYS_JOINT_GUIDES:
        m_showJointGuides=!m_showJointGuides;
        m_jointGuidesButton->setLabel(m_showJointGuides?"Joint guides: On (select a joint)":"Joint guides: Off");
        if(g_GlWindow) g_GlWindow->redraw();
        break;
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
    case IDC_PHYS_SETTINGS: EditPhysicsSettings(); break;
    case IDC_PHYS_AUTOGENERATE:
        AutoGenerate();
        break;
    case IDC_PHYS_UNDO_GENERATION: UndoGeneration(); break;
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
    case IDC_PHYS_DIAGNOSTICS: ToggleDiagnostics(); break;
    case IDC_PHYS_SAVE_DIAGNOSTICS: SaveDiagnostics(); break;
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
    if(g_GlWindow) g_GlWindow->redraw();
    return 1;
}

void PhysicsAssetEditor::OnModelLoaded(const char *modelPath)
{
    if(m_settingsDialog) m_settingsDialog->Cancel();
    StopSimulation();
    m_modelPath = modelPath ? modelPath : "";
    m_diagnosticsRows.clear();
    m_diagnosticsMetadata.clear();
    m_lastDiagnostics = ragdolldiag::Snapshot{};
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
    const auto effective=physicsasset::ResolveBodySettings(m_asset,body);
    SetFloat(m_linearDamping,effective.linearDamping);SetFloat(m_angularDamping,effective.angularDamping);
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
    m_jointMotion[0]->select(static_cast<int>(joint.twistMotion));
    m_jointMotion[1]->select(static_cast<int>(joint.swing1Motion));
    m_jointMotion[2]->select(static_cast<int>(joint.swing2Motion));
    SetFloat(m_frameAnchor[0],joint.frameAnchor.x);SetFloat(m_frameAnchor[1],joint.frameAnchor.y);SetFloat(m_frameAnchor[2],joint.frameAnchor.z);
    SetFloat(m_frameAngles[0],joint.frameAngles.x);SetFloat(m_frameAngles[1],joint.frameAngles.y);SetFloat(m_frameAngles[2],joint.frameAngles.z);
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
    const auto effective=physicsasset::ResolveBodySettings(m_asset,body);
    if(body.linearDamping>=0||linearDamping!=effective.linearDamping) body.linearDamping=linearDamping;
    if(body.angularDamping>=0||angularDamping!=effective.angularDamping) body.angularDamping=angularDamping;
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
        for(auto &pair:m_asset.collisionPairs)
        {if(!Q_stricmp(pair.first.c_str(),oldName.c_str())) pair.first=body.name;if(!Q_stricmp(pair.second.c_str(),oldName.c_str())) pair.second=body.name;}
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
    float hmin, hmax, release, damage, impulse, anchor[3], axis[3], frameAnchor[3], frameAngles[3];
    if(!ReadStrictFloat(m_hingeMin, hmin) || !ReadStrictFloat(m_hingeMax, hmax) ||
       !ReadStrictFloat(m_releaseDamage, release) || !ReadStrictFloat(m_breakDamage, damage) ||
       !ReadStrictFloat(m_breakImpulse, impulse) || release < 0 || damage < 0 || impulse < 0)
    {
        UpdateStatus("Invalid hinge limits or damage thresholds.");
        return false;
    }
    for(int i = 0; i < 3; ++i)
    {
        if(!ReadStrictFloat(m_hingeAnchor[i], anchor[i]) || !ReadStrictFloat(m_hingeAxis[i], axis[i]) ||
           !ReadStrictFloat(m_frameAnchor[i],frameAnchor[i]) || !ReadStrictFloat(m_frameAngles[i],frameAngles[i]))
        {
            UpdateStatus("Joint frames require finite XYZ and Euler values.");
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
    for(int i = 0; i < 3; ++i)
        if(m_jointMotion[i]->getSelectedIndex() < 0 || m_jointMotion[i]->getSelectedIndex() > 2)
        { UpdateStatus("Select valid D6 angular motions."); return false; }
    joint.twistMotion = joint.kind == physicsasset::JointKind::Hinge ? physicsasset::AngularMotion::Limited : static_cast<physicsasset::AngularMotion>(m_jointMotion[0]->getSelectedIndex());
    joint.swing1Motion = joint.kind == physicsasset::JointKind::Hinge ? physicsasset::AngularMotion::Limited : static_cast<physicsasset::AngularMotion>(m_jointMotion[1]->getSelectedIndex());
    joint.swing2Motion = joint.kind == physicsasset::JointKind::Hinge ? physicsasset::AngularMotion::Limited : static_cast<physicsasset::AngularMotion>(m_jointMotion[2]->getSelectedIndex());
    joint.frameAnchor=joint.kind==physicsasset::JointKind::D6?physicsasset::Vec3{frameAnchor[0],frameAnchor[1],frameAnchor[2]}:physicsasset::Vec3{};
    joint.frameAngles=joint.kind==physicsasset::JointKind::D6?physicsasset::Vec3{frameAngles[0],frameAngles[1],frameAngles[2]}:physicsasset::Vec3{};
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
    if(joint.kind == physicsasset::JointKind::Hinge || release || damage || impulse ||
        joint.twistMotion != physicsasset::AngularMotion::Limited || joint.swing1Motion != physicsasset::AngularMotion::Limited || joint.swing2Motion != physicsasset::AngularMotion::Limited)
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
    joint.twistMotion = joint.swing1Motion = joint.swing2Motion = physicsasset::AngularMotion::Limited;
    joint.frameAnchor={};joint.frameAngles={};
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
    m_asset.collisionPairs.erase(std::remove_if(m_asset.collisionPairs.begin(),m_asset.collisionPairs.end(),[&](const auto &pair){return !Q_stricmp(pair.first.c_str(),name.c_str())||!Q_stricmp(pair.second.c_str(),name.c_str());}),m_asset.collisionPairs.end());
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
    joint.twistMotion = joint.swing1Motion = joint.swing2Motion = physicsasset::AngularMotion::Limited;
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
    if(!m_model.getStudioHeader()) { UpdateStatus("Load a model before opening template generation."); return; }
    if(!m_templateDialog) m_templateDialog = new RagdollTemplateDialog(*this);
    m_templateDialog->Open();
}

bool PhysicsAssetEditor::CollectTemplateBones(std::vector<ragtemplate::Bone> &out, std::string &error, const std::vector<std::string> &excludedSections)
{
    const auto *hdr = m_model.getStudioHeader();
    if(!hdr || hdr->numbones <= 0 || hdr->numbones > MAXSTUDIOBONES)
    { error = "Invalid Studio skeleton."; return false; }
    const auto *bones = reinterpret_cast<const mstudiobone_t *>(reinterpret_cast<const byte *>(hdr) + hdr->boneindex);
    std::vector<std::vector<Vector>> vertices;
    if(!m_model.GetRagdollFitVertices(vertices, excludedSections)) { error="Invalid reference geometry."; return false; }
    std::vector<ragtemplate::Bone> result(hdr->numbones);
    for(int i = 0; i < hdr->numbones; ++i)
    {
        auto &bone = result[i]; bone.name = bones[i].name; bone.parent = bones[i].parent;
        bone.localOrigin = {bones[i].value[0],bones[i].value[1],bones[i].value[2]};
        bone.localAngles = {bones[i].value[3],bones[i].value[4],bones[i].value[5]};
        for(const Vector &v:vertices[i])
        {
            const auto point=ToAssetVec(v);bone.vertices.push_back(point);
            if(!bone.hasBounds) { bone.boundsMin=bone.boundsMax=point;bone.hasBounds=true; }
            else
            {
                bone.boundsMin={std::min(bone.boundsMin.x,point.x),std::min(bone.boundsMin.y,point.y),std::min(bone.boundsMin.z,point.z)};
                bone.boundsMax={std::max(bone.boundsMax.x,point.x),std::max(bone.boundsMax.y,point.y),std::max(bone.boundsMax.z,point.z)};
            }
        }
        double longest = 0;
        for(int child = 0; child < hdr->numbones; ++child)
        {
            if(bones[child].parent != i) continue;
            const Vector direction(bones[child].value[0],bones[child].value[1],bones[child].value[2]);
            if(direction.Length() > longest) { longest = direction.Length(); bone.childOffset = ToAssetVec(direction); }
        }
    }
    out = std::move(result); return true;
}

void PhysicsAssetEditor::SelectGenerationJoint(const std::string &name)
{
    m_generationSelectedJoint.clear();
    if(m_generationPreview)
        for(const auto &joint:m_generationPreview->joints)
            if(joint.name==name) {m_generationSelectedJoint=name;break;}
    if(g_GlWindow) g_GlWindow->redraw();
}

void PhysicsAssetEditor::ClearGenerationPreview()
{
    StopSimulation();
    m_generationSelectedJoint.clear();
    m_generationPreview.reset();
    if(g_GlWindow) g_GlWindow->redraw();
}

void PhysicsAssetEditor::EditPhysicsSettings()
{
    if(m_generationPreview) {UpdateStatus("Apply or cancel the generated preview before editing asset settings. Use the template profile to change a candidate.");return;}
    if(!HasAsset()) {UpdateStatus("Add/import collision or generate a rig before editing physics settings.");return;}
    StopSimulation();if(m_settingsDialog&&m_settingsDialog->isVisible()) return;
    const std::string before=physicsasset::Serialize(m_asset),model=m_modelPath;
    auto apply=[this,before,model](const physicsasset::AssetDef &value)->bool
    {
        if(model!=m_modelPath||before!=physicsasset::Serialize(m_asset)) {UpdateStatus("Asset/model changed. Reopen physics settings.");return false;}
        bool updateLinear=false,updateAngular=false;float number=0;
        if(m_selectedBody>=0&&m_selectedBody<int(m_asset.bodies.size()))
        {
            const auto old=physicsasset::ResolveBodySettings(m_asset,m_asset.bodies[m_selectedBody]);
            updateLinear=ReadStrictFloat(m_linearDamping,number)&&number==old.linearDamping;
            updateAngular=ReadStrictFloat(m_angularDamping,number)&&number==old.angularDamping;
        }
        m_asset=value;MarkDirty();
        if(m_selectedBody>=0&&m_selectedBody<int(m_asset.bodies.size()))
        {
            const auto now=physicsasset::ResolveBodySettings(m_asset,m_asset.bodies[m_selectedBody]);
            if(updateLinear) SetFloat(m_linearDamping,now.linearDamping);
            if(updateAngular) SetFloat(m_angularDamping,now.angularDamping);
        }
        UpdateStatus("Applied physics settings. Save writes the asset; pending body/joint text is preserved.");return true;
    };
    if(m_settingsDialog) m_settingsDialog->Open(m_asset,std::move(apply));else m_settingsDialog=new PhysicsSettingsDialog(this,m_asset,false,std::move(apply));
}

void PhysicsAssetEditor::UndoGeneration()
{
    ClearGenerationPreview();
    if(m_templateDialog && m_templateDialog->isVisible()) m_templateDialog->Cancel();
    std::string error;
    if(!m_generation.Undo(m_asset,&error)) { UpdateStatus(error.c_str()); return; }
    MarkDirty(); RefreshBodyList(m_asset.bodies.empty() ? -1 : 0); RefreshJointList(m_asset.joints.empty() ? -1 : 0);
    UpdateStatus("Restored the asset before the last generation. Save writes this undo to disk.");
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
    // Never interpret an uncommitted generator preview as an empty authored asset.
    if(m_generationPreview)
    {
        if(m_templateDialog)return m_templateDialog->ApplyPreview(true);
        UpdateStatus("Apply the generated candidate before saving .phys.");return false;
    }
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
    if(m_settingsDialog) m_settingsDialog->Cancel();
    if(m_templateDialog) m_templateDialog->Cancel();
    ClearGenerationPreview();
    m_generation = ragtemplate::Transaction{};
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

#endif

static bool BuildReferenceBoneMatrices(studiohdr_t *hdr, std::vector<matrix3x4> &out)
{
    if(!hdr || hdr->numbones <= 0 || hdr->numbones > MAXSTUDIOBONES) return false;
    auto *bones = reinterpret_cast<mstudiobone_t *>(reinterpret_cast<byte *>(hdr) + hdr->boneindex);
    out.resize(hdr->numbones);
    std::vector<byte> state(hdr->numbones, 0);
    std::function<bool(int)> buildBone = [&](int i) -> bool
    {
        if(i < 0 || i >= hdr->numbones) return false;
        if(bones[i].parent < -1) return false;
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

bool PhysicsAssetEditor::RebuildPreview()
{
    const physicsasset::AssetDef &previewAsset = ActivePreviewAsset();
#ifdef PHYSICS_ASSET_EDITOR_PHYSX
    std::string error;
    if(m_generationPreview ? !physicsasset::Validate(previewAsset,&error) : !Validate(error)) { UpdateStatus(error.c_str()); return false; }
    if(!m_preview->Initialize()) { UpdateStatus("Failed to initialize PhysX preview."); return false; }
    m_preview->ClearScene();

    PxSceneDesc sceneDesc(m_preview->physics->getTolerancesScale());
    sceneDesc.gravity = PxVec3(0.0f, 0.0f, -800.0f);
    // Match runtime: retain PhysX's persistent contact manifold cache.
    sceneDesc.flags |= PxSceneFlag::eENABLE_CCD;
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
        pairFlags=PxPairFlag::eCONTACT_DEFAULT|PxPairFlag::eDETECT_CCD_CONTACT;
        return physicsasset::AssetPairFilter(filterData0,filterData1);
    };
    sceneDesc.filterCallback=&m_preview->contactFilter;
    m_preview->contactGroup.reset(new physicsasset::ContactGroup(previewAsset));
    m_preview->scene = m_preview->physics->createScene(sceneDesc);
    if(!m_preview->scene) { m_preview->ClearScene(); return false; }

    m_preview->ground = PxCreatePlane(*m_preview->physics, PxPlane(0, 0, 1, 0), *m_preview->material);
    if(m_preview->ground) m_preview->scene->addActor(*m_preview->ground);
    m_preview->bodies.resize(previewAsset.bodies.size(), nullptr);

    studiohdr_t *hdr = m_model.getStudioHeader();
    std::vector<matrix3x4> referenceBones;
    if(!BuildReferenceBoneMatrices(hdr, referenceBones))
    {
        UpdateStatus("Invalid studio reference skeleton; cannot build physics preview.");
        m_preview->ClearScene();
        return false;
    }
    std::vector<int> previewBoneIndices(previewAsset.bodies.size(), -1);
    std::vector<PxTransform> referenceBodyPoses(previewAsset.bodies.size(), PxTransform(PxIdentity));

    for(size_t i = 0; i < previewAsset.bodies.size(); ++i)
    {
        const auto &body = previewAsset.bodies[i];
        const int bone = FindBone(body.bone.c_str());
        if(bone < 0) { m_preview->ClearScene(); return false; }
        previewBoneIndices[i] = bone;
        referenceBodyPoses[i] = MatrixToPx(referenceBones[bone].ConcatTransforms(BodyLocalMatrix(body)));
        const matrix3x4 world = m_model.GetToolBoneTransform(bone).ConcatTransforms(BodyLocalMatrix(body));
        PxRigidDynamic *actor=nullptr;PxShape *shape=nullptr;
        if(!physicsasset::CreatePrimitiveBody(*m_preview->physics,*m_preview->material,MatrixToPx(world),previewAsset,i,1,actor,shape)) {m_preview->ClearScene();return false;}
        shape->setSimulationFilterData(physicsasset::AssetBodyFilterData(1u,i,*m_preview->contactGroup));
        shape->userData=const_cast<physicsasset::BodyContactToken *>(m_preview->contactGroup->Token(i));
        m_preview->scene->addActor(*actor);
        m_preview->bodies[i] = actor;
    }

    for(const auto &jointDef : previewAsset.joints)
    {
        const int parentIndex = physicsasset::FindBody(previewAsset, jointDef.parent.c_str());
        const int childIndex = physicsasset::FindBody(previewAsset, jointDef.child.c_str());
        if(parentIndex < 0 || childIndex < 0) { m_preview->ClearScene(); return false; }
        PxRigidDynamic *parent = m_preview->bodies[parentIndex];
        PxRigidDynamic *child = m_preview->bodies[childIndex];
        const int childBone = previewBoneIndices[childIndex];
        if(childBone < 0) { m_preview->ClearScene(); return false; }
        PxJoint *joint=physicsasset::CreateAssetJoint(*m_preview->physics,*parent,*child,referenceBodyPoses[parentIndex],referenceBodyPoses[childIndex],MatrixToPx(referenceBones[childBone]),previewAsset,jointDef);
        if(!joint) {m_preview->ClearScene();return false;}m_preview->joints.push_back(joint);
    }
    ragdolldiag::ReferenceSpan diagnosticSpan;
    std::vector<bool> isChild(previewAsset.bodies.size(),false);
    for(const auto &joint : previewAsset.joints)
    {
        const int child = physicsasset::FindBody(previewAsset,joint.child.c_str());
        if(child >= 0) isChild[child] = true;
    }
    for(size_t i = 0; i < previewBoneIndices.size(); ++i)
    {
        const Vector p = referenceBones[previewBoneIndices[i]].GetOrigin();
        diagnosticSpan.Add({p.x,p.y,p.z});
        if(!isChild[i] && m_preview->diagnosticsRootBody < 0)
        {
            m_preview->diagnosticsRootBody = static_cast<int>(i);
            m_preview->diagnosticsRootBone = previewBoneIndices[i];
        }
    }
    m_preview->diagnosticsBindSpan = diagnosticSpan.Value();
    return true;
#else
    return false;
#endif
}

void PhysicsAssetEditor::StartSimulation()
{
    const physicsasset::AssetDef &previewAsset = ActivePreviewAsset();
    if(m_simulating || previewAsset.bodies.empty()) return;
    if(!m_generationPreview && m_selectedJoint >= 0 && !ApplyJointFields()) return;
    if(!m_generationPreview && m_selectedBody >= 0 && !ApplyBodyFields()) return;
    if(!RebuildPreview())
    {
#ifndef PHYSICS_ASSET_EDITOR_PHYSX
        mxMessageBox(this, "This pxmv build does not include PhysX preview support. The asset can still be authored and saved.", APP_TITLE_STR, MX_MB_OK | MX_MB_INFORMATION);
#endif
        return;
    }
    if(m_simulationMode==SimulationMode::FullRagdoll)m_model.CaptureToolAnimationPose();
    else m_model.ReleaseToolAnimationPose();
    m_simulating = true;
    if(m_diagnosticsEnabled) BeginDiagnosticCapture();
    m_simulateButton->setLabel("Stop");
    UpdateStatus(m_simulationMode==SimulationMode::FullRagdoll
        ?"Full ragdoll preview running: unboxed bones frozen. Stop / Reset restores animation."
        :"Partial ragdoll preview running: unboxed bones animate. Defined bodies remain physical.");
}

void PhysicsAssetEditor::StopSimulation()
{
    m_model.ReleaseToolAnimationPose();
    if(!m_simulating)
    {
        m_model.ClearToolBoneOverrides();
        return;
    }
    if(m_diagnosticsEnabled)
    {
        m_diagnosticsReportDue = true;
        RecordPreviewDiagnostics();
    }
    m_simulating = false;
    m_model.SetToolPoseDiagnostics(false);
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
    const double inputFrameDelta = dt;
    dt = std::clamp(dt, 0.0f, 0.1f);
    m_preview->accumulator += dt;
    constexpr float step = static_cast<float>(physicsasset::kSimulationStepSeconds);
    int steps = 0;
    while(m_preview->accumulator >= step && steps++ < 10)
    {
        m_preview->scene->simulate(step);
        m_preview->scene->fetchResults(true);
        m_preview->accumulator -= step;
        if(m_diagnosticsEnabled)
        {
            const auto start = std::chrono::steady_clock::now();
            ragdolldiag::Snapshot sample;
            sample.stepSeconds = step;
            sample.inputFrameSeconds = std::isfinite(inputFrameDelta) ? inputFrameDelta : -1;
            sample.accumulatorSeconds = m_preview->accumulator;
            if(!std::isfinite(inputFrameDelta)) ++sample.invalidSamples;
            sample.bindSpan = m_preview->diagnosticsBindSpan;
            for(size_t i = 0; i < m_preview->bodies.size(); ++i)
                ragdolldiag::MeasureBody(m_preview->bodies[i],sample,static_cast<int>(i) == m_preview->diagnosticsRootBody);
            for(size_t i = 0; i < m_preview->joints.size(); ++i)
                ragdolldiag::MeasureJoint(m_preview->joints[i],sample,static_cast<int>(i));
            PxSimulationStatistics stats;
            m_preview->scene->getSimulationStatistics(stats);
            ragdolldiag::MeasureScene(&stats,sample);
            sample.samplingMilliseconds = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            m_preview->diagnosticsWindow.Push(sample);
            if(m_preview->diagnosticsCadence.Advance(step,0.25)) m_diagnosticsReportDue = true;
        }
    }
    SyncSimulationPoseToModel();
#endif
}

void PhysicsAssetEditor::SyncSimulationPoseToModel()
{
    const physicsasset::AssetDef &previewAsset = ActivePreviewAsset();
#ifdef PHYSICS_ASSET_EDITOR_PHYSX
    if(!m_simulating || !m_preview) return;
    m_model.ClearToolBoneOverrides();
    m_model.SetToolPhysicalRootBone(m_preview->diagnosticsRootBone);
    for(size_t i = 0; i < previewAsset.bodies.size() && i < m_preview->bodies.size(); ++i)
    {
        PxRigidDynamic *actor = m_preview->bodies[i];
        if(!actor) continue;
        const int bone = FindBone(previewAsset.bodies[i].bone.c_str());
        if(bone < 0) continue;
        const PxTransform local = MatrixToPx(BodyLocalMatrix(previewAsset.bodies[i]));
        const PxTransform boneWorld = actor->getGlobalPose() * local.getInverse();
        m_model.SetToolBoneOverride(bone, PxToMatrix(boneWorld));
    }
#endif
}

void PhysicsAssetEditor::ToggleDiagnostics()
{
#ifdef PHYSICS_ASSET_EDITOR_PHYSX
    if(m_diagnosticsEnabled && m_simulating)
    {
        m_diagnosticsReportDue = true;
        RecordPreviewDiagnostics();
    }
    m_diagnosticsEnabled = !m_diagnosticsEnabled;
    m_diagnosticsButton->setLabel(m_diagnosticsEnabled ? "Diagnostics: On" : "Diagnostics: Off");
    m_model.SetToolPoseDiagnostics(false);
    if(m_diagnosticsEnabled && m_simulating) BeginDiagnosticCapture();
    UpdateStatus(m_diagnosticsEnabled ? "F0 diagnostics enabled. Simulate to capture; Save F0 log exports measurements." :
        "F0 diagnostics disabled; captured rows are retained for Save F0 log.");
#else
    UpdateStatus("F0 live diagnostics require a PhysX-enabled pxmv build.");
#endif
}

void PhysicsAssetEditor::BeginDiagnosticCapture()
{
#ifdef PHYSICS_ASSET_EDITOR_PHYSX
    const physicsasset::AssetDef &previewAsset = ActivePreviewAsset();
    m_diagnosticsRows.clear();
    m_diagnosticsDroppedRows = 0;
    m_diagnosticsReportDue = false;
    m_lastDiagnostics = ragdolldiag::Snapshot{};
    m_preview->diagnosticsCadence.Reset();
    m_preview->diagnosticsWindow.Reset();
    m_model.SetToolPoseDiagnostics(true,m_preview->diagnosticsRootBone);
    std::ostringstream meta;
    meta.imbue(std::locale::classic());
    meta << "# PrimeXT J.10.6.10 F0 diagnostic schema 1\n# model " << m_modelPath
        << "\n# generated_preview " << (m_generationPreview ? 1 : 0)
        << "\n# source_asset " << m_assetPath << "\n# unsaved " << (m_dirty ? 1 : 0)
        << "\n# preview_step_s " << physicsasset::kSimulationStepSeconds << "; report_period_sim_s 0.25; gravity_z -800"
        << "\n# solver_position " << previewAsset.simulation.positionIterations << "; solver_velocity " << previewAsset.simulation.velocityIterations
        << "; self_collision " << physicsasset::SelfCollisionModeName(previewAsset.selfCollision) << "; overrides_in_resolved_asset"
        << "\n# root_body_index " << m_preview->diagnosticsRootBody << "; root_bone_index " << m_preview->diagnosticsRootBone
        << "\n# bind_span " << m_preview->diagnosticsBindSpan
        << "\n# max_rows 4096; peaks_between_reports; contact_counts_are_scene_wide_non_CCD"
        << "\n# angular_axis_excess_is_a_lower_bound_not_full_cone_error; units=u,u/s,rad/s,deg,ms"
        << "\n# resolved_asset follows (includes unsaved authoring edits)\n";
    std::istringstream assetText(physicsasset::Serialize(previewAsset));
    std::string line;
    while(std::getline(assetText,line)) meta << "# phys " << line << '\n';
    m_diagnosticsMetadata = meta.str();
#endif
}

void PhysicsAssetEditor::RecordPreviewDiagnostics()
{
#ifdef PHYSICS_ASSET_EDITOR_PHYSX
    if(!m_preview || m_preview->diagnosticsWindow.Empty()) return;
    // SetUpBones has already run for this draw. ClearToolBoneOverrides marks
    // old pose measurements invalid before the next simulation/render cycle.
    m_preview->diagnosticsWindow.PushPose(m_model.GetToolPoseDiagnostics());
    if(!m_diagnosticsReportDue) return;
    m_diagnosticsReportDue = false;
    m_lastDiagnostics = m_preview->diagnosticsWindow.Take();
    m_lastDiagnostics.simulationSeconds = m_preview->diagnosticsCadence.Elapsed();
    if(m_diagnosticsRows.size() < 4096) m_diagnosticsRows.push_back(ragdolldiag::CsvRow(m_lastDiagnostics));
    else ++m_diagnosticsDroppedRows;
    const auto &s = m_lastDiagnostics;
    UpdateStatus(va("F0 anchor %.4gu; omega %.3grad/s; pose body/child/ancestor %.3g/%.3g/%.3gu; sleep %u/%u; invalid %u; rows %u%s",
        s.maxAnchorSeparation,s.maxAngularSpeed,s.pose.drivenPositionDelta,s.pose.descendantPositionDelta,
        s.pose.ancestorPositionDelta,s.sleeping,s.bodies,s.invalidSamples+s.pose.invalidBones+s.pose.invalidComparisons,
        static_cast<unsigned>(m_diagnosticsRows.size()),m_diagnosticsDroppedRows ? " (capture limit reached)" : ""));
#endif
}

void PhysicsAssetEditor::SaveDiagnostics()
{
    if(m_diagnosticsRows.empty()) { UpdateStatus("No F0 samples yet. Enable Diagnostics and Simulate, then Save F0 log."); return; }
    // C11 exclusive-create mode, supported by the project's modern CRTs.
    // Never overwrite the asset or a previous diagnostic recording.
    FILE *file = nullptr;
    std::string path;
    for(unsigned run = 1; run <= 10000; ++run)
    {
        path = m_assetPath + ".f0." + std::to_string(run) + ".csv";
        errno = 0;
        file = std::fopen(path.c_str(),"wbx");
        if(file) break;
        if(errno != EEXIST) { UpdateStatus(va("Cannot create F0 log %s (error %d).",path.c_str(),errno)); return; }
    }
    if(!file) { UpdateStatus("No unused F0 log filename available."); return; }
    bool ok = true;
    auto write = [&](const std::string &text)
    { if(std::fwrite(text.data(),1,text.size(),file) != text.size()) ok = false; };
    write(m_diagnosticsMetadata);
    write(std::string(ragdolldiag::CsvHeader()) + "\n");
    for(const std::string &row : m_diagnosticsRows) write(row + "\n");
    write("# dropped_rows " + std::to_string(m_diagnosticsDroppedRows) + "\n");
    if(std::fclose(file) != 0) ok = false;
    if(!ok)
    {
        std::remove(path.c_str());
        UpdateStatus("F0 log write failed; the incomplete recording was removed.");
        return;
    }
    UpdateStatus(va("Saved F0 recording: %s",path.c_str()));
}

static raggeometry::Pose ConstraintPose(const matrix3x4 &matrix)
{
    auto v=[](const Vector &p)->physicsasset::Vec3{return {p.x,p.y,p.z};};
    return {v(matrix.GetForward()),v(matrix.GetRight()),v(matrix.GetUp()),v(matrix.GetOrigin())};
}

void PhysicsAssetEditor::DrawOverlay()
{
    const physicsasset::AssetDef &previewAsset = ActivePreviewAsset();
    // Called after the main DrawModel/SetUpBones pass, including when the
    // editor is hidden. Do not compare last-frame matrices before rendering.
    if(m_simulating && m_diagnosticsEnabled) RecordPreviewDiagnostics();
    if(!isVisible() || (previewAsset.bodies.empty() && previewAsset.collisions.empty())) return;
    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_CURRENT_BIT | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
    // Authoring wireframes must remain visible inside the solid model.
    // Save/restore state so the overlay cannot affect the floor or next frame.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDepthMask(GL_FALSE);
    glLineWidth(1.5f);

    // Root collision vertices are model-local. v3 bone-bound vertices are
    // stored in bone-local space, so preview them through the currently shown
    // Studio pose; an animated door therefore carries its green wireframe.
    glColor3f(0.55f, 1.0f, 0.25f);
    glBegin(GL_LINES);
    for(const auto &collision : previewAsset.collisions)
    {
        matrix3x4 collisionMatrix = m_model.GetToolModelTransform();
        const bool boneBound = !collision.bone.empty();
        if(boneBound)
        {
            const int bone = FindBone(collision.bone.c_str());
            if(bone < 0) continue;
            collisionMatrix = m_model.GetToolBoneTransform(bone);
        }
        auto drawVertex = [&](const physicsasset::Vec3 &v)
        {
            const Vector point = collisionMatrix.VectorTransform(Vector(v.x, v.y, v.z));
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

    std::vector<matrix3x4> bodyMatrices(previewAsset.bodies.size());
    std::vector<bool> bodyValid(previewAsset.bodies.size(), false);
    for(size_t i = 0; i < previewAsset.bodies.size(); ++i)
    {
#ifdef PHYSICS_ASSET_EDITOR_PHYSX
        if(m_simulating && m_preview && i < m_preview->bodies.size() && m_preview->bodies[i])
            bodyMatrices[i] = PxToMatrix(m_preview->bodies[i]->getGlobalPose());
        else
#endif
        {
            const int bone = FindBone(previewAsset.bodies[i].bone.c_str());
            if(bone < 0) continue;
            bodyMatrices[i] = m_model.GetToolBoneTransform(bone).ConcatTransforms(BodyLocalMatrix(previewAsset.bodies[i]));
        }
        bodyValid[i] = true;

        float matrix[16]; bodyMatrices[i].CopyToArray(matrix);
        glPushMatrix(); glMultMatrixf(matrix);
        if((int)i == m_selectedBody) glColor3f(1.0f, 0.75f, 0.1f); else glColor3f(0.15f, 0.85f, 1.0f);
        const auto &shape = previewAsset.bodies[i].shape;
        if(shape.type == physicsasset::ShapeType::Box) DrawBox(shape.halfExtents);
        else if(shape.type == physicsasset::ShapeType::Sphere) DrawSphere(shape.radius);
        else DrawCapsule(shape.radius, shape.halfHeight);
        glPopMatrix();
    }

    glColor3f(1.0f, 0.25f, 0.4f);
    glBegin(GL_LINES);
    for(const auto &joint : previewAsset.joints)
    {
        const int a = physicsasset::FindBody(previewAsset, joint.parent.c_str());
        const int b = physicsasset::FindBody(previewAsset, joint.child.c_str());
        if(a < 0 || b < 0 || !bodyValid[a] || !bodyValid[b]) continue;
        const Vector pa = bodyMatrices[a].GetOrigin();
        const Vector pb = bodyMatrices[b].GetOrigin();
        glVertex3fv(pa); glVertex3fv(pb);
    }
    glEnd();
    int guideIndex=m_selectedJoint;
    if(m_generationPreview)
    {
        guideIndex=-1;
        for(size_t i=0;i<previewAsset.joints.size();++i)
            if(previewAsset.joints[i].name==m_generationSelectedJoint && !m_generationSelectedJoint.empty())
            {guideIndex=static_cast<int>(i);break;}
    }
    if(m_showJointGuides && guideIndex>=0 && guideIndex<int(previewAsset.joints.size()))
    {
        const auto &joint=previewAsset.joints[guideIndex];
        const int a=physicsasset::FindBody(previewAsset,joint.parent.c_str()),b=physicsasset::FindBody(previewAsset,joint.child.c_str());
        std::vector<matrix3x4> reference;
        if(a>=0 && b>=0 && bodyValid[a] && bodyValid[b] && BuildReferenceBoneMatrices(m_model.getStudioHeader(),reference))
        {
            const int parentBone=FindBone(previewAsset.bodies[a].bone.c_str()),childBone=FindBone(previewAsset.bodies[b].bone.c_str());
            if(parentBone>=0 && childBone>=0)
            {
                physicsasset::ConstraintGuide guide;
                const matrix3x4 rp=reference[parentBone].ConcatTransforms(BodyLocalMatrix(previewAsset.bodies[a]));
                const matrix3x4 rc=reference[childBone].ConcatTransforms(BodyLocalMatrix(previewAsset.bodies[b]));
                if(physicsasset::BuildConstraintGuide(joint,ConstraintPose(rp),ConstraintPose(rc),ConstraintPose(reference[childBone]),ConstraintPose(bodyMatrices[a]),ConstraintPose(bodyMatrices[b]),4,guide))
                {
                    glBegin(GL_LINES);
                    for(const auto &line:guide.lines) {glColor3f(line.color.x,line.color.y,line.color.z);glVertex3f(line.from.x,line.from.y,line.from.z);glVertex3f(line.to.x,line.to.y,line.to.z);}
                    glEnd();
                }
            }
        }
    }
    glPopAttrib();
}
