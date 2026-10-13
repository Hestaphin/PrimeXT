// I.10.3: scale-preserving physics-driven Studio render bone matrices.
#include "../physics_asset_render_coordinates.h"
#include <cassert>
#include <cmath>
#include <iostream>

static void NearImpl(const Vector &a, const Vector &b, const char *file, int line, float tolerance=0.003f)
{
    if(std::fabs(a.x-b.x)>tolerance || std::fabs(a.y-b.y)>tolerance || std::fabs(a.z-b.z)>tolerance)
        std::cerr << file << ":" << line << " got " << a.x << "," << a.y << "," << a.z << " vs " << b.x << "," << b.y << "," << b.z << "\n";
    assert(std::fabs(a.x-b.x) <= tolerance);
    assert(std::fabs(a.y-b.y) <= tolerance);
    assert(std::fabs(a.z-b.z) <= tolerance);
}
#define Near(a,b) NearImpl(a,b,__FILE__,__LINE__)
static void ScaleCase(float scale, const Vector &owner, const Vector &angles)
{
    const Vector zero(0,0,0), bonePivot(20,0,0), local(1,0,0);
    const matrix3x4 ownerWorld(owner,angles,scale);
    const matrix3x4 ownerUnit(owner,angles,1.0f);
    assert(std::fabs(physicsasset::StudioPhysicalRenderScale(ownerWorld)-scale)<0.001f);
    const Vector pivot=ownerWorld.VectorTransform(bonePivot);
    // Animated bones arrive with a scale-bearing rotation basis.
    const matrix3x4 animatedParent(pivot,angles,scale);
    Vector childAngles=angles;
    childAngles.y+=35.0f; // hinged child orientation differs from the owner
    const matrix3x4 animatedChild(animatedParent.VectorTransform(Vector(2,0,0)),childAngles,scale);
    const matrix3x4 physicalParent=physicsasset::StudioScaledWorldBone(pivot,
        physicsasset::StudioUnitWorldBone(animatedParent).GetQuaternion(),scale);
    Near(physicalParent.VectorTransform(local),ownerWorld.VectorTransform(Vector(21,0,0)));
    Near(physicalParent.GetOrigin(),pivot);
    const matrix3x4 relative=physicsasset::StudioAnimatedWorldRelative(animatedParent,animatedChild);
    const matrix3x4 reconstructed=physicsasset::StudioRebuildPhysicalChild(physicalParent,relative,scale);
    Near(reconstructed.GetOrigin(),animatedChild.GetOrigin());
    Near(reconstructed.VectorTransform(local),animatedChild.VectorTransform(local));
    // World-space relative positions are not scaled during reconstruction.
    const Vector rootPhysicalPos=pivot;
    const matrix3x4 rootUnit(rootPhysicalPos,physicsasset::StudioUnitWorldBone(animatedParent).GetQuaternion());
    matrix3x4 ragdollChildUnit=rootUnit.ConcatTransforms(relative);
    const matrix3x4 ragdollChild=physicsasset::StudioScaledWorldBone(ragdollChildUnit.GetOrigin(),
        ragdollChildUnit.GetQuaternion(),scale);
    Near(ragdollChild.VectorTransform(local),animatedChild.VectorTransform(local));
    // Simulated root ancestor uses the reverse (child -> parent) relation.
    const matrix3x4 ancestor=physicsasset::StudioRebuildPhysicalChild(ragdollChild,relative.Invert(),scale);
    Near(ancestor.VectorTransform(local),physicalParent.VectorTransform(local));
    // Exit interpolation: strip both scaled bases before extracting quaternion.
    matrix3x4 exitFrom=physicsasset::StudioUnitWorldBone(physicalParent);
    matrix3x4 exitTo=physicsasset::StudioUnitWorldBone(animatedParent);
    const matrix3x4 exitMid=physicsasset::StudioScaledWorldBone(
        (physicalParent.GetOrigin()+animatedParent.GetOrigin())*0.5f,
        exitFrom.GetQuaternion(),scale);
    Near(exitMid.VectorTransform(local),animatedParent.VectorTransform(local));
    Near(exitTo.VectorTransform(local),pivot + ownerUnit.VectorRotate(local)); // stripped basis is unit length
}
int main()
{
    for (float s: {0.5f,1.f,2.f})
    {
        ScaleCase(s,Vector(0,0,0),Vector(0,0,0));
        ScaleCase(s,Vector(11,-13,4),Vector(0,90,0));
        ScaleCase(s,Vector(-8,7,12),Vector(10,43,-8));
    }
    // The original I.9.6 mismatch: physical point X=41 at scale two.
    const matrix3x4 pose=physicsasset::StudioScaledWorldBone(Vector(40,0,0),Vector4D(0,0,0,1),2.f);
    Near(pose.VectorTransform(Vector(1,0,0)),Vector(42,0,0));
    std::cout << "I.10.3 render part/ragdoll/descendant/exit scale cases PASS (X=42)\n";
}
