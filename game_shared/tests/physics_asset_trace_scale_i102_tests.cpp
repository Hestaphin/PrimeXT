// I.10.2: exercise the *project's real matrix4x4 arithmetic*; no PhysX SDK needed.
#include "../physics_asset_trace_coordinates.h"
#include <cassert>
#include <cmath>
#include <iostream>

static void Near(const Vector &a, const Vector &b)
{
    assert(std::fabs(a.x - b.x) < 0.002f);
    assert(std::fabs(a.y - b.y) < 0.002f);
    assert(std::fabs(a.z - b.z) < 0.002f);
}

static void Case(const Vector &owner, const Vector &angles, float scale, float hingeYaw)
{
    const Vector s(scale, scale, scale);
    const matrix4x4 unit(owner, angles, 1.0f);
    const matrix4x4 scaled(owner, angles, s);
    const Vector boneLocal(20, 0, 0);
    const Vector v(1, 0.5f, -0.25f);
    const Vector pivot = scaled.VectorTransform(boneLocal);
    // A moving/revolute part: actor pose contains the scaled pivot but a
    // rigid (unit-basis) rotation. The client may expose a scaled bone basis.
    Vector partAngles = angles;
    partAngles.y += hingeYaw;
    const matrix4x4 physical(pivot, partAngles, 1.0f);
    const matrix4x4 animated(pivot, partAngles, s);
    const Vector expected = physical.VectorTransform(Vector(v.x*scale, v.y*scale, v.z*scale));
    physicsasset::ScaledTraceBoneFrame sv, cl;
    assert(sv.Initialize(unit, physical, s));
    assert(cl.Initialize(unit, animated, s));
    const Vector localSv = sv.ToTraceLocal(v);
    const Vector localCl = cl.ToTraceLocal(v);
    Near(localSv,localCl);
    Near(scaled.VectorTransform(localSv),expected);
    Near(scaled.VectorTransform(localCl),expected);
    // Unit-basis replicated part overrides were not scaled before I.10.3.
    physicsasset::ScaledTraceBoneFrame clientPhysical;
    assert(clientPhysical.Initialize(unit, physical, s));
    Near(scaled.VectorTransform(clientPhysical.ToTraceLocal(v)),expected);
}

int main()
{
    const Vector zero(0,0,0);
    for (float s : {0.5f, 1.f, 2.f})
    {
        Case(zero,zero,s,0.f);
        Case(Vector(10,-6,5),Vector(0,90,0),s,0.f);
        Case(Vector(-14,8,7),Vector(0,45,0),s,35.f);
        Case(Vector(4,8,11),Vector(12,25,-7),s,5.f);
    }
    // Original isolated reproduction: pivot 20, vertex 1, scale 2 -> X=42.
    physicsasset::ScaledTraceBoneFrame trace;
    const matrix4x4 world(Vector(0,0,0),Vector(0,0,0),1.0f);
    const matrix4x4 bone(Vector(40,0,0),Vector(0,0,0),1.0f);
    assert(trace.Initialize(world,bone,Vector(2,2,2)));
    const Vector meshLocal=trace.ToTraceLocal(Vector(1,0,0));
    Near(meshLocal,Vector(21,0,0));
    Near(matrix4x4(zero,zero,2.0f).VectorTransform(meshLocal),Vector(42,0,0));
    assert(!trace.Initialize(world,bone,Vector(0,1,1)));
    assert(!trace.Initialize(world,bone,Vector(-2,1,1)));
    assert(!trace.Initialize(world,bone,Vector(NAN,1,1)));
    std::cout << "I.10.2 server/client trace scale: X=42, all rotation/motion/scale cases PASS\n";
}
