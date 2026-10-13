// PrimeXT modular definition loader. Pure C++17; usable in the separate editor.
#include "modular_weapons.h"
#include "cartridge_damage.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <iomanip>
#include <map>
#include <locale>
#include <set>
#include <sstream>

namespace modular {
namespace {
std::string Trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \r\n\t");
    return begin == std::string::npos ? "" : s.substr(begin, s.find_last_not_of(" \r\n\t") - begin + 1);
}
bool Identifier(const std::string& s) {
    if (s.empty() || s.size() > 63) return false;
    for (char ch : s)
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-')) return false;
    return true;
}
bool RelativeAsset(const std::string& s) {
    if (s.empty() || s.size() > 240 || s[0] == '/' || s[0] == '\\' || s.find("..") != std::string::npos || s.find(':') != std::string::npos) return false;
    for (char ch : s)
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '/' || ch == '_' || ch == '-' || ch == '.')) return false;
    return true;
}
bool ParseFloat(const std::string& s, float& out, float lo, float hi) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const float value = std::strtof(s.c_str(), &end);
    if (errno == ERANGE || !end || *end != '\0' || !std::isfinite(value) || value < lo || value > hi) return false;
    out = value;
    return true;
}
bool ParseInt(const std::string& s, int& out, int lo, int hi) {
    if (s.empty()) return false;
    char* end = nullptr;
    errno = 0;
    long value = std::strtol(s.c_str(), &end, 10);
    if (errno == ERANGE || !end || *end != '\0' || value < lo || value > hi) return false;
    out = static_cast<int>(value);
    return true;
}
// No silent duplicate keys; 'accept' is the only repeatable field.
using Fields = std::map<std::string, std::string>;
struct Parsed { Fields fields; std::vector<std::string> accepted; std::string error; int line = 0; };
Parsed Parse(const std::string& input) {
    Parsed result;
    if (input.size() > 65536) { result.error = "file exceeds 64 KiB"; return result; }
    std::istringstream stream(input);
    std::string line;
    for (int n = 1; std::getline(stream, line); ++n) {
        const std::string value = Trim(line);
        if (value.empty() || value[0] == '#' || value[0] == ';') continue;
        const auto eq = value.find('=');
        if (eq == std::string::npos) { result.error = "expected key = value"; result.line = n; return result; }
        std::string key = Trim(value.substr(0, eq));
        std::string data = Trim(value.substr(eq+1));
        if (key.empty() || data.empty()) { result.error = "empty key/value"; result.line = n; return result; }
        if (key == "accept") { result.accepted.push_back(data); continue; }
        if (!result.fields.emplace(key, data).second) { result.error = "duplicate key: " + key; result.line = n; return result; }
    }
    return result;
}
}

namespace {
std::set<std::string>& BehaviorIds() {
    static std::set<std::string> ids = {"hitscan", "ballistic", "ammo_cycle", "none", "fire_loaded_cartridge"};
    return ids;
}
}
bool RegisterBehaviorId(const std::string& id) {
    return Identifier(id) && BehaviorIds().insert(id).second;
}
bool KnownBehaviorId(const std::string& id) {
    return BehaviorIds().count(id) != 0;
}

void Registry::Error(const std::string& path, int line, const std::string& message) {
    diagnostics_.push_back({path, message, line});
}

