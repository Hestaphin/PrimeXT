// I.10.3: world-space Studio bones driven by .phys must retain entity scale.
// Physical positions and networked ragdoll relative translations are already
// in world units. Only the bone's rotation basis needs the model scale.
#ifndef PHYSICS_ASSET_RENDER_COORDINATES_H
#define PHYSICS_ASSET_RENDER_COORDINATES_H

#include "matrix.h"
#include <cmath>

namespace physicsasset
{
// Animation matrices may have a scaled basis; Physics snapshots carry an
// unscaled quaternion. Never extract quaternions or call the orthonormal-only
// matrix3x4::Invert() on an animation matrix without removing its scale.
inline matrix3x4 StudioUnitWorldBone( const matrix3x4 &bone )
{
    matrix3x4 unit = bone;
    const float f = bone.GetForward().Length();
    const float r = bone.GetRight().Length();
    const float u = bone.GetUp().Length();
    if( std::isfinite(f) && f > 1e-6f ) unit.SetForward( bone.GetForward() / f );
    if( std::isfinite(r) && r > 1e-6f ) unit.SetRight( bone.GetRight() / r );
    if( std::isfinite(u) && u > 1e-6f ) unit.SetUp( bone.GetUp() / u );
    return unit;
}

// The renderer's owner transform (not a networked quaternion) is the scale
// authority, so both newly received physics poses and animation use the same
// scale even while changing animation/physics ownership.
inline float StudioPhysicalRenderScale( const matrix3x4 &entityTransform )
{
    const float scale = entityTransform.GetForward().Length();
    return std::isfinite(scale) && scale > 1e-6f ? scale : 1.0f;
}

inline matrix3x4 StudioScaledWorldBone( const Vector &worldOrigin, const Vector4D &rotation, float scale )
{
    matrix3x4 out(worldOrigin, rotation);
    out.SetForward( out.GetForward() * scale );
    out.SetRight( out.GetRight() * scale );
    out.SetUp( out.GetUp() * scale );
    // World translation was scaled before reaching us: do NOT multiply it.
    return out;
}

// World-sized relative position + relative rotation. The return transform has
// a unit basis so that it composes correctly with a unit-basis physical parent.
inline matrix3x4 StudioAnimatedWorldRelative( const matrix3x4 &animatedParent, const matrix3x4 &animatedChild )
{
    return StudioUnitWorldBone(animatedParent).Invert().ConcatTransforms(StudioUnitWorldBone(animatedChild));
}

inline matrix3x4 StudioRebuildPhysicalChild( const matrix3x4 &physicalParent, const matrix3x4 &relative, float scale )
{
    matrix3x4 child = StudioUnitWorldBone(physicalParent).ConcatTransforms(relative);
    return StudioScaledWorldBone(child.GetOrigin(), child.GetQuaternion(), scale);
}
}

#endif // PHYSICS_ASSET_RENDER_COORDINATES_H
