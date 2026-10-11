// I.10.7: exercise the exact cache-clear helper used by FreeWorld/FreePhysic.
#include "physics_asset_cache_lifecycle.h"
#include "physics_asset_format.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <cstdio>

struct FakePxMesh
{
	bool *shapeAlive;
	int releaseCount = 0;
	void release()
	{
		// Would be unsafe to drop the cache's ownership while shapes exist.
		assert(!*shapeAlive);
		++releaseCount;
	}
};

static const char *kAssetA = R"(physics_asset
{
    version 4
    collision "root" {
        type triangle_mesh
        vertex 0 0 0
        vertex 1 0 0
        vertex 0 1 0
        triangle 0 1 2
    }
})";
static const char *kAssetB = R"(physics_asset
{
    version 4
    collision "root" {
        type triangle_mesh
        vertex 0 0 0
        vertex 2 0 0
        vertex 0 1 0
        triangle 0 1 2
    }
})";

// Mirrors the production model-path lookup policy: both present and absent
// assets are retained in the map-scoped definition cache.
static std::shared_ptr<physicsasset::AssetDef> Fetch(
    std::map<std::string, std::shared_ptr<physicsasset::AssetDef>> &definitions,
    const std::string &model, const std::string &path)
{
    auto it = definitions.find(model);
    if(it != definitions.end()) return it->second;
    std::ifstream input(path);
    std::shared_ptr<physicsasset::AssetDef> result;
    if(input)
    {
        const std::string contents((std::istreambuf_iterator<char>(input)),
                                   std::istreambuf_iterator<char>());
        result = std::make_shared<physicsasset::AssetDef>();
        std::string error;
        if(!physicsasset::Parse(contents, *result, &error)) result.reset();
    }
    definitions.emplace(model, result);
    return result;
}

int main()
{
    using Definitions = std::map<std::string, std::shared_ptr<physicsasset::AssetDef>>;
    Definitions definitions;
    std::map<std::string, FakePxMesh *> triangleMeshes, convexMeshes;
    bool shapeAlive = true;
    FakePxMesh triangle{&shapeAlive};
    FakePxMesh convex{&shapeAlive};
    triangleMeshes["chassis"] = &triangle;
    triangleMeshes["missing"] = nullptr;
    convexMeshes["door"] = &convex;

    // Test per-map positive and negative lookup persistence and refresh.
    const std::string path = "i107_asset_test_fixture.phys";
    const std::string missingPath = "i107_new_asset_test_fixture.phys";
    std::remove(path.c_str());
    std::remove(missingPath.c_str());
    { std::ofstream out(path); out << kAssetA; }
    auto first = Fetch(definitions, "models/chassis.mdl", path);
    assert(first && !first->collisions.empty());
    const auto firstFingerprint = first->collisionFingerprint;
    assert(!Fetch(definitions, "models/new.mdl", missingPath));

    // Replace existing content and create the formerly absent sidecar.
    { std::ofstream out(path); out << kAssetB; }
    { std::ofstream out(missingPath); out << kAssetB; }
    assert(Fetch(definitions, "models/chassis.mdl", path)->collisionFingerprint == firstFingerprint);
    assert(!Fetch(definitions, "models/new.mdl", missingPath));

    // Simulate map actor/shape cleanup, then use production teardown helper.
    shapeAlive = false;
    physicsasset::ReleaseWorldAssetCaches(triangleMeshes, convexMeshes, definitions);
    assert(triangle.releaseCount == 1 && convex.releaseCount == 1);
    assert(triangleMeshes.empty() && convexMeshes.empty() && definitions.empty());

    auto changed = Fetch(definitions, "models/chassis.mdl", path);
    auto created = Fetch(definitions, "models/new.mdl", missingPath);
    assert(changed && changed->collisionFingerprint != firstFingerprint);
    assert(created && created->collisionFingerprint == changed->collisionFingerprint);

    // Repeated lifecycle calls must not double-release PhysX cache references.
    physicsasset::ReleaseWorldAssetCaches(triangleMeshes, convexMeshes, definitions);
    assert(triangle.releaseCount == 1 && convex.releaseCount == 1);
    assert(definitions.empty());

    // Leave no test assets on disk.
    assert(std::remove(path.c_str()) == 0);
    assert(std::remove(missingPath.c_str()) == 0);
}