namespace {
std::string LowerAscii(std::string text) {
    for (char& c : text)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return text;
}
bool CategoryPath(const std::string& value) {
    if (value.empty()) return true;
    if (value.size()>255) return false;
    size_t begin=0; int depth=0;
    for (;;) {
        const auto end=value.find('/',begin);
        if (!Identifier(value.substr(begin,end==std::string::npos?end:end-begin)) || ++depth>16) return false;
        if (end==std::string::npos) return true;
        begin=end+1;
    }
}
using OverrideMember = std::string AmmoOverride::*;
const std::map<std::string,OverrideMember>& OverrideFields() {
    static const std::map<std::string,OverrideMember> fields={
        {"view_model",&AmmoOverride::viewModel},{"player_model",&AmmoOverride::playerModel},
        {"world_model",&AmmoOverride::worldModel},{"fire_sound",&AmmoOverride::fireSound},
        {"reload_sound",&AmmoOverride::reloadSound},{"sprite",&AmmoOverride::sprite},
        {"hud_sprite",&AmmoOverride::hudSprite},{"kill_sprite",&AmmoOverride::killSprite}};
    return fields;
}
bool VariantPath(const std::string& role,const std::string& path) {
    if (!RelativeAsset(path)) return false;
    const auto dot=path.find_last_of('.');
    const auto extension=dot==std::string::npos?std::string():LowerAscii(path.substr(dot));
    if (role=="view_model" || role=="player_model" || role=="world_model") return extension==".mdl";
    if (role=="fire_sound" || role=="reload_sound" || role=="pickup_sound" || role=="primary_sound" || role=="secondary_sound") return extension==".wav";
    if (role=="sprite" || role=="hud_sprite") return extension==".spr" || extension==".txt";
    if (role=="kill_sprite") return extension==".spr";
    return false;
}
bool ExtendedKey(const std::string& key) {
    return key.compare(0,6,"asset.")==0 || key.compare(0,5,"ammo.")==0;
}
bool ParseWeaponExtensions(const Fields& fields,Weapon& weapon,const std::set<std::string>& accepted,
                           std::string& error) {
    std::map<std::string,Fields> assets;
    std::map<std::string,AmmoOverride> overrides;
    const auto category=fields.find("category");
    if (category!=fields.end()) weapon.category=category->second;
    if (!CategoryPath(weapon.category)) { error="invalid category path"; return false; }
    for (const auto& field:fields) {
        if (!ExtendedKey(field.first)) continue;
        const bool asset=field.first.compare(0,6,"asset.")==0;
        const size_t prefix=asset?6:5, dot=field.first.find('.',prefix);
        if (dot==std::string::npos) { error="extension key needs an ID and property"; return false; }
        const auto id=field.first.substr(prefix,dot-prefix), key=field.first.substr(dot+1);
        if (!Identifier(id)) { error="invalid asset/ammunition extension ID"; return false; }
        if (asset) {
            if (key!="role" && key!="path" && key!="when") { error="unknown asset variant property"; return false; }
            assets[id][key]=field.second;
        } else {
            const auto member=OverrideFields().find(key);
            if (!accepted.count(id)) {
                error="ammunition override references a cartridge not accepted by this weapon"; return false;
            }
            auto& override=overrides[id]; override.cartridgeId=id;
            if (weapon.schemaVersion >= 4 && key == "fire_rate") {
                float rpm = 0.f;
                if (!ParseFloat(field.second, rpm, 1.f, 6000.f)) {
                    error="ammo fire_rate must be 1..6000 rounds per minute"; return false;
                }
                override.fireRateRpm = rpm;
            } else if (weapon.schemaVersion >= 4 && key == "spread") {
                float spread = 0.f;
                if (!ParseFloat(field.second, spread, 0.f, 1.f)) {
                    error="ammo spread must be 0..1"; return false;
                }
                override.spreadOverride = spread;
            } else {
                if (member==OverrideFields().end() || !VariantPath(key,field.second)) {
                    error="ammunition override needs a valid asset role/path"; return false;
                }
                override.*(member->second)=field.second;
            }
        }
    }
    if (weapon.schemaVersion<3 && (category!=fields.end() || !assets.empty() || !overrides.empty())) {
        error="categories and conditional/ammunition assets require weapon format 3"; return false;
    }
    if (assets.size()>128) { error="weapon exceeds 128 asset variants"; return false; }
    std::set<std::string> names,defaults;
    for (const auto& entry:assets) {
        if (!names.insert(LowerAscii(entry.first)).second) { error="duplicate asset variant ID (case-insensitive)"; return false; }
        const auto get=[&](const char* key) { const auto f=entry.second.find(key); return f==entry.second.end()?std::string():f->second; };
        AssetVariant variant; variant.id=entry.first; variant.role=get("role"); variant.path=get("path");
        if (!VariantPath(variant.role,variant.path)) { error="invalid variant role or asset path"; return false; }
        const auto when=get("when");
        if (when.empty() || when=="default") {
            if (!defaults.insert(variant.role).second) { error="more than one default variant for an asset role"; return false; }
        } else if (when=="random") variant.condition=AssetCondition::Random;
        else if (when.compare(0,5,"mode:")==0) {
            variant.condition=AssetCondition::FireMode; variant.conditionValue=when.substr(5);
            if (variant.conditionValue!="auto" && variant.conditionValue!="semi" && variant.conditionValue!="burst" && variant.conditionValue!="single") {
                error="unknown fire-mode condition"; return false;
            }
        } else if (when.compare(0,5,"ammo:")==0) {
            variant.condition=AssetCondition::Cartridge; variant.conditionValue=when.substr(5);
            if (!accepted.count(variant.conditionValue)) { error="variant condition references disallowed ammunition"; return false; }
        } else { error="unknown asset condition"; return false; }
        weapon.assets.push_back(std::move(variant));
    }
    for (auto& entry:overrides) weapon.ammoOverrides.push_back(std::move(entry.second));
    return true;
}
}

bool Registry::ValidateForRuntime(int ammoSlots, int usedAmmoSlots,
    const std::vector<std::string>& occupiedAmmoNames,
    const std::vector<int>& occupiedWeaponIds, std::string& error) const {
    error.clear();
    if (!valid_) { error = "invalid modular registry"; return false; }
    // J.11.2: explicit cartridge-owned shot/damage logic is supported. All
    // asset/compatibility checks still run before any runtime registration.
    if (ammoSlots <= 1 || usedAmmoSlots < 0 || usedAmmoSlots >= ammoSlots ||
        cartridges_.size() > static_cast<size_t>(ammoSlots - usedAmmoSlots - 1)) {
        error = "not enough free legacy ammunition slots"; return false;
    }
    std::set<std::string> names;
    for (const auto& name : occupiedAmmoNames) names.insert(LowerAscii(name));
    for (const auto& cartridge : cartridges_) {
        if (!names.insert(LowerAscii(cartridge.id)).second) {
            error = "cartridge duplicates an existing/other ammo name: " + cartridge.id;
            return false;
        }
    }
    std::set<int> weaponIds(occupiedWeaponIds.begin(), occupiedWeaponIds.end());
    for (const auto& weapon : weapons_) {
        if (weaponIds.count(weapon.runtimeId)) {
            error = "weapon ID already registered: " + weapon.id;
            return false;
        }
    }
    return true;
}

