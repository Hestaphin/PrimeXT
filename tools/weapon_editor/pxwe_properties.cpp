// Copyright (C) 2026 PrimeXT contributors. GPL-3.0-or-later.
#include "pxwe_properties.h"
#include <algorithm>
#include <set>
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
    if (asset && kind == Kind::Cartridge && asset->Get("version") == "4") {
        const auto logic = asset->Get("attack_logic"), damage = asset->Get("damage_logic");
        const bool hitscan = logic == "hitscan";
        if ((hitscan && (key == "base_damage" || key == "damage_coefficient")) ||
            (!hitscan && (key == "default_damage" || key == "legacy_bullet_type")) ||
            (!hitscan && damage == "configurable_base" && key == "damage_coefficient") ||
            (!hitscan && damage == "energy_based" && key == "base_damage")) {
            auto info = Info("Damage", "Unavailable with the current Attack Logic / Damage Logic. This field is disabled rather than deleted so you can discover how other damage choices work. Change Attack Logic or Damage Logic to enable it.");
            info.readOnly = true; return info;
        }
    }
    if (key == "id") { auto p = Info("Identity", "Definition ID and filename. Read-only in this cell; choose Asset > Rename ID or F2 to rename safely. IDs use 1..63 ASCII letters, digits, _ or -. Renaming updates editor-owned references, but not external maps/saves."); p.readOnly = true; return p; }
    if (key == "version") return Info("Identity", "File format version (1–4). Version 4 lets cartridges choose Hitscan or Ballistic and uses a separate Damage Logic. Version numbers are not damage models. Changing this alone does not convert fields; use F5 to validate.", ResourceType::None, {"1", "2", "3", "4"});
    if (key == "category") return Info("Identity", "Optional format-3 category, for example pistols/compact. Up to 16 nested ID components, 255 characters total. Clearing it removes the category.");
    if (key == "sprite") return Info("Sprites", kind == Kind::Weapon ? "Default weapon HUD sprite/list, relative to the game folder (.spr or .txt). Conditional HUD assets can override it; blank uses the normal weapon HUD lookup." : "Optional format-3 ammo icon (.spr or .txt), relative to the game folder. Blank keeps the normal ammunition icon.", ResourceType::Sprite);
    if (kind == Kind::Cartridge) {
        static const std::map<std::string, std::string> physics = {
            {"chamber", "Chamber identifier, for example 9x19. Chamber is descriptive metadata today: the engine does not reject mismatched chamber names. Only allow physically compatible ammunition in your designs."},
            {"projectile", "Projectile/profile identifier, for example fmj or jhp. Used by game penetration profiles; 1..63 ASCII ID characters."},
            {"mass_grams", "Mass of one projectile in grams (0.001..100000). Determines kinetic energy; a cartridge's projectile_count does not multiply this mass."},
            {"diameter_mm", "Projectile diameter in millimetres (0.001..10000). Determines drag area and the circular area used by format-3 damage."},
            {"drag_coefficient", "Dimensionless drag coefficient (0..20). Zero disables air drag; higher values reduce projectile velocity faster."},
            {"reference_velocity_mps", "Reference speed in metres/second (0.01..10000), used for cartridge previews and initial compatibility entries. Each weapon's allowed-ammo velocity sets its actual muzzle speed."},
            {"projectile_count", "Projectiles per cartridge (1..256). Multiplied by the attack projectile count, subject to the game's total limit of 256."}
        };
        const auto f = physics.find(key); if (f != physics.end()) return Info("Cartridge physics", f->second);
        if (key == "attack_logic") return Info("Cartridge physics", "Attack Logic chooses how a fired bullet travels. Hitscan instantly traces a straight line with classic impact behavior; Ballistic simulates a moving bullet affected by gravity, drag and velocity. This setting belongs to the cartridge, not the weapon's Primary/Secondary Behavior.", ResourceType::None, {"hitscan", "ballistic"});
        if (key == "damage_logic") return Info("Damage", "Damage Logic controls how damage is calculated. Hitscan supports legacy (fixed authored Default Damage). Ballistic supports configurable_base (Base Damage scaled by retained energy) or energy_based (Damage Coefficient multiplied by current energy per projectile area). Incompatible choices are not offered; change Attack Logic first.", ResourceType::None,
            asset && asset->Get("attack_logic") == "ballistic" ? std::vector<std::string>{"configurable_base", "energy_based"} : std::vector<std::string>{"legacy"});
        if (key == "default_damage") return Info("Damage", "Hitscan Legacy only. Base HP damage of each projectile before hitgroup/armor modifiers; for example 60 means 60 base damage per successful bullet impact. Legacy Bullet Type does not replace this value with a skill-table number.");
        if (key == "legacy_bullet_type") return Info("Damage", "Hitscan Legacy only. Impact/bullet classification: 1 = pistol 9mm, 2 = MP5/SMG, 3 = .357 revolver. This affects compatible legacy effects, NOT the authored Default Damage.", ResourceType::None, {"1", "2", "3"});
        if (key == "base_damage") return Info("Damage", "Ballistic Configurable Base only. Initial projectile damage before loss of kinetic energy; damage scales by retained energy percentage, with a 5% floor for a real positive-energy hit. For example 40 at half energy gives 20 before armor/hitgroup.");
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
            {"npc_usable", "Allow this weapon in the modular NPC path (1 or 0). In Version 4, the ammo-specific RPM limits how quickly NPC firing events may produce shots, but existing monster AI animation and schedules may fire more slowly."},
            {"default_fire_rate", "Version 4 Default Fire Rate: shooting events per minute (RPM), 1..6000. 600 RPM fires every 0.1 seconds, independent of pellet count. Each allowed cartridge can override it; leaving that override blank inherits this value."},
            {"default_spread", "Version 4 Default Spread (0..1): angular variation used by firing logic, not literal degrees. Zero uses the smallest engine spread. Each allowed cartridge can override it; explicit zero is different from blank (inherit)."},
            {"fire_interval", "Older weapon format only: firing interval in seconds (0.01..60). Version 4 uses Default Fire Rate in RPM instead."},
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
            if (field == "behavior") return Info(section, "This describes what the button does, not the projectile flight path. For Version 4 use fire_loaded_cartridge, ammo_cycle or none; the loaded cartridge decides Hitscan/Ballistic.", ResourceType::None,
                asset && asset->Get("version") == "4" ? std::vector<std::string>{"fire_loaded_cartridge", "ammo_cycle", "none"} : std::vector<std::string>{"ballistic", "hitscan", "ammo_cycle", "none"});
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
        if (key == "default_fire_sound") return Info("Sounds and events", "Version 4 Default Fire Sound (.wav under sound/). For instance weapons/default.wav. When a cartridge has an ammo-specific Fire Sound it replaces this; blank ammo override inherits this sound. The same loaded cartridge applies for player and NPC shooting.", ResourceType::Sound);
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
            if (dot != std::string::npos && key.substr(dot + 1) == "fire_rate") return Info("Ammunition overrides", "Version 4 Fire Rate override in RPM, 1..6000. Leave blank to inherit Default Fire Rate. This affects only this weapon and cartridge combination; pellets do not multiply RPM.");
            if (dot != std::string::npos && key.substr(dot + 1) == "spread") return Info("Ammunition overrides", "Version 4 Spread override (0..1). Blank inherits Default Spread; entering 0 explicitly gives minimum spread. This does not modify the cartridge globally.");
            if (dot != std::string::npos && RoleType(key.substr(dot + 1)) != ResourceType::None)
                return Info("Ammunition overrides", "Format-3 resource override for this allowed cartridge. Takes precedence over conditional and default assets. Firing uses loaded ammo; reload uses selected ammo. Sound paths omit sound/; other paths are relative to the game folder.", RoleType(key.substr(dot + 1)));
        }
    }
    return Info("Other fields", "Unrecognized property. F5 reports unsupported or malformed keys; clear this value to remove it, or correct the definition before saving.");
}
std::string DetailedPropertyHelp(Kind kind, const std::string& key, const Asset* asset) {
    const auto info = DescribeProperty(kind, key, asset);
    std::string when = "Use it while tuning a weapon or cartridge for your mod.";
    std::string relates = "Validate with F5, then Save/Export (Ctrl+S) to test the result in game.";
    std::string example = "Change just this field, validate and compare the result against the previous setting.";
    std::string mistake = "Do not assume that changing a definition instantly updates a running game. Reload its assets first.";
    std::string see = "Getting Started and Property Reference in the Beginner Guide (Tools menu).";
    if (key == "id") {
        when = "Use Rename ID (F2) to change internal script identifiers; edit Display Name if only the HUD label should change.";
        relates = "Cartridge IDs are used in accepted-ammo lists, default_cartridge and ammo.<id> overrides; weapon IDs appear in manifests and filenames. Numeric runtime_id is separate.";
        example = "Rename cartridge fmj to fmj_124. All editor-owned weapon references change together; the old definition file is deleted when you Save.";
        mistake = "Changing an ID cannot automatically fix external map entities, save games or manually authored scripts. Back these up first.";
        see = "Asset > Rename ID; Display Name; Cartridges tab; Rename tutorial.";
    } else if (key == "attack_logic") {
        when = "Choose Hitscan for classic instant shooting; choose Ballistic for bullets that move over time and can drop under gravity.";
        relates = "Attack Logic belongs to the loaded CARTRIDGE. Primary/Secondary Behavior decides what pressing a button does. Damage Logic controls the damage formula.";
        example = "Hitscan + Legacy instantly traces a straight bullet; Ballistic + Configurable Base simulates a traveling projectile.";
        mistake = "Hitscan cannot use the energy-based or configurable-base damage modes; do not confuse this with automatic/semi fire modes.";
        see = "Damage Logic; Muzzle Velocity; Primary/Secondary Behavior; Getting Started.";
    } else if (key == "damage_logic") {
        when = "Choose Legacy for instant hitscan; Configurable Base for balanced ballistic damage; Energy-Based for projectile energy-based balancing.";
        relates = "Legacy uses default_damage; Configurable Base uses base_damage and retained muzzle energy; Energy-Based uses damage_coefficient, mass, diameter and impact velocity.";
        example = "Base Damage 40 at 50% retained energy causes 20 base damage before armor and hitgroup modifiers.";
        mistake = "Inactive fields are gray on purpose. Change Attack Logic to see the compatible damage choices.";
        see = "Attack Logic; Default Damage; Base Damage; Damage Coefficient.";
    } else if (key == "default_damage" || key == "base_damage") {
        when = "Adjust the starting HP damage without changing the bullet's visual model.";
        relates = key == "default_damage" ?
            "Legacy Hitscan: bullet_type classifies effects, but does NOT replace your authored damage. Armor and hitgroups can change final HP loss." :
            "Ballistic Configurable Base: impact damage is this number times remaining energy / muzzle energy (clamped to at least 5% for a valid positive-energy impact).";
        example = key == "default_damage" ?
            "60 means 60 base HP per successful projectile hit before armor/headshot modifiers; it is not an unconditional 60 HP loss." :
            "40 base damage at full energy -> 40, at half energy -> 20, at quarter energy -> 10, at 5% -> 2.";
        mistake = "Damage is per impacting projectile, not necessarily per trigger pull; multi-projectile attacks can hit several times.";
        see = "Damage Logic; Projectile Count; Primary Projectiles; Legacy Bullet Type.";
    } else if (key == "damage_coefficient") {
        when = "Use this for Energy-Based Ballistic damage, or older format-3 cartridges.";
        relates = "Energy in joules is 0.5 × kilograms × metres-per-second squared. Damage equals coefficient × energy / bullet cross-sectional area (mm²).";
        example = "Doubling speed roughly quadruples kinetic energy (before drag), so the same coefficient produces about four times damage.";
        mistake = "This is a gameplay balancing scale, not a scientifically certified wound/damage prediction. Extremely high values may be rejected.";
        see = "Damage Logic; Mass; Diameter; Reference Velocity; Fire Rate.";
    } else if (key == "legacy_bullet_type" || key == "bullet_type") {
        when = "Use for legacy hitscan classification: 1 pistol, 2 SMG/MP5, 3 .357 revolver.";
        relates = "The Version-4 legacy cartridge's default_damage is authoritative. This selector provides compatible impact classification and legacy behavior.";
        example = "Choose type 2 with default_damage 60 for an SMG-style impact using 60 authored base HP damage.";
        mistake = "Bullet Type does not automatically change projectile diameter, mass, cartridge chamber or authored damage.";
        see = "Attack Logic; Legacy Damage; Chamber; Projectile.";
    } else if (key == "default_fire_rate" || key.find(".fire_rate") != std::string::npos) {
        when = "Set how quickly trigger events can happen, as rounds/events per minute (RPM).";
        relates = "Specific Cartridges-tab ammo.fire_rate overrides take priority; an empty override inherits the weapon's default_fire_rate.";
        example = "600 RPM = 60/600 = 0.10 s between events; 300 RPM = 0.20 s; 900 RPM ≈ 0.067 s.";
        mistake = "RPM counts firing EVENTS. One event may release multiple pellets/projectiles; this is not necessarily the number of individual bullets.";
        see = "Cartridges tab; Default Spread; Ammo Overrides; Fire Mode.";
    } else if (key == "default_spread" || key == "spread" || key.find(".spread") != std::string::npos) {
        when = "Increase spread for less precise firing; use zero to request the minimum engine variation.";
        relates = "The loaded cartridge's custom spread beats default_spread. A blank override means inherit; an explicit 0 remains a custom override.";
        example = "With default_spread 0.02 and ammo.special.spread 0, ordinary ammo inherits 0.02 while special ammo uses 0.";
        mistake = "The numeric setting is NOT an angle in degrees. Do not treat blank and zero as the same override.";
        see = "Cartridges tab; Fire Rate; Recoil; Effective Values guide.";
    } else if (key == "default_fire_sound" || key == "fire_sound" || key.find(".fire_sound") != std::string::npos) {
        when = "Choose a .wav sound that plays when a shot is fired; preview it in the Cartridges dialog.";
        relates = "Sound paths are relative to game_folder/sound/, for example weapons/example.wav. Custom ammo fire sounds override the weapon default.";
        example = "Default weapons/normal.wav; suppressed ammo uses ammo.special.fire_sound = weapons/quiet.wav.";
        mistake = "A gunshot sound is different from impact audio when a bullet strikes concrete or metal. Do not include the leading sound/ folder.";
        see = "Cartridges tab; Model / Animations; Reload Sound; Impact Profiles.";
    } else if (key == "reference_velocity_mps" || key.find(".velocity") != std::string::npos) {
        when = "Tune bullet speed for simulated flight and previews; speed is in metres per second (m/s).";
        relates = "Actual muzzle velocity for each allowed cartridge is stored on the WEAPON's Cartridges tab, which takes precedence over the cartridge reference value.";
        example = "A weapon can fire the same compatible cartridge at 360 m/s while another firearm launches it at 400 m/s.";
        mistake = "A Hitscan attack is an instant line trace; velocity does not make its legacy damage decay with distance.";
        see = "Attack Logic; Mass; Drag Coefficient; Cartridges tab.";
    } else if (key == "mass_grams" || key == "diameter_mm" || key == "drag_coefficient") {
        when = "Tune ballistic behavior and Energy-Based damage for a projectile design.";
        relates = "Mass is grams, diameter is millimetres, and drag coefficient controls air resistance. Ballistic flight and damage may use them together.";
        example = "For example, a 8 g projectile is 0.008 kg; if moving at 400 m/s it carries 640 joules of kinetic energy.";
        mistake = "Do not enter kilograms in Mass (grams), or treat Drag Coefficient as a direct HP damage multiplier.";
        see = "Attack Logic; Damage Logic; Reference Velocity; Ballistics tab.";
    } else if (key == "default_cartridge" || key == "ammo_name") {
        when = "Use the Cartridges tab to choose which ammunition is accepted and initially loaded.";
        relates = "default_cartridge must match an accepted cartridge ID. ammo_name identifies the ammo inventory pool; do not change it blindly if the weapon uses custom reserve logic.";
        example = "Allow hitscan9 and ballistic9, then select hitscan9 as the default loadout.";
        mistake = "Renaming a cartridge ID is different from typing a new cartridge that is not in the manifest; use Rename ID to preserve references.";
        see = "Cartridges tab; Rename ID; Accepted Ammo; Ammo Overrides.";
    } else if (key == "runtime_id") {
        when = "Use only when allocating or remapping modular weapon slots for an engine build.";
        relates = "Numeric runtime IDs identify weapon instances to game/server/client systems, independently of text script ID.";
        example = "Renaming weapon smg to smg_gold leaves its runtime_id 32 unchanged.";
        mistake = "Changing runtime_id may affect network state or saved games; Rename ID does NOT change this number.";
        see = "Weapon ID; Display Name; HUD Slot.";
    } else if (key.find("_behavior") != std::string::npos || key.find("_mode") != std::string::npos) {
        when = "Choose what a trigger/button does and whether holding it repeats the action.";
        relates = "In V4 use fire_loaded_cartridge to fire the loaded ammo; Attack Logic belongs to the ammo and determines how that shot travels.";
        example = "Primary: fire_loaded_cartridge / auto; Secondary: ammo_cycle / semi.";
        mistake = "Behavior is not the same as Hitscan/Ballistic Attack Logic; semi/auto/burst describes input repetition.";
        see = "Attack Logic; Default Fire Rate; Burst Count; Secondary Action.";
    } else if (key.compare(0, 6, "asset.") == 0 || key.compare(0, 5, "ammo.") == 0) {
        when = "Override a default model, sound or other asset for a particular ammo or condition.";
        relates = "A key ammo.<cartridge_id>.<property> is owned by one weapon and one cartridge; a conditional asset can use asset.<variant>.when = ammo:<id>.";
        example = "ammo.special.view_model replaces the general view model only when special is loaded.";
        mistake = "Conditional assets are NOT automatically renamed by typing a different arbitrary string; use Rename ID on a referenced cartridge.";
        see = "Cartridges tab; Model / Animations; Asset Variants; Rename ID.";
    } else if (key == "view_model" || key == "player_model" || key == "world_model") {
        when = "Choose the view-in-hands, held-by-others or dropped-on-ground model independently.";
        relates = "Paths are relative to the mod root and point to actual Studio .mdl files. Model viewer Preview Scale is display-only.";
        example = "models/v_example.mdl for first person, p_example.mdl in other players' hands, w_example.mdl on the floor.";
        mistake = "Renaming a weapon definition ID does NOT automatically rename these independent .mdl files.";
        see = "Model / Animations tab; First-Person View; Preview Scale.";
    } else if (key == "category") {
        when = "Organize files into editor folders without changing the asset's ID.";
        relates = "Category paths contain up to 16 components. The Assets tree also supports explicit empty categories.";
        example = "rifles/modern groups related weapons in the editor.";
        mistake = "A category move does not change the weapon's resource path or rename any .mdl.";
        see = "Tree context menu; Rename category; Rename ID.";
    } else if (key == "version") {
        when = "Identify which definition schema the parser expects. New custom logic is in Version 4.";
        relates = "Version numbers select file layouts; they do not select a damage model independently.";
        example = "Version 4 Cartridge uses attack_logic and damage_logic; Version 4 Weapon uses default_fire_rate.";
        mistake = "Changing only the version digit does not automatically convert missing or incompatible fields.";
        see = "Attack Logic; Damage Logic; Getting Started; F5 Validate.";
    } else if (key == "clip_size" || key == "max_carry" || key == "default_ammo") {
        when = "Balance ammunition availability, starting reserve and magazine size.";
        relates = "Clip Size controls rounds immediately loaded; Default Ammo is starting pickup ammo; Max Carry is reserve capacity.";
        example = "A 30-round magazine with 90 reserve rounds is different from a weapon with 90 in its magazine.";
        mistake = "Do not confuse number of rounds with the number of projectiles produced per shot.";
        see = "Cartridges tab; Projectile Count; Fire Mode.";
    } else if (key == "slot" || key == "position" || key == "weight" || key == "flags") {
        when = "Use these properties to organize weapon selection and determine game inventory behavior.";
        relates = "HUD Slot (0–4) groups guns under keys 1–5; Position (0–9) orders them; Weight (0–100) affects selection priority; Flags combines bit options, such as 2 = no auto reload.";
        example = "Slot 1, Position 0 places a gun in the second HUD column at its first position. A weight of 20 ranks lower than weight 70.";
        mistake = "Flags is a bitmask: adding flag values enables multiple rules. Do not assume the Weight number changes bullet damage.";
        see = "Weapon inventory; Clip Size; Default Ammo; HUD sprites.";
    } else if (key == "reload_seconds" || key == "range" || key == "idle_seconds") {
        when = "Tune how long an action takes or how far a fired shot may travel.";
        relates = "Reload Seconds and Idle Seconds are time measured in seconds; Range is world distance in engine units. These do not override cartridge Attack Logic.";
        example = "Reload Seconds 1.5 requests a 1.5-second reload; a shorter Range prevents very distant impacts.";
        mistake = "An animation may have its own length. The timing setting and model frames should be checked together in game.";
        see = "Fire Rate; Model / Animations; Attack Logic; Clip Size.";
    } else if (key == "npc_usable" || key == "muzzle_flash" || key == "fire_underwater" || key == "auto_reload") {
        when = "Enable or disable a gameplay permission or effect for this weapon.";
        relates = "NPC Usable affects modular NPC use; Muzzle Flash is the firing visual; Fire Underwater gates firing when submerged; Auto Reload allows automatic reloading when the clip empties.";
        example = "Set fire_underwater to false for a weapon that cannot shoot when fully submerged; set muzzle_flash false for a visually muted muzzle effect.";
        mistake = "NPC Usable does not guarantee an NPC will shoot: AI schedules and ammunition still determine what happens.";
        see = "Primary Behavior; Reload Seconds; NPC AI; Model / Animations.";
    } else if (key == "projectile_count" || key.find("_projectiles") != std::string::npos || key.find("_ammo_cost") != std::string::npos || key.find("_burst_count") != std::string::npos) {
        when = "Design shotgun pellets, bursts or special attacks that consume multiple rounds.";
        relates = "A weapon's primary/secondary projectile count multiplies the cartridge projectile_count, subject to the engine safety cap. Ammo Cost counts cartridges spent, Burst Count schedules repeated firing events.";
        example = "A cartridge producing 4 pellets and an action producing 2 projectiles requests 8 projectiles per event; not all are guaranteed to hit.";
        mistake = "Firing 8 pellets does NOT mean spending 8 cartridges unless Ammo Cost or other game rules say so.";
        see = "Fire Rate; Fire Mode; Clip Size; Projectile Count.";
    } else if (key.find("_recoil_") != std::string::npos) {
        when = "Adjust how much aiming motion the player experiences after each weapon action.";
        relates = "Pitch describes vertical rotation, Yaw horizontal. Both are signed numeric degrees and are separate from random Spread.";
        example = "A recoil pitch magnitude of 3 produces a stronger vertical reaction than a magnitude of 1, subject to engine conventions.";
        mistake = "Do not assume recoil changes the bullet damage formula; Spread and Recoil perform different jobs.";
        see = "Default Spread; Fire Mode; Primary/Secondary Behavior.";
    } else if (key == "reload_sound" || key == "pickup_sound" || key.find("_sound") != std::string::npos) {
        when = "Select optional WAV audio for reloading, picking up or performing a particular attack.";
        relates = "Sound values are paths under the game's sound/ folder. Attack sounds, conditional sound variants and cartridge overrides can use different precedence depending on format.";
        example = "weapons/reload.wav means the file at game_folder/sound/weapons/reload.wav.";
        mistake = "Do not prepend sound/ to a sound path or substitute a muzzle shot sound for an impact sound.";
        see = "Default Fire Sound; Ammo Overrides; Model / Animations.";
    } else if (key == "sprite" || key.find("_event") != std::string::npos || key.find("_sprite") != std::string::npos) {
        when = "Select a HUD sprite, death icon or engine event resource.";
        relates = "A .spr sprite is visual HUD artwork; an .sc event script can control effects. Paths are relative to the game folder and are separate from Studio .mdl models.";
        example = "A weapon may use sprites/custom_hud.spr while its view model remains models/v_custom.mdl.";
        mistake = "Changing a sprite doesn't replace the first-person weapon model; changing an event doesn't change projectile Damage Logic.";
        see = "Sprites tab; Model / Animations; Conditional Assets.";
    } else if (key == "fire_interval" || key.find("_interval") != std::string::npos) {
        when = "Interpret older-format time-based firing settings, if importing a pre-Version-4 project.";
        relates = "The older interval is in seconds. New Version 4 definitions instead use RPM: seconds = 60 / fire_rate.";
        example = "An old 0.1 second interval is equivalent to 600 RPM in the new field.";
        mistake = "Do not put fire_interval or primary_interval in a new Version 4 file and expect them to override default_fire_rate.";
        see = "Default Fire Rate; Cartridges tab; Legacy formats.";
    } else if (key.find("animation") != std::string::npos) {
        when = "Select which Studio model sequence plays during the corresponding action.";
        relates = "The first-person .mdl must contain this animation index. Preview its sequences in Model / Animations.";
        example = "A reload animation can play when reloading, separately from fire and draw animations.";
        mistake = "Previewing at 2x speed does not edit the original model sequence or the game's behavior.";
        see = "View Model; Model Viewer; Reload Seconds.";
    }
    if (info.readOnly) {
        when = "This field is currently disabled. Choose the compatible Attack Logic / Damage Logic or use the dedicated Rename ID action.";
        mistake += " Disabled fields are shown to explain prerequisites; changing them directly is not supported.";
    }
    std::string message = "PROPERTY: " + key + "\r\n\r\n";
    message += "WHAT IS IT?\r\n" + info.description + "\r\n\r\n";
    message += "WHEN SHOULD I USE IT?\r\n" + when + "\r\n\r\n";
    message += "RELATED SETTINGS / IN-GAME RESULT\r\n" + relates + "\r\n\r\n";
    message += "WORKED EXAMPLE (ILLUSTRATIVE)\r\n" + example + "\r\n\r\n";
    message += "COMMON MISTAKE / IMPORTANT LIMITATION\r\n" + mistake + "\r\n\r\n";
    message += "SEE ALSO\r\n" + see;
    return message;
}

