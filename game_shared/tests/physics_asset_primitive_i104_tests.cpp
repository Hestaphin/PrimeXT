// I.10.4 primitive-only hinge trace geometry: PhysX SDK independent.
#include "../physics_asset_primitive_trace.h"
#include "../physics_asset_trace_coordinates.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

static bool Near(float a,float b,float eps=0.01f) { return std::fabs(a-b)<eps; }
static void CheckShape(physicsasset::ShapeDef shape, float x, float y, float z)
{
    std::vector<physicsasset::PrimitiveTriangle> triangles;
    physicsasset::BuildPrimitiveTraceTriangles(shape,triangles);
    assert(!triangles.empty() && triangles.size()<512);
    float lo[3]={1e10f,1e10f,1e10f}, hi[3]={-1e10f,-1e10f,-1e10f};
    for(const auto &tri : triangles)
        for(const auto &v : tri)
        {
            const float c[3]={v.x,v.y,v.z};
            for(int j=0;j<3;++j) {
                assert(std::isfinite(c[j]));
                lo[j]=std::min(lo[j],c[j]); hi[j]=std::max(hi[j],c[j]);
            }
        }
    assert(Near(lo[0],-x)&&Near(hi[0],x));
    assert(Near(lo[1],-y)&&Near(hi[1],y));
    assert(Near(lo[2],-z)&&Near(hi[2],z));
}

static void CheckHingeLayout()
{
    using namespace physicsasset;
    AssetDef asset;
    BodyDef door;
    door.name="door";door.bone="DoorBone";door.part="door";
    door.shape.type=ShapeType::Box;door.shape.halfExtents={2,1,4};
    door.propDrive=PropDriveMode::Animated;
    asset.bodies.push_back(door);
    asset.joints.emplace_back();asset.joints.back().kind=JointKind::Hinge;
    asset.joints.back().child="door";
    auto parts=BuildPartLayout(asset);
    assert(parts.size()==1);
    assert(PrimitiveHingeBodyIndex(asset,parts[0])==0);
    // A hinge with a mesh-backed section must not create duplicate geometry.
    parts[0].collisionIndices.push_back(0);
    assert(PrimitiveHingeBodyIndex(asset,parts[0])==-1);
    parts[0].collisionIndices.clear();
    asset.joints[0].kind=JointKind::D6;
    assert(PrimitiveHingeBodyIndex(asset,parts[0])==-1);
    asset.joints[0].kind=JointKind::Hinge;
    asset.bodies[0].name="renamed";
    assert(PrimitiveHingeBodyIndex(asset,parts[0])==-1);
    // Unrelated ragdoll body does not become an interactive primitive.
    asset.bodies[0].propDrive=PropDriveMode::None;
    asset.bodies[0].part.clear();
    assert(BuildPartLayout(asset).empty());
}

static void CheckTransforms()
{
    using namespace physicsasset;
    ShapeDef box; box.type=ShapeType::Box;box.halfExtents={2,1,4};
    std::vector<PrimitiveTriangle> triangles;
    BuildPrimitiveTraceTriangles(box,triangles);
    const Vector offset(1.0f,-3.0f,0.5f), localAngles(15,35,-7);
    const matrix4x4 shapeToBone(offset,localAngles,1.0f);
    for(float scale : {0.5f,1.0f,2.0f})
    {
        const Vector s(scale,scale,scale);
        const matrix4x4 entity(Vector(10,-5,7),Vector(0,35,0),1.0f);
        const matrix4x4 entityScaled(Vector(10,-5,7),Vector(0,35,0),s);
        const Vector pivot = entityScaled.VectorTransform(Vector(20,0,0));
        const matrix4x4 bone(pivot,Vector(0,60,0),1.0f);
        ScaledTraceBoneFrame trace;
        assert(trace.Initialize(entity,bone,s));
        for(const auto &tri : triangles)
            for(const auto &v : tri)
            {
                const Vector local=shapeToBone.VectorTransform(Vector(v.x,v.y,v.z));
                const Vector expected=bone.VectorTransform(local*scale);
                const Vector actual=entityScaled.VectorTransform(trace.ToTraceLocal(local));
                assert((expected-actual).Length()<0.003f);
            }
    }
}

int main()
{
    using namespace physicsasset;
    ShapeDef box;box.type=ShapeType::Box;box.halfExtents={2,1,4};
    CheckShape(box,2,1,4);
    ShapeDef sphere;sphere.type=ShapeType::Sphere;sphere.radius=3;
    CheckShape(sphere,3,3,3);
    ShapeDef cap;cap.type=ShapeType::Capsule;cap.radius=2;cap.halfHeight=5;
    CheckShape(cap,7,2,2);
    cap.halfHeight=0;
    CheckShape(cap,2,2,2);
    cap.radius=-1;
    std::vector<PrimitiveTriangle> invalid;
    BuildPrimitiveTraceTriangles(cap,invalid);
    assert(invalid.empty());
    CheckHingeLayout();
    CheckTransforms();
    std::cout << "I.10.4 primitive box/sphere/capsule; hinge policy; scale/rotation PASS\n";
}