void Registry::DisableRuntime(const std::string& reason) {
    Error("<runtime>", 0, reason);
    valid_ = false;
    weapons_.clear();
    cartridges_.clear();
}

bool Registry::ReadManifest(const ReadFile& read, const char* path, const char* folder,
                            const char* extension, std::vector<std::string>& paths) {
    std::string input;
    if (!read(path, input)) return true; // opt-in so unmodified HL mods still work
    if (input.size() > 65536) { Error(path, 0, "manifest too large"); return false; }
    std::set<std::string> seen;
    std::istringstream stream(input);
    std::string line;
    for (int n = 1; std::getline(stream, line); ++n) {
        const auto id = Trim(line);
        if (id.empty() || id[0] == '#' || id[0] == ';') continue;
        if (!Identifier(id)) { Error(path, n, "invalid manifest asset id: " + id); continue; }
        if (!seen.insert(id).second) { Error(path, n, "duplicate manifest id: " + id); continue; }
        if (paths.size() >= 256) { Error(path, n, "manifest exceeds 256 entries"); break; }
        paths.push_back(std::string(folder) + id + extension);
    }
    return true;
}

const Cartridge* Registry::FindCartridge(const std::string& id) const {
    for (const auto& entry : cartridges_) if (entry.id == id) return &entry;
    return nullptr;
}
int Registry::AmmoReserveLimit(const std::string& cartridgeId) const {
    int limit = 0;
    for (const auto& weapon : weapons_)
        for (const auto& accept : weapon.compatible)
            if (accept.cartridgeId == cartridgeId) limit = std::max(limit, weapon.maxCarry);
    return limit;
}
const Weapon* Registry::FindWeapon(const std::string& id) const {
    for (const auto& entry : weapons_) if (entry.id == id) return &entry;
    return nullptr;
}
const Weapon* Registry::FindWeaponByRuntimeId(int id) const {
    for (const auto& entry : weapons_) if (entry.runtimeId == id) return &entry;
    return nullptr;
}