std::vector<PropertyRow> PropertyRows(const Asset& asset, const std::set<std::string>& collapsed) {
    auto keys = FieldNames(asset.kind);
    const bool v4 = asset.Get("version") == "4";
    if (v4) {
        if (asset.kind == Kind::Weapon) {
            const std::set<std::string> old = {"fire_interval", "spread", "fire_sound", "bullet_type", "primary_interval", "secondary_interval", "primary_spread", "secondary_spread", "primary_sound", "secondary_sound"};
            keys.erase(std::remove_if(keys.begin(), keys.end(), [&](const auto& k){ return old.count(k) != 0; }), keys.end());
            keys.push_back("default_fire_rate"); keys.push_back("default_spread"); keys.push_back("default_fire_sound");
        } else {
            keys.erase(std::remove(keys.begin(), keys.end(), "damage"), keys.end());
            for (const char* k : {"attack_logic", "damage_logic", "default_damage", "legacy_bullet_type", "base_damage"}) keys.emplace_back(k);
        }
    } else {
        const std::set<std::string> newer = {"attack_logic", "damage_logic", "default_damage", "legacy_bullet_type", "base_damage", "default_fire_rate", "default_spread", "default_fire_sound"};
        keys.erase(std::remove_if(keys.begin(), keys.end(), [&](const auto& k){ return newer.count(k) != 0; }), keys.end());
    }
    // Keep disabled fields visible so a beginner understands the alternatives.
    // ReadOnly and the help text below explain why they cannot be edited.
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
