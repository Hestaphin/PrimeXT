// I.10.2: Shared server/client coordinate convention for bone-bound .phys
// movement-trace triangles. Vertices in the trace mesh are *unscaled entity
// local*, because TraceMesh::SetMeshOrientation applies model scale later.
#ifndef PHYSICS_ASSET_TRACE_COORDINATES_H
#define PHYSICS_ASSET_TRACE_COORDINATES_H

#include "matrix.h"
#include <cmath>

namespace physicsasset
{
struct ScaledTraceBoneFrame
{
    matrix4x4 entityFrame; // unit scale; matrix4x4::VectorITransform is NOT an inverse for scaled bases
    Vector origin, forward, right, up, scale;

    bool Initialize( const matrix4x4 &unitEntityFrame, const matrix4x4 &boneWorld, const Vector &modelScale )
    {
        if( !std::isfinite(modelScale.x) || !std::isfinite(modelScale.y) || !std::isfinite(modelScale.z) ||
            modelScale.x <= 0.0f || modelScale.y <= 0.0f || modelScale.z <= 0.0f ) return false;

        Vector f = boneWorld.GetForward();
        Vector r = boneWorld.GetRight();
        Vector u = boneWorld.GetUp();
        const float fl = f.Length(), rl = r.Length(), ul = u.Length();
        if( !std::isfinite(fl) || !std::isfinite(rl) || !std::isfinite(ul) ||
            fl <= 0.0f || rl <= 0.0f || ul <= 0.0f ) return false;

        // Client animation bones already contain entity scale in their basis,
        // whereas replicated/physical bones can have a unit-length basis.
        // Strip either basis scale before applying model scale exactly once.
        entityFrame = unitEntityFrame;
        origin = boneWorld.GetOrigin();
        forward = f / fl;
        right = r / rl;
        up = u / ul;
        scale = modelScale;
        return std::isfinite(origin.x) && std::isfinite(origin.y) && std::isfinite(origin.z);
    }

    Vector ToTraceLocal( const Vector &boneLocal ) const
    {
        const Vector world = origin + forward * (boneLocal.x * scale.x) +
            right * (boneLocal.y * scale.y) + up * (boneLocal.z * scale.z);
        const Vector localScaled = entityFrame.VectorITransform(world);
        // Divide AFTER unit-basis inverse. VectorITransform on a scaled
        // matrix would multiply by scale, not divide by it.
        return Vector(localScaled.x / scale.x, localScaled.y / scale.y, localScaled.z / scale.z);
    }
};
}

#endif // PHYSICS_ASSET_TRACE_COORDINATES_H