bool Registry::Load(const ReadFile& read) {
    weapons_.clear(); cartridges_.clear(); diagnostics_.clear(); valid_ = false;
    std::vector<std::string> cartridgePaths, weaponPaths;
    ReadManifest(read, "scripts/cartridges/manifest.txt", "scripts/cartridges/", ".cartridge", cartridgePaths);
    ReadManifest(read, "scripts/weapons/manifest.txt", "scripts/weapons/", ".weapon", weaponPaths);
    for (const auto& path : cartridgePaths) {
        std::string data;
        if (!read(path, data)) { Error(path, 0, "cannot read cartridge file"); continue; }
        const Parsed p = Parse(data);
        if (!p.error.empty()) { Error(path, p.line, p.error); continue; }
        Cartridge c;
        const Fields& f = p.fields;
        static const std::set<std::string> permitted = {"version", "id", "chamber", "projectile", "mass_grams", "diameter_mm", "drag_coefficient", "reference_velocity_mps", "projectile_count", "damage", "damage_coefficient", "category", "sprite", "attack_logic", "damage_logic", "default_damage", "legacy_bullet_type", "base_damage"};
        bool unknown = !p.accepted.empty();
        for (const auto& kv : f) if (!permitted.count(kv.first)) unknown = true;
        if (unknown) { Error(path, 0, "unknown or unsupported cartridge field"); continue; }
        auto field = [&f](const char* k) -> std::string { auto it = f.find(k); return it == f.end() ? "" : it->second; };
        int schema = 0;
        if (!ParseInt(field("version"), schema, 1, kCartridgeSchemaVersion) ||
            !Identifier(c.id = field("id")) || !Identifier(c.chamber = field("chamber")) ||
            !Identifier(c.projectileType = field("projectile")) ||
            !ParseFloat(field("mass_grams"), c.massGrams, 0.001f, 100000.f) ||
            !ParseFloat(field("diameter_mm"), c.diameterMm, 0.001f, 10000.f) ||
            !ParseFloat(field("drag_coefficient"), c.dragCoefficient, 0.f, 20.f) ||
            !ParseFloat(field("reference_velocity_mps"), c.referenceVelocityMps, 0.01f, 10000.f) ||
            !ParseInt(field("projectile_count"), c.projectileCount, 1, 256) ||
            (schema == 2 && !ParseFloat(field("damage"), c.damage, 0.01f, 1000.f)) ||
            (schema != 2 && !field("damage").empty()) ||
            (schema < 3 && !field("damage_coefficient").empty()) ||
            (schema == 3 && (!ParseFloat(field("damage_coefficient"), c.damageCoefficient, 0.f, 1000000.f) || c.damageCoefficient <= 0)) ||
            (schema < 4 && (!field("attack_logic").empty() || !field("damage_logic").empty() ||
                           !field("default_damage").empty() || !field("base_damage").empty() || !field("legacy_bullet_type").empty()))) {
            Error(path, 0, "invalid or missing required cartridge property"); continue;
        }
        c.schemaVersion = schema;
        c.category=field("category"); c.sprite=field("sprite");
        if (!CategoryPath(c.category) || (!c.sprite.empty() && !VariantPath("sprite",c.sprite)) ||
            (schema<3 && (!c.category.empty() || !c.sprite.empty()))) {
            Error(path,0,"invalid cartridge category/sprite or format 3 required"); continue;
        }
        if (schema == 4) {
            const std::string attack=field("attack_logic"), damage=field("damage_logic");
            if (attack=="hitscan") c.attackLogic=AttackLogic::Hitscan;
            else if (attack=="ballistic") c.attackLogic=AttackLogic::Ballistic;
            else { Error(path,0,"v4 attack_logic must be hitscan or ballistic"); continue; }
            if (damage=="legacy") c.damageLogic=DamageLogic::Legacy;
            else if (damage=="configurable_base") c.damageLogic=DamageLogic::ConfigurableBase;
            else if (damage=="energy_based") c.damageLogic=DamageLogic::EnergyBased;
            else { Error(path,0,"v4 damage_logic must be legacy, configurable_base or energy_based"); continue; }
            const bool legacy=c.attackLogic==AttackLogic::Hitscan && c.damageLogic==DamageLogic::Legacy;
            const bool base=c.attackLogic==AttackLogic::Ballistic && c.damageLogic==DamageLogic::ConfigurableBase;
            const bool energy=c.attackLogic==AttackLogic::Ballistic && c.damageLogic==DamageLogic::EnergyBased;
            if ((!legacy && !base && !energy) || !field("damage").empty() ||
                (legacy && (!ParseFloat(field("default_damage"),c.defaultDamage,0.01f,1000.f) ||
                            !ParseInt(field("legacy_bullet_type"),c.legacyBulletType,1,3))) ||
                (!legacy && (!field("default_damage").empty() || !field("legacy_bullet_type").empty())) ||
                (base && !ParseFloat(field("base_damage"),c.baseDamage,0.01f,1000.f)) ||
                (!base && !field("base_damage").empty()) ||
                (energy && (!ParseFloat(field("damage_coefficient"),c.damageCoefficient,0.000001f,1000000.f))) ||
                (!energy && !field("damage_coefficient").empty())) {
                Error(path,0,"invalid v4 attack/damage logic combination or inactive/invalid damage field"); continue;
            }
            if (energy) {
                const double muzzle=EnergyDensityDamage(KineticEnergyJ(c.massGrams,c.referenceVelocityMps),c.diameterMm,c.damageCoefficient);
                if (!std::isfinite(muzzle) || muzzle<=0 || muzzle>1000) {
                    Error(path,0,"v4 reference energy damage must be positive and at most 1000"); continue;
                }
            }
        }
        if (schema == 3) {
            const double damage = MuzzleDamage(c, c.referenceVelocityMps);
            if (!std::isfinite(damage) || damage <= 0 || damage > 1000) {
                Error(path, 0, "reference muzzle damage must be positive and at most 1000"); continue;
            }
        }
        if (path != "scripts/cartridges/" + c.id + ".cartridge") { Error(path, 0, "cartridge id must match filename"); continue; }
        if (FindCartridge(c.id)) { Error(path, 0, "duplicate cartridge id"); continue; }
        cartridges_.push_back(std::move(c));
    }
    for (const auto& path : weaponPaths) {
        std::string data;
        if (!read(path, data)) { Error(path, 0, "cannot read weapon file"); continue; }
        const Parsed p = Parse(data);
        if (!p.error.empty()) { Error(path, p.line, p.error); continue; }
        Weapon w;
        const Fields& f = p.fields;
        static const std::set<std::string> permitted = {"version", "id", "display_name", "ammo_name", "default_cartridge", "runtime_id", "clip_size", "default_ammo", "max_carry", "slot", "position", "fire_interval", "spread", "reload_seconds", "range", "bullet_type", "view_model", "player_model", "world_model", "fire_sound", "reload_sound", "pickup_sound", "sprite", "fire_event", "weight", "flags", "fire_animation", "draw_animation", "reload_animation", "primary_behavior", "primary_mode", "primary_interval", "primary_spread", "primary_projectiles", "primary_ammo_cost", "primary_burst_count", "primary_recoil_pitch", "primary_recoil_yaw", "primary_animation", "primary_sound", "primary_event", "secondary_behavior", "secondary_mode", "secondary_interval", "secondary_spread", "secondary_projectiles", "secondary_ammo_cost", "secondary_burst_count", "secondary_recoil_pitch", "secondary_recoil_yaw", "secondary_animation", "secondary_sound", "secondary_event", "muzzle_flash", "fire_underwater", "auto_reload", "idle_animation", "idle_seconds", "animation_extension", "npc_usable"};
        static const std::set<std::string> v4Only = {"default_fire_rate", "default_spread", "default_fire_sound"};
        static const std::set<std::string> deprecatedV4 = {"fire_interval", "spread", "fire_sound", "bullet_type",
            "primary_interval", "secondary_interval", "primary_spread", "secondary_spread", "primary_sound", "secondary_sound"};
        int requestedSchema=0;
        const auto sv=f.find("version");
        if (sv!=f.end()) ParseInt(sv->second,requestedSchema,1,kSchemaVersion);
        bool unknown = false;
        for (const auto& kv : f) {
            if (!permitted.count(kv.first) && !(requestedSchema==4 && v4Only.count(kv.first)) &&
                kv.first!="category" && !ExtendedKey(kv.first)) unknown=true;
            if (requestedSchema==4 && deprecatedV4.count(kv.first)) unknown=true;
        }
        if (unknown) { Error(path, 0, "unknown weapon property"); continue; }
        auto field = [&f](const char* k) -> std::string { auto it = f.find(k); return it == f.end() ? "" : it->second; };
        int schema = 0;
        if (!ParseInt(field("version"), schema, 1, kSchemaVersion) ||
            !Identifier(w.id = field("id")) || (w.displayName = field("display_name")).empty() || w.displayName.size() > 96 ||
            w.displayName.find_first_of("\r\n\0", 0, 3) != std::string::npos ||
            !Identifier(w.ammoName = field("ammo_name")) ||
            !Identifier(w.defaultCartridgeId = field("default_cartridge")) ||
            !ParseInt(field("runtime_id"), w.runtimeId, kFirstWeaponId, kLastWeaponId) ||
            !ParseInt(field("clip_size"), w.clipSize, 1, 255) ||
            !ParseInt(field("default_ammo"), w.defaultAmmo, 0, 255) ||
            !ParseInt(field("max_carry"), w.maxCarry, 1, 999) ||
            !ParseInt(field("slot"), w.slot, 0, 4) ||
            !ParseInt(field("position"), w.position, 0, 9) ||
            (schema == 4 ? (!ParseFloat(field("default_fire_rate"),w.defaultFireRateRpm,1.f,6000.f) ||
                            !ParseFloat(field("default_spread"),w.defaultSpread,0.f,1.f)) :
                           (!ParseFloat(field("fire_interval"), w.fireInterval, 0.01f, 60.f) ||
                            !ParseFloat(field("spread"), w.spread, 0.f, 1.f))) ||
            !ParseFloat(field("reload_seconds"), w.reloadSeconds, 0.01f, 120.f) ||
            !ParseFloat(field("range"), w.range, 1.f, 100000.f) ||
            (schema < 4 && !ParseInt(field("bullet_type"), w.bulletType, 1, 3))) {
            Error(path, 0, "invalid or missing required weapon property"); continue;
        }
        w.viewModel = field("view_model"); w.playerModel = field("player_model"); w.worldModel = field("world_model");
        w.schemaVersion=schema;
        w.fireSound = field("fire_sound"); w.reloadSound = field("reload_sound"); w.pickupSound = field("pickup_sound");
        if (schema == 4) {
            w.defaultFireSound=field("default_fire_sound");
            if (!w.defaultFireSound.empty() && !VariantPath("fire_sound",w.defaultFireSound)) {
                Error(path,0,"default_fire_sound must be a valid .wav path"); continue;
            }
            // Shared legacy execution fields are a temporary staging bridge,
            // but v4 is explicitly blocked from admission until J.11.2.
            w.fireInterval=static_cast<float>(SecondsPerShot(w.defaultFireRateRpm));
            w.spread=w.defaultSpread;
            w.fireSound=w.defaultFireSound;
        }
        w.sprite = field("sprite"); w.fireEvent = field("fire_event");
        if (!RelativeAsset(w.viewModel) || !RelativeAsset(w.playerModel) || !RelativeAsset(w.worldModel)) {
            Error(path, 0, "invalid model path"); continue;
        }
        bool badAsset = false;
        for (const auto& asset : {w.fireSound, w.reloadSound, w.pickupSound, w.sprite, w.fireEvent})
            if (!asset.empty() && !RelativeAsset(asset)) badAsset = true;
        if (badAsset) { Error(path, 0, "invalid resource path"); continue; }
        if (!field("weight").empty() && !ParseInt(field("weight"), w.weight, 0, 100)) badAsset = true;
        if (!field("flags").empty() && !ParseInt(field("flags"), w.flags, 0, 31)) badAsset = true;
        if (!field("fire_animation").empty() && !ParseInt(field("fire_animation"), w.fireAnimation, 0, 255)) badAsset = true;
        if (!field("draw_animation").empty() && !ParseInt(field("draw_animation"), w.drawAnimation, 0, 255)) badAsset = true;
        if (!field("reload_animation").empty() && !ParseInt(field("reload_animation"), w.reloadAnimation, 0, 255)) badAsset = true;
        if (badAsset) { Error(path, 0, "invalid optional weapon value"); continue; }
        if (!field("npc_usable").empty()) {
            if (field("npc_usable") == "1") w.npcUsable = true;
            else if (field("npc_usable") != "0") {
                Error(path, 0, "npc_usable must be 0 or 1"); continue;
            }
        }
        // v1 definitions have the same original hitscan behavior and no secondary.
        // v2 can override per-action settings while retaining the old core keys.
        w.primary.interval = w.fireInterval;
        w.primary.spread = w.spread;
        w.primary.animation = w.fireAnimation;
        w.primary.sound = w.fireSound;
        w.primary.event = w.fireEvent;
        w.secondary.behavior = "none";
        if (schema == 4) w.primary.behavior = "fire_loaded_cartridge";
        auto action = [&](const char* prefix, Attack& a) -> bool {
            const std::string key(prefix);
            const auto get = [&](const char* suffix) { return field((key + suffix).c_str()); };
            const std::string behavior = get("behavior"), mode = get("mode");
            if (!behavior.empty()) a.behavior = behavior;
            if (!mode.empty()) a.mode = mode;
            if (!KnownBehaviorId(a.behavior) || (a.mode != "auto" && a.mode != "semi" && a.mode != "burst" && a.mode != "single")) return false;
            const std::string interval = get("interval"), spread = get("spread");
            const std::string count = get("projectiles"), cost = get("ammo_cost"), burst = get("burst_count");
            const std::string pitch = get("recoil_pitch"), yaw = get("recoil_yaw"), anim = get("animation");
            if ((!interval.empty() && !ParseFloat(interval, a.interval, 0.01f, 60.f)) ||
                (!spread.empty() && !ParseFloat(spread, a.spread, 0.f, 1.f)) ||
                (!count.empty() && !ParseInt(count, a.projectiles, 1, 64)) ||
                (!cost.empty() && !ParseInt(cost, a.ammoCost, 0, 32)) ||
                (!burst.empty() && !ParseInt(burst, a.burstCount, 2, 16)) ||
                (!pitch.empty() && !ParseFloat(pitch, a.recoilPitch, -90.f, 90.f)) ||
                (!yaw.empty() && !ParseFloat(yaw, a.recoilYaw, -90.f, 90.f)) ||
                (!anim.empty() && !ParseInt(anim, a.animation, 0, 255))) return false;
            if (!get("sound").empty()) a.sound = get("sound");
            if (!get("event").empty()) a.event = get("event");
            return (a.behavior == "ammo_cycle" ? (a.ammoCost == 0 && a.mode == "semi") :
                    (a.behavior == "none" || a.ammoCost >= 1)) &&
                   (a.sound.empty() || RelativeAsset(a.sound)) &&
                   (a.event.empty() || RelativeAsset(a.event));
        };
        if (!action("primary_", w.primary) || !action("secondary_", w.secondary) ||
            (schema == 4 && (w.primary.behavior == "hitscan" || w.primary.behavior == "ballistic" ||
                             w.secondary.behavior == "hitscan" || w.secondary.behavior == "ballistic")) ||
            (w.primary.mode == "single" && w.clipSize != 1) ||
            (w.secondary.mode == "single" && w.clipSize != 1)) {
            Error(path, 0, "invalid or unregistered weapon action (single requires a one-round clip)"); continue;
        }
        auto boolean = [&](const char* name, bool& dest) -> bool {
            const std::string value = field(name);
            if (value.empty()) return true;
            if (value == "true" || value == "1") { dest = true; return true; }
            if (value == "false" || value == "0") { dest = false; return true; }
            return false;
        };
        if (!boolean("muzzle_flash", w.muzzleFlash) ||
            !boolean("fire_underwater", w.fireUnderwater) ||
            !boolean("auto_reload", w.autoReload) ||
            (!field("idle_animation").empty() && !ParseInt(field("idle_animation"), w.idleAnimation, -1, 255)) ||
            (!field("idle_seconds").empty() && !ParseFloat(field("idle_seconds"), w.idleSeconds, 0.1f, 120.f))) {
            Error(path, 0, "invalid weapon state or visual setting"); continue;
        }
        if (!field("animation_extension").empty()) w.animationExtension = field("animation_extension");
        if (!Identifier(w.animationExtension) || w.animationExtension.size() > 31) {
            Error(path, 0, "invalid animation extension"); continue;
        }
        if (path != "scripts/weapons/" + w.id + ".weapon") { Error(path, 0, "weapon id must match filename"); continue; }
        std::set<std::string> acceptedIds;
        for (const auto& accepted : p.accepted) {
            const auto colon = accepted.find(':');
            if (colon == std::string::npos) { Error(path, 0, "accept syntax must be cartridge_id:velocity_mps"); badAsset = true; break; }
            Compatibility a;
            a.cartridgeId = Trim(accepted.substr(0, colon));
            if (!Identifier(a.cartridgeId) || !FindCartridge(a.cartridgeId) || !acceptedIds.insert(a.cartridgeId).second ||
                !ParseFloat(Trim(accepted.substr(colon+1)), a.muzzleVelocityMps, 0.01f, 10000.f)) {
                Error(path, 0, "invalid/duplicate/unregistered compatible cartridge"); badAsset = true; break;
            }
            w.compatible.push_back(std::move(a));
        }
        if (badAsset) continue;
        for (const auto& entry : w.compatible) {
            const Cartridge* cartridge = FindCartridge(entry.cartridgeId);
            if (!cartridge) { badAsset = true; break; }
            if (w.schemaVersion >= 4 && cartridge->schemaVersion != 4) {
                Error(path,0,"v4 weapons may accept only v4 cartridges"); badAsset=true; break;
            }
            if (w.schemaVersion < 4 && cartridge->schemaVersion >= 4) {
                Error(path,0,"v4 cartridges require a v4 weapon"); badAsset=true; break;
            }
            const double damage = cartridge->schemaVersion == 4 ?
                (cartridge->damageLogic==DamageLogic::Legacy ? cartridge->defaultDamage :
                 cartridge->damageLogic==DamageLogic::ConfigurableBase ? cartridge->baseDamage :
                 EnergyDensityDamage(KineticEnergyJ(cartridge->massGrams,entry.muzzleVelocityMps),
                                     cartridge->diameterMm,cartridge->damageCoefficient)) :
                MuzzleDamage(*cartridge, entry.muzzleVelocityMps);
            if (!std::isfinite(damage) || damage <= 0 || damage > 1000) {
                Error(path, 0, "compatible cartridge muzzle damage must be positive and at most 1000");
                badAsset = true; break;
            }
            if (((w.primary.behavior == "ballistic" || (cartridge->schemaVersion >= 3 && w.primary.behavior == "hitscan") ||
                    (cartridge->schemaVersion >= 4 && w.primary.behavior == "fire_loaded_cartridge")) &&
                    w.primary.projectiles * cartridge->projectileCount > 256) ||
                ((w.secondary.behavior == "ballistic" || (cartridge->schemaVersion >= 3 && w.secondary.behavior == "hitscan") ||
                    (cartridge->schemaVersion >= 4 && w.secondary.behavior == "fire_loaded_cartridge")) &&
                    w.secondary.projectiles * cartridge->projectileCount > 256)) {
                Error(path,0,"cartridge action exceeds 256 projectiles per trigger");
                badAsset = true;
                break;
            }
        }
        if (badAsset) continue;
        if (w.compatible.empty() || !acceptedIds.count(w.defaultCartridgeId)) {
            Error(path, 0, "weapon needs accepted default_cartridge"); continue;
        }
        std::string extensionError;
        if (!ParseWeaponExtensions(f,w,acceptedIds,extensionError)) { Error(path,0,extensionError); continue; }
        if (FindWeapon(w.id) || FindWeaponByRuntimeId(w.runtimeId)) { Error(path, 0, "duplicate weapon id or runtime id"); continue; }
        weapons_.push_back(std::move(w));
    }
    valid_ = diagnostics_.empty();
    // Atomic failure: no partially loaded sets that could disagree across DLLs.
    if (!valid_) { weapons_.clear(); cartridges_.clear(); }
    return valid_;
}
uint32_t Registry::Fingerprint() const {
    if (!valid_) return 0;
    // FNV-1a over the versioned canonical writer output, not source whitespace.
    uint32_t hash = 2166136261u;
    auto absorb = [&hash](const std::string& bytes) {
        for (unsigned char c : bytes) { hash ^= c; hash *= 16777619u; }
        hash ^= 0xffu; hash *= 16777619u; // record boundary
    };
    absorb("PrimeXT modular manifest J4 v1");
    for (const auto& c : cartridges_) absorb(SerializeCartridge(c));
    for (const auto& w : weapons_) absorb(SerializeWeapon(w));
    return hash;
}
uint32_t Registry::RuntimeFingerprint() const {
    if (!valid_) return 0;
    bool format3 = false;
    for (const auto& c : cartridges_) format3 |= c.schemaVersion >= 3;
    for (const auto& w : weapons_) format3 |= w.schemaVersion >= 3;
    uint32_t hash = Fingerprint();
    if (format3) for (unsigned char c : std::string("PrimeXT modular runtime J.10.6.9.4")) {
        hash ^= c; hash *= 16777619u;
    }
    bool format4 = false;
    for (const auto& c : cartridges_) format4 |= c.schemaVersion >= 4;
    for (const auto& w : weapons_) format4 |= w.schemaVersion >= 4;
    if (format4) for (unsigned char c : std::string("PrimeXT modular runtime J.11.2 cartridge-dispatch")) {
        hash ^= c; hash *= 16777619u;
    }
    return hash;
}
std::string SerializeCartridge(const Cartridge& c) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<float>::max_digits10)
           << "version = " << c.schemaVersion << "\nid = " << c.id << "\nchamber = " << c.chamber
           << "\nprojectile = " << c.projectileType
           << "\nmass_grams = " << c.massGrams
           << "\ndiameter_mm = " << c.diameterMm
           << "\ndrag_coefficient = " << c.dragCoefficient
           << "\nreference_velocity_mps = " << c.referenceVelocityMps
           << "\nprojectile_count = " << c.projectileCount << '\n';
    if (c.schemaVersion == 2) output << "damage = " << c.damage << '\n';
    if (c.schemaVersion == 3) output << "damage_coefficient = " << c.damageCoefficient << '\n';
    if (c.schemaVersion == 4) {
        output << "attack_logic = " << (c.attackLogic==AttackLogic::Hitscan?"hitscan":"ballistic") << '\n';
        switch (c.damageLogic) {
        case DamageLogic::Legacy:
            output << "damage_logic = legacy\ndefault_damage = " << c.defaultDamage
                   << "\nlegacy_bullet_type = " << c.legacyBulletType << '\n'; break;
        case DamageLogic::ConfigurableBase:
            output << "damage_logic = configurable_base\nbase_damage = " << c.baseDamage << '\n'; break;
        case DamageLogic::EnergyBased:
            output << "damage_logic = energy_based\ndamage_coefficient = " << c.damageCoefficient << '\n'; break;
        }
    }
    if (!c.category.empty()) output << "category = " << c.category << '\n';
    if (!c.sprite.empty()) output << "sprite = " << c.sprite << '\n';
    return output.str();
}
std::string SerializeWeapon(const Weapon& w) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<float>::max_digits10)
           << "version = " << (w.schemaVersion>=4?4:w.schemaVersion>=3?3:2) << "\nid = " << w.id << "\ndisplay_name = " << w.displayName
           << "\nruntime_id = " << w.runtimeId << "\nammo_name = " << w.ammoName
           << "\ndefault_cartridge = " << w.defaultCartridgeId
           << "\nclip_size = " << w.clipSize << "\ndefault_ammo = " << w.defaultAmmo
           << "\nmax_carry = " << w.maxCarry << "\nslot = " << w.slot
           << "\nposition = " << w.position << "\nweight = " << w.weight
           << "\nflags = " << w.flags
           << "\nview_model = " << w.viewModel
           << "\nplayer_model = " << w.playerModel
           << "\nworld_model = " << w.worldModel;
    if (w.schemaVersion >= 4)
        output << "\ndefault_fire_rate = " << w.defaultFireRateRpm
               << "\ndefault_spread = " << w.defaultSpread;
    else
        output << "\nfire_interval = " << w.fireInterval
               << "\nspread = " << w.spread;
    output << "\nreload_seconds = " << w.reloadSeconds << "\nrange = " << w.range;
    if (w.schemaVersion < 4) output << "\nbullet_type = " << w.bulletType;
    output           << "\nnpc_usable = " << (w.npcUsable ? 1 : 0)
           << "\nfire_animation = " << w.fireAnimation
           << "\ndraw_animation = " << w.drawAnimation
           << "\nreload_animation = " << w.reloadAnimation << '\n';
    if (w.schemaVersion >= 4) {
        if (!w.defaultFireSound.empty()) output << "default_fire_sound = " << w.defaultFireSound << '\n';
    } else if (!w.fireSound.empty()) output << "fire_sound = " << w.fireSound << '\n';
    if (!w.reloadSound.empty()) output << "reload_sound = " << w.reloadSound << '\n';
    if (!w.pickupSound.empty()) output << "pickup_sound = " << w.pickupSound << '\n';
    if (!w.sprite.empty()) output << "sprite = " << w.sprite << '\n';
    if (!w.fireEvent.empty()) output << "fire_event = " << w.fireEvent << '\n';
    const auto writeAction = [&](const char* prefix, const Attack& a) {
        output << prefix << "behavior = " << a.behavior << '\n'
               << prefix << "mode = " << a.mode << '\n'
               ;
        if (w.schemaVersion < 4)
            output << prefix << "interval = " << a.interval << '\n'
                   << prefix << "spread = " << a.spread << '\n';
        output               << prefix << "projectiles = " << a.projectiles << '\n'
               << prefix << "ammo_cost = " << a.ammoCost << '\n'
               << prefix << "burst_count = " << a.burstCount << '\n'
               << prefix << "recoil_pitch = " << a.recoilPitch << '\n'
               << prefix << "recoil_yaw = " << a.recoilYaw << '\n'
               << prefix << "animation = " << a.animation << '\n';
        if (w.schemaVersion<4 && !a.sound.empty()) output << prefix << "sound = " << a.sound << '\n';
        if (!a.event.empty()) output << prefix << "event = " << a.event << '\n';
    };
    writeAction("primary_", w.primary);
    writeAction("secondary_", w.secondary);
    output << "muzzle_flash = " << (w.muzzleFlash ? "true" : "false") << '\n'
           << "fire_underwater = " << (w.fireUnderwater ? "true" : "false") << '\n'
           << "auto_reload = " << (w.autoReload ? "true" : "false") << '\n'
           << "idle_animation = " << w.idleAnimation << '\n'
           << "idle_seconds = " << w.idleSeconds << '\n'
           << "animation_extension = " << w.animationExtension << '\n';
    for (const auto& a : w.compatible)
        output << "accept = " << a.cartridgeId << ':' << a.muzzleVelocityMps << '\n';
    if (!w.category.empty()) output << "category = " << w.category << '\n';
    std::vector<const AssetVariant*> assets;
    for (const auto& a:w.assets) assets.push_back(&a);
    std::sort(assets.begin(),assets.end(),[](const AssetVariant* a,const AssetVariant* b) { return a->id<b->id; });
    for (const auto* a:assets) {
        output << "asset." << a->id << ".role = " << a->role << '\n'
               << "asset." << a->id << ".path = " << a->path << '\n'
               << "asset." << a->id << ".when = ";
        switch (a->condition) {
        case AssetCondition::Default: output << "default"; break;
        case AssetCondition::Random: output << "random"; break;
        case AssetCondition::FireMode: output << "mode:" << a->conditionValue; break;
        case AssetCondition::Cartridge: output << "ammo:" << a->conditionValue; break;
        }
        output << '\n';
    }
    std::vector<const AmmoOverride*> overrides;
    for (const auto& a:w.ammoOverrides) overrides.push_back(&a);
    std::sort(overrides.begin(),overrides.end(),[](const AmmoOverride* a,const AmmoOverride* b) { return a->cartridgeId<b->cartridgeId; });
    for (const auto* a:overrides) {
        for (const auto& field:OverrideFields())
            if (!(a->*(field.second)).empty()) output << "ammo." << a->cartridgeId << '.' << field.first << " = " << (a->*(field.second)) << '\n';
        if (w.schemaVersion >= 4) {
            if (a->fireRateRpm) output << "ammo." << a->cartridgeId << ".fire_rate = " << *a->fireRateRpm << '\n';
            if (a->spreadOverride) output << "ammo." << a->cartridgeId << ".spread = " << *a->spreadOverride << '\n';
        }
    }
    return output.str();
}
Registry& SharedRegistry() { static Registry registry; return registry; }
} // namespace modular
