// I.10.8: verify on-disk removal uses the production sidecar deletion helper.
#include "physics_asset_sidecar_removal.h"
#include "physics_asset_format.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static bool Present(const fs::path &path)
{
    std::error_code ec;
    return fs::exists(path, ec);
}

int main()
{
    std::error_code ec;
    const fs::path sandbox = fs::temp_directory_path() / "primext_i108_sidecar_test";
    fs::remove_all(sandbox, ec);
    assert(fs::create_directory(sandbox));
    const fs::path mdl = sandbox / "chassis.mdl";
    const fs::path sidecar = physicsasset::BuildAssetPath(mdl.string());
    const std::string collisionOnly = R"(physics_asset
{
    version 5
    collision "root"
    {
        type triangle_mesh
        vertex 0 0 0
        vertex 1 0 0
        vertex 0 1 0
        triangle 0 1 2
    }
})";
    { std::ofstream output(sidecar); output << collisionOnly; }
    physicsasset::AssetDef asset;
    std::string error;
    assert(physicsasset::LoadFile(sidecar.string().c_str(), asset, &error));
    assert(asset.collisions.size() == 1 && asset.bodies.empty());
    asset.collisions.clear();
    // The shared schema intentionally forbids serializing empty assets.
    assert(!physicsasset::Validate(asset, &error));
    bool removed = true;
    error.clear();
    assert(physicsasset::RemoveEmptyAssetSidecar(sidecar.string(), asset, removed, &error));
    assert(removed && !Present(sidecar));
    // A fresh asset query sees no .phys, so the caller may use legacy fallback.
    physicsasset::AssetDef reread;
    assert(!physicsasset::LoadFile(sidecar.string().c_str(), reread, &error));
    removed = true;
    assert(physicsasset::RemoveEmptyAssetSidecar(sidecar.string(), asset, removed, &error));
    assert(!removed); // already absent is an idempotent success

    // A collision-plus-ragdoll file clears and SAVES the collision in place,
    // keeping the authored bodies, not deleting its sidecar.
    physicsasset::AssetDef withBody;
    physicsasset::BodyDef body;
    body.name = "root";
    body.bone = "Bip01";
    withBody.bodies.push_back(body);
    assert(physicsasset::SaveFile(sidecar.string().c_str(), withBody, &error));
    removed = true;
    assert(!physicsasset::RemoveEmptyAssetSidecar(sidecar.string(), withBody, removed, &error));
    assert(!removed && Present(sidecar));
    physicsasset::AssetDef bodyAgain;
    assert(physicsasset::LoadFile(sidecar.string().c_str(), bodyAgain, &error));
    assert(bodyAgain.bodies.size() == 1 && bodyAgain.collisions.empty());
    // Never delete an asset with joints (even if otherwise empty/invalid).
    physicsasset::AssetDef danglingJoint;
    danglingJoint.joints.emplace_back();
    assert(!physicsasset::RemoveEmptyAssetSidecar(sidecar.string(), danglingJoint, removed, &error));
    assert(Present(sidecar));

    // Guard against removing arbitrary files or filesystem objects.
    assert(!physicsasset::RemoveEmptyAssetSidecar((sandbox / "chassis.mdl").string(), asset, removed, &error));
    assert(!physicsasset::RemoveEmptyAssetSidecar("", asset, removed, &error));
    const fs::path dir = sandbox / "directory.phys";
    assert(fs::create_directory(dir));
    assert(!physicsasset::RemoveEmptyAssetSidecar(dir.string(), asset, removed, &error));
    assert(Present(dir));
    const fs::path link = sandbox / "link.phys";
    fs::create_symlink(sidecar, link, ec);
    if(!ec)
    {
        assert(!physicsasset::RemoveEmptyAssetSidecar(link.string(), asset, removed, &error));
        assert(Present(sidecar));
        fs::remove(link);
    }
    assert(fs::remove(sidecar));
    assert(fs::remove(dir));
    assert(fs::remove(sandbox));
    std::cout << "I.10.8 empty collision sidecar deletion, intact bodies, guards and reload PASS\n";
}
