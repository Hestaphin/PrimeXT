// J.10.6.6: catch cross-platform physics interface and primitive ring regressions.
// This intentionally includes physic.h before all other engine headers.
#include "../../server/physic.h"
#include "../physics_asset_primitive_trace.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <vector>

using ResolveImpactSignature = bool (IPhysicLayer::*)(
    CBaseEntity *, const Vector &, const Vector &, Vector &, Vector &, std::uint32_t &);
static_assert(std::is_same_v<decltype(&IPhysicLayer::ResolveRenderImpact), ResolveImpactSignature>,
    "Physics impact interface must use the same fixed-width unsigned material hash on all platforms");

int main()
{
    using namespace physicsasset;
    ShapeDef sphere;
    sphere.type = ShapeType::Sphere;
    sphere.radius = 3;
    std::vector<PrimitiveTriangle> triangles;
    BuildPrimitiveTraceTriangles(sphere, triangles);
    assert(triangles.size() == 168);
    triangles.clear();

    ShapeDef capsule;
    capsule.type = ShapeType::Capsule;
    capsule.radius = 2;
    capsule.halfHeight = 5;
    BuildPrimitiveTraceTriangles(capsule, triangles);
    assert(triangles.size() == 192);
    std::cout << "J.10.6.6 physics header, override type and primitive rings PASS\n";
}
