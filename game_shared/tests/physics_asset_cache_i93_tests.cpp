// I.9.3 B3: content-aware cache keys and legacy cache format upgrade.
#include "physics_asset_format.h"
#include "clipfile.h"
#include <cassert>
#include <iostream>
#include <string>

int main()
{
    const std::string first = R"(physics_asset
    {
        version 5
        collision "door" {
            type triangle_mesh
            vertex 0 0 0
            vertex 1 0 0
            vertex 0 1 0
            triangle 0 1 2
        }
    })";
    // Different physics data with the same MDL file/body/skin.
    std::string modified = first;
    auto at = modified.find("vertex 1 0 0");
    assert(at != std::string::npos);
    modified.replace(at, std::string("vertex 1 0 0").size(), "vertex 2 0 0");
    physicsasset::AssetDef a,b,c;
    std::string error;
    assert(physicsasset::Parse(first,a,&error) && error.empty());
    assert(physicsasset::Parse(modified,b,&error));
    assert(physicsasset::Parse(first,c,&error));
    assert(a.collisionFingerprint != 0);
    assert(a.collisionFingerprint == c.collisionFingerprint);
    assert(a.collisionFingerprint != b.collisionFingerprint);
    assert(clipfile::kFormatVersion == 3);  // old .clip v2 must be invalidated
    clipfile::CacheEntry entry{};
    entry.body = 0; entry.skin = 0;
    entry.geometryType = clipfile::GeometryType::CookedTriangleMesh;
    entry.sourceFingerprint = a.collisionFingerprint;
    assert(entry.sourceFingerprint != b.collisionFingerprint);
    std::cout << "I.9.3 .phys collision fingerprint/cache version PASS\n";
}
