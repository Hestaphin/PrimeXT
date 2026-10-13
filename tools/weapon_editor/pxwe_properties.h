// Native editor property descriptions and game-relative resource browsing.
#pragma once
#include "pxwe_project.h"
#include <set>

namespace pxwe {
enum class ResourceType { None, Model, Sound, Sprite, KillSprite, Event };
struct PropertyInfo {
    std::string section, description;
    std::vector<std::string> choices;
    ResourceType resource = ResourceType::None;
    bool readOnly = false;
};
PropertyInfo DescribeProperty(Kind kind, const std::string& key, const Asset* asset = nullptr);
// Full beginner-oriented, scrollable help for every property (including dynamic keys).
std::string DetailedPropertyHelp(Kind kind, const std::string& key, const Asset* asset = nullptr);
struct PropertyRow {
    std::string key, section;
    bool Heading() const { return key.empty(); }
};
std::vector<PropertyRow> PropertyRows(const Asset& asset, const std::set<std::string>& collapsed);
enum class PropertyPage { General, Models, Sprites };
PropertyPage PageForProperty(const Asset& asset, const std::string& key);
std::vector<PropertyRow> PageRows(const Asset& asset, const std::set<std::string>& collapsed, PropertyPage page);
fs::path BrowseDirectory(const fs::path& root, ResourceType type);
// Resolve an existing loose resource and enforce the game's portable path
// grammar. Sound values omit the sound/ prefix used on disk.
std::string ResourcePath(const fs::path& root, const fs::path& selected, ResourceType type);
} // namespace pxwe
