#pragma once

#include "physics_asset_format.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace physicsasset
{
// An empty asset is not a valid serialized .phys. After an explicit UI
// confirmation, remove ONLY its sidecar, allowing the normal legacy collision
// selection to run when the model is loaded again. No .phys schema change.
// The caller must not invoke this for any authored bodies/joints/collisions.
inline bool RemoveEmptyAssetSidecar(const std::string &path, const AssetDef &asset,
    bool &removed, std::string *error = nullptr)
{
    removed = false;
    if(error) error->clear();
    if(!asset.bodies.empty() || !asset.joints.empty() || !asset.collisions.empty())
    {
        if(error) *error = "Cannot remove a .phys sidecar containing authored bodies, joints or collision geometry.";
        return false;
    }
    if(path.empty() || std::filesystem::path(path).extension() != ".phys")
    {
        if(error) *error = "Invalid .phys sidecar path; refusing to remove anything.";
        return false;
    }

    std::error_code ec;
    const std::filesystem::file_status status = std::filesystem::symlink_status(path, ec);
    if(status.type() == std::filesystem::file_type::not_found ||
       ec == std::errc::no_such_file_or_directory)
        return true; // Already absent: legacy fallback is already eligible.
    if(ec)
    {
        if(error) *error = "Cannot inspect .phys sidecar '" + path + "': " + ec.message();
        return false;
    }
    // Do not delete a directory (std::filesystem::remove permits that) or
    // follow/remove an unexpected symlink or special filesystem object.
    if(!std::filesystem::is_regular_file(status))
    {
        if(error) *error = "Refusing to remove non-regular .phys sidecar '" + path + "'.";
        return false;
    }

    removed = std::filesystem::remove(path, ec);
    if(ec)
    {
        if(error) *error = "Cannot remove .phys sidecar '" + path + "': " + ec.message();
        return false;
    }
    return true; // A concurrent removal is also harmless (removed == false).
}
}
