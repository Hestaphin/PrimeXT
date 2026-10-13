// Shared, stateless selection. A weapon pins the seed until its next action.
#pragma once
#include "modular_weapons.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace modular {
enum class AssetAction : int { None, Deploy, Primary, Secondary, Reload, AmmoChange, Pickup };
struct AssetSelection {
    std::string cartridgeId, mode;
    uint32_t seed = 0;
    AssetAction action = AssetAction::None;
};
inline bool ValidAssetAction(int action) { return action >= 0 && action <= int(AssetAction::Pickup); }
inline uint32_t AssetHash(uint32_t hash, const std::string& text) {
    for (unsigned char c : text) { hash ^= c; hash *= 16777619u; }
    return (hash ^ 0xffu) * 16777619u;
}
inline std::string AssetFallback(const Weapon& w, const std::string& role) {
    if (role == "view_model") return w.viewModel;
    if (role == "player_model") return w.playerModel;
    if (role == "world_model") return w.worldModel;
    if (role == "reload_sound") return w.reloadSound;
    if (role == "pickup_sound") return w.pickupSound;
    if (role == "sprite" || role == "hud_sprite") return w.sprite;
    if (role == "primary_sound") return w.schemaVersion >= 4 ? w.defaultFireSound :
        (w.primary.sound.empty() ? w.fireSound : w.primary.sound);
    if (role == "secondary_sound") return w.schemaVersion >= 4 ? w.defaultFireSound :
        (w.secondary.sound.empty() ? w.fireSound : w.secondary.sound);
    if (role == "fire_sound") return w.schemaVersion >= 4 ? w.defaultFireSound : w.fireSound;
    return {};
}
inline std::string OverrideAsset(const AmmoOverride& a, const std::string& role) {
    if (role == "view_model") return a.viewModel;
    if (role == "player_model") return a.playerModel;
    if (role == "world_model") return a.worldModel;
    if (role == "reload_sound") return a.reloadSound;
    if (role == "fire_sound" || role == "primary_sound" || role == "secondary_sound") return a.fireSound;
    if (role == "sprite") return a.sprite;
    if (role == "hud_sprite") return a.hudSprite.empty() ? a.sprite : a.hudSprite;
    if (role == "kill_sprite") return a.killSprite;
    return {};
}
inline std::string SelectAsset(const Weapon& w, const AssetSelection& selection,
                               const std::string& role, const std::string& alternate = {}) {
    // Ammo override > ammo condition > mode condition > random > default > flat.
    // At each tier prefer the action-specific role over its generic counterpart.
    for (const auto& a : w.ammoOverrides) if (a.cartridgeId == selection.cartridgeId) {
        std::string path = OverrideAsset(a, role);
        if (path.empty() && !alternate.empty()) path = OverrideAsset(a, alternate);
        if (!path.empty()) return path;
    }
    const AssetCondition tiers[] = { AssetCondition::Cartridge, AssetCondition::FireMode,
                                     AssetCondition::Random, AssetCondition::Default };
    for (auto tier : tiers) for (const auto& candidateRole : {role, alternate}) {
        if (candidateRole.empty()) continue;
        std::vector<const AssetVariant*> choices;
        for (const auto& a : w.assets) {
            if (a.role != candidateRole || a.condition != tier) continue;
            if (tier == AssetCondition::Cartridge && a.conditionValue != selection.cartridgeId) continue;
            if (tier == AssetCondition::FireMode && a.conditionValue != selection.mode) continue;
            choices.push_back(&a);
        }
        if (choices.empty()) continue;
        std::sort(choices.begin(), choices.end(), [](const auto* a, const auto* b) { return a->id < b->id; });
        uint32_t hash = AssetHash(2166136261u ^ selection.seed, w.id);
        hash = AssetHash(hash, candidateRole);
        hash = AssetHash(hash, selection.cartridgeId);
        hash = AssetHash(hash, selection.mode);
        hash ^= uint32_t(selection.action) * 0x9e3779b9u;
        return choices[tier == AssetCondition::Random ? hash % choices.size() : 0]->path;
    }
    std::string path = AssetFallback(w, role);
    return path.empty() && !alternate.empty() ? AssetFallback(w, alternate) : path;
}
// Compact DeathMsg extension; indices use canonical order, never file order.
struct KillAssetReference { int kind = 0, index = 0; };
inline std::vector<const AssetVariant*> SortedAssets(const Weapon& w) {
    std::vector<const AssetVariant*> result;
    for (const auto& a : w.assets) result.push_back(&a);
    std::sort(result.begin(), result.end(), [](const auto* a, const auto* b) { return a->id < b->id; });
    return result;
}
inline std::vector<const AmmoOverride*> SortedOverrides(const Weapon& w) {
    std::vector<const AmmoOverride*> result;
    for (const auto& a : w.ammoOverrides) result.push_back(&a);
    std::sort(result.begin(), result.end(), [](const auto* a, const auto* b) { return a->cartridgeId < b->cartridgeId; });
    return result;
}
inline KillAssetReference ReferenceKillAsset(const Weapon& w, const std::string& path) {
    if (path.empty()) return {};
    const auto assets = SortedAssets(w);
    for (size_t i=0; i<assets.size(); ++i)
        if (assets[i]->role == "kill_sprite" && assets[i]->path == path) return {1, int(i)};
    const auto overrides = SortedOverrides(w);
    for (size_t i=0; i<overrides.size(); ++i)
        if (overrides[i]->killSprite == path) return {2, int(i)};
    return {};
}
inline std::string ResolveKillAsset(const Weapon& w, KillAssetReference ref) {
    if (ref.index < 0) return {};
    if (ref.kind == 1) {
        const auto assets = SortedAssets(w);
        if (ref.index < int(assets.size()) && assets[ref.index]->role == "kill_sprite") return assets[ref.index]->path;
    } else if (ref.kind == 2) {
        const auto overrides = SortedOverrides(w);
        if (ref.index < int(overrides.size())) return overrides[ref.index]->killSprite;
    }
    return {};
}
} // namespace modular
