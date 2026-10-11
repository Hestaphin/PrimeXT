// Copyright (C) 2026 PrimeXT contributors. GPL-3.0-or-later.
#include "pxwe_properties.h"
#include <algorithm>
#include <stdexcept>

namespace pxwe {
namespace {
PropertyInfo Info(const std::string& section, const std::string& text,
                  ResourceType type = ResourceType::None, std::vector<std::string> choices = {}) {
    return {section, text, std::move(choices), type, false};
}
std::string Lower(std::string text) {
    for (char& c : text) if (c >= 'A' && c <= 'Z') c = char(c + ('a' - 'A'));
    return text;
}
ResourceType RoleType(const std::string& role) {
    if (role == "view_model" || role == "player_model" || role == "world_model") return ResourceType::Model;
    if (role == "fire_sound" || role == "reload_sound" || role == "pickup_sound" ||
        role == "primary_sound" || role == "secondary_sound") return ResourceType::Sound;
    if (role == "sprite" || role == "hud_sprite") return ResourceType::Sprite;
    if (role == "kill_sprite") return ResourceType::KillSprite;
    return ResourceType::None;
}
}
PropertyInfo DescribeProperty(Kind kind, const std::string& key, const Asset* asset) {
    if (key == "id") { auto p = Info("Identity", "Definition ID and filename. Read-only; Duplicate creates a new ID. IDs use 1..63 ASCII letters, digits, _ or -."); p.readOnly = true; return p; }
    if (key == "version") return Info("Identity", "File format (1, 2 or 3). Format 3 enables conditions, ammo overrides, categories and cartridge coefficients. Changing this value does not convert damage fields; F5 checks compatibility.", ResourceType::None, {"1", "2", "3"});
    if (key == "category") return Info("Identity", "Optional format-3 category, for example pistols/compact. Up to 16 nested ID components, 255 characters total. Clearing it removes the category.");
    if (key == "sprite") return Info("Sprites", kind == Kind::Weapon ? "Default weapon HUD sprite/list, relative to the game folder (.spr or .txt). Conditional HUD assets can override it; blank uses the normal weapon HUD lookup." : "Optional format-3 ammo icon (.spr or .txt), relative to the game folder. Blank keeps the normal ammunition icon.", ResourceType::Sprite);
    if (kind == Kind::Cartridge) {
        static const std::map<std::string, std::string> physics = {
            {"chamber", "Chamber identifier, for example 9x19. Accepted cartridges in a weapon must have matching chambers."},
            {"projectile", "Projectile/profile identifier, for example fmj or jhp. Used by game penetration profiles; 1..63 ASCII ID characters."},
            {"mass_grams", "Mass of one projectile in grams (0.001..100000). Determines kinetic energy; a cartridge's projectile_count does not multiply this mass."},
            {"diameter_mm", "Projectile diameter in millimetres (0.001..10000). Determines drag area and the circular area used by format-3 damage."},
            {"drag_coefficient", "Dimensionless drag coefficient (0..20). Zero disables air drag; higher values reduce projectile velocity faster."},
            {"reference_velocity_mps", "Reference speed in metres/second (0.01..10000), used for cartridge previews and initial compatibility entries. Each weapon's allowed-ammo velocity sets its actual muzzle speed."},
            {"projectile_count", "Projectiles per cartridge (1..256). Multiplied by the attack projectile count, subject to the game's total limit of 256."}
        };
        const auto f = physics.find(key); if (f != physics.end()) return Info("Cartridge physics", f->second);
        if (key == "damage") return Info("Damage", "Format-2 damage per projectile at its reference velocity (0.01..1000). Format 1 uses legacy damage; format 3 requires damage_coefficient and this field must be blank.");
        if (key == "damage_coefficient") return Info("Damage", "Format-3 coefficient k (>0, at most 1000000). Damage = k * current kinetic energy in joules / circular area in mm squared. Clear legacy damage when converting. Reference and allowed muzzle damage must be positive and at most 1000.");
    } else {
        static const std::map<std::string, std::string> identity = {
            {"display_name", "Human-readable weapon name (1..96 UTF-8 bytes). It does not rename the definition ID or its filename."},
            {"runtime_id", "Unique modular weapon number (32..62). New and duplicated weapons receive a free number; changing it affects HUD, networking and saved games."}
        };
        const auto i = identity.find(key); if (i != identity.end()) return Info("Identity", i->second);
        static const std::map<std::string, std::string> inventory = {
            {"ammo_name", "Registered cartridge ID for the weapon's ammunition pool. Set default on the Cartridges page also updates this field."},
            {"default_cartridge", "Initially loaded cartridge ID. It must be allowed on the Cartridges page; loaded rounds and the next reload selection remain separate."},
            {"clip_size", "Magazine capacity (1..255 rounds). The single fire mode requires a one-round magazine."},
            {"default_ammo", "Initial pickup ammunition (0..255 rounds). This is separate from the magazine capacity and maximum reserve."},
            {"max_carry", "Maximum reserve ammunition (1..999 rounds)."},
            {"slot", "HUD slot (0..4), corresponding to player number keys 1..5."},
            {"position", "Position within the HUD slot (0..9)."},
            {"weight", "Automatic weapon-selection priority (0..100). Higher weights have higher priority."},
            {"flags", "Item flag bitmask (0..31): 1 select when empty, 2 no auto reload, 4 no auto switch when empty, 8 limit in world, 16 exhaustible."}
        };
        const auto n = inventory.find(key); if (n != inventory.end()) return Info("Inventory and HUD", n->second);
        static const std::map<std::string, std::string> behavior = {
            {"npc_usable", "Allow this weapon in the modular NPC path (1 or 0)."},
            {"fire_interval", "Default primary firing interval in seconds (0.01..60). primary_interval overrides it."},
            {"spread", "Default primary spread (0..1). Zero gives the smallest spread; primary_spread overrides it."},
            {"reload_seconds", "Reload duration in seconds (0.01..120)."},
            {"range", "Maximum trace/projectile range in engine units (1..100000)."},
            {"bullet_type", "Legacy hitscan bullet type: 1 pistol, 2 SMG, 3 revolver. Format-3 damage uses the loaded cartridge's energy instead of the legacy damage table."},
            {"muzzle_flash", "Enable the weapon's muzzle flash (true/false or 1/0)."},
            {"fire_underwater", "Allow firing while fully underwater (true/false or 1/0)."},
            {"auto_reload", "Allow automatic reload on an empty magazine (true/false or 1/0)."}
        };
        const auto b = behavior.find(key);
        if (b != behavior.end()) {
            auto p = Info("Weapon behavior", b->second);
            if (key == "npc_usable") p.choices = {"1", "0"};
            if (key == "muzzle_flash" || key == "fire_underwater" || key == "auto_reload") p.choices = {"true", "false"};
            if (key == "bullet_type") p.choices = {"1", "2", "3"};
            return p;
        }
        if (key.compare(0, 8, "primary_") == 0 || key.compare(0, 10, "secondary_") == 0) {
            const bool primary = key.compare(0, 8, "primary_") == 0;
            const auto field = key.substr(primary ? 8 : 10);
            const auto section = primary ? "Primary attack" : "Secondary attack";
            if (field == "behavior") return Info(section, "Attack behavior: ballistic projectile, hitscan trace, ammo_cycle, or none. ammo_cycle requires semi mode and zero ammo_cost. Other active attacks require ammo_cost >= 1.", ResourceType::None, {"ballistic", "hitscan", "ammo_cycle", "none"});
            if (field == "mode") return Info(section, "Fire mode: auto repeats while held; semi fires once per press; burst fires burst_count shots; single requires clip_size = 1.", ResourceType::None, {"auto", "semi", "burst", "single"});
            static const std::map<std::string, std::string> action = {
                {"interval", "Time between attack shots in seconds (0.01..60)."},
                {"spread", "Attack-specific spread (0..1)."},
                {"projectiles", "Projectiles per attack (1..64), multiplied by cartridge projectile_count, with a total limit of 256."},
                {"ammo_cost", "Rounds consumed by each attack (0..32). ammo_cycle requires 0; other active attacks require at least 1."},
                {"burst_count", "Shots in burst mode (2..16)."},
                {"recoil_pitch", "Signed vertical recoil in degrees (-90..90)."},
                {"recoil_yaw", "Signed horizontal recoil in degrees (-90..90)."},
                {"animation", "View-model sequence index for this attack (0..255). The selected model must contain the sequence."},
                {"sound", "Attack sound (.wav), relative to sound/, for example weapons/fire.wav. Ammo overrides and conditional assets take precedence."},
                {"event", "Client event script (.sc), relative to the game folder, for example events/glock1.sc. Leave blank if unused."}
            };
            const auto a = action.find(field);
            if (a != action.end()) return Info(section, a->second, field == "sound" ? ResourceType::Sound : field == "event" ? ResourceType::Event : ResourceType::None);
        }
        static const std::map<std::string, std::string> visual = {
            {"view_model", "First-person Studio model (.mdl), relative to the game folder. New weapons start with models/v_<ID>.mdl; replace it with an existing model or create that file."},
            {"player_model", "Held third-person Studio model (.mdl), relative to the game folder. New weapons start with models/p_<ID>.mdl."},
            {"world_model", "Dropped/pickup Studio model (.mdl), relative to the game folder. New weapons start with models/w_<ID>.mdl."},
            {"fire_animation", "Fallback primary firing sequence index (0..255). primary_animation overrides it."},
            {"draw_animation", "Deployment sequence index (0..255). The selected model must contain this sequence."},
            {"reload_animation", "Reload sequence index (0..255). The selected model must contain this sequence."},
            {"idle_animation", "Idle sequence index (-1..255). -1 disables the configured idle animation."},
            {"idle_seconds", "Interval between idle animations in seconds (0.1..120)."},
            {"animation_extension", "Player animation extension ID (1..31 ASCII ID characters), for example onehanded. The player model must support it."}
        };
        const auto v = visual.find(key); if (v != visual.end()) return Info("Models and animations", v->second, RoleType(key));
        if (key == "fire_sound" || key == "reload_sound" || key == "pickup_sound")
            return Info("Sounds and events", key == "fire_sound" ? "Default firing .wav path, relative to sound/. Attack sounds and format-3 conditions/overrides may replace it." : key == "reload_sound" ? "Reload .wav path, relative to sound/. Reload uses the selected cartridge; format-3 ammo overrides take precedence." : "Pickup .wav path, relative to sound/. Blank leaves this optional sound unset.", ResourceType::Sound);
        if (key == "fire_event") return Info("Sounds and events", "Fallback primary client event script (.sc), relative to the game folder. primary_event overrides it.", ResourceType::Event);
        if (key.compare(0, 6, "asset.") == 0) {
            const auto dot = key.find('.', 6);
            if (dot != std::string::npos) {
                const auto field = key.substr(dot + 1);
                if (field == "role") return Info("Conditional assets", "Format-3 variant role. At most one explicit default per role; the role also determines the path's file type.", ResourceType::None,
                    {"view_model", "player_model", "world_model", "fire_sound", "primary_sound", "secondary_sound", "reload_sound", "pickup_sound", "sprite", "hud_sprite", "kill_sprite"});
                if (field == "path") {
                    const auto role = asset ? asset->Get(key.substr(0, dot) + ".role") : "";
                    return Info("Conditional assets", "Format-3 resource path for this variant. Models/sprites are relative to the game folder; .wav sounds are relative to sound/. Choose a valid role before browsing.", RoleType(role));
                }
                if (field == "when") return Info("Conditional assets", "Format-3 condition: default, random, mode:auto/semi/burst/single or ammo:<allowed-ID>. Ammo overrides precede ammo conditions, mode, stable random, default and flat fields.", ResourceType::None, {"default", "random", "mode:auto", "mode:semi", "mode:burst", "mode:single"});
            }
        }
        if (key.compare(0, 5, "ammo.") == 0) {
            const auto dot = key.find('.', 5);
            if (dot != std::string::npos && RoleType(key.substr(dot + 1)) != ResourceType::None)
                return Info("Ammunition overrides", "Format-3 resource override for this allowed cartridge. Takes precedence over conditional and default assets. Firing uses loaded ammo; reload uses selected ammo. Sound paths omit sound/; other paths are relative to the game folder.", RoleType(key.substr(dot + 1)));
        }
    }
    return Info("Other fields", "Unrecognized property. F5 reports unsupported or malformed keys; clear this value to remove it, or correct the definition before saving.");
}
std::vector<PropertyRow> PropertyRows(const Asset& asset, const std::set<std::string>& collapsed) {
    auto keys = FieldNames(asset.kind);
    for (const auto& field : asset.fields)
        if (std::find(keys.begin(), keys.end(), field.first) == keys.end()) keys.push_back(field.first);
    std::vector<PropertyRow> rows;
    for (const auto& section : {"Identity", "Inventory and HUD", "Cartridge physics", "Damage", "Weapon behavior",
        "Primary attack", "Secondary attack", "Models and animations", "Sounds and events", "Sprites", "Conditional assets", "Ammunition overrides", "Other fields"}) {
        bool heading = false;
        for (const auto& key : keys) if (DescribeProperty(asset.kind, key, &asset).section == section) {
            if (!heading) { rows.push_back({"", section}); heading = true; }
            if (!collapsed.count(section)) rows.push_back({key, section});
        }
    }
    return rows;
}
PropertyPage PageForProperty(const Asset& asset, const std::string& key) {
    const auto info = DescribeProperty(asset.kind, key, &asset);
    if (info.resource == ResourceType::Sprite || info.resource == ResourceType::KillSprite || key == "sprite") return PropertyPage::Sprites;
    if (key.compare(0, 6, "asset.") == 0) {
        const auto dot = key.find('.', 6);
        const auto role = dot == std::string::npos ? "" : asset.Get(key.substr(0, dot) + ".role");
        if (role == "sprite" || role == "hud_sprite" || role == "kill_sprite") return PropertyPage::Sprites;
        return PropertyPage::Models;
    }
    if (info.resource == ResourceType::Model || info.resource == ResourceType::Sound || info.resource == ResourceType::Event ||
        info.section == "Models and animations" || key == "primary_animation" || key == "secondary_animation") return PropertyPage::Models;
    return PropertyPage::General;
}
std::vector<PropertyRow> PageRows(const Asset& asset, const std::set<std::string>& collapsed, PropertyPage page) {
    // Filter fields before building headings, including collapsed sections.
    const auto all = PropertyRows(asset, {}); std::vector<PropertyRow> result;
    std::string section;
    for (const auto& row : all) {
        if (row.Heading() || PageForProperty(asset, row.key) != page) continue;
        if (section != row.section) { section = row.section; result.push_back({"", section}); }
        if (!collapsed.count(section)) result.push_back(row);
    }
    return result;
}
fs::path BrowseDirectory(const fs::path& root, ResourceType type) {
    if (type == ResourceType::Model) return root / "models";
    if (type == ResourceType::Sound) return root / "sound";
    if (type == ResourceType::Sprite || type == ResourceType::KillSprite) return root / "sprites";
    if (type == ResourceType::Event) return root / "events";
    return root;
}
std::string ResourcePath(const fs::path& root, const fs::path& selected, ResourceType type) {
    if (root.empty() || type == ResourceType::None) throw std::runtime_error("Select a resource property in an open game folder first.");
    if (!fs::is_regular_file(selected)) throw std::runtime_error("Choose an existing resource file.");
    const auto base = fs::weakly_canonical(fs::absolute(root)), target = fs::canonical(selected);
    auto r = base.begin(), t = target.begin();
    for (; r != base.end(); ++r, ++t) {
        if (t == target.end()) throw std::runtime_error("Choose a file inside the game folder.");
#ifdef _WIN32
        const bool same = Lower(r->u8string()) == Lower(t->u8string());
#else
        const bool same = *r == *t;
#endif
        if (!same) throw std::runtime_error("Choose a file inside the game folder.");
    }
    fs::path relative;
    for (; t != target.end(); ++t) relative /= *t;
    auto rel = relative.generic_u8string();
    if (type == ResourceType::Sound) {
        if (Lower(rel.substr(0, 6)) != "sound/") throw std::runtime_error("Choose a .wav file inside the game's sound folder.");
        rel.erase(0, 6);
    }
    if (rel.empty() || rel.size() > 240 || rel.find("..") != std::string::npos ||
        rel.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_./-") != std::string::npos)
        throw std::runtime_error("The game needs a relative resource path of at most 240 ASCII letters, digits, _, -, / or .; rename this resource before using it.");
    const auto ext = Lower(relative.extension().u8string());
    const bool valid = (type == ResourceType::Model && ext == ".mdl") || (type == ResourceType::Sound && ext == ".wav") ||
        (type == ResourceType::Sprite && (ext == ".spr" || ext == ".txt")) || (type == ResourceType::KillSprite && ext == ".spr") ||
        (type == ResourceType::Event && ext == ".sc");
    if (!valid) throw std::runtime_error("This file type does not match the selected resource property.");
    return rel;
}
} // namespace pxwe
