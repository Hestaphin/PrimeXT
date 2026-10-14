// PrimeXT modular weapon definitions. Shared by game DLLs and standalone tools.
// Copyright (C) 2026 PrimeXT contributors. GPL-3.0-or-later.
#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace modular {
constexpr int kSchemaVersion = 4;
constexpr int kCartridgeSchemaVersion = 4;
constexpr int kFirstWeaponId = 32; // preserve all legacy 6-bit weapon IDs below this boundary
constexpr int kLastWeaponId = 62; // 63 is WEAPON_SUIT

// Version 4: firing physics is a property of the loaded cartridge.
// These enums do not select weapon primary/secondary actions.
enum class AttackLogic { Hitscan, Ballistic };
enum class DamageLogic { Legacy, ConfigurableBase, EnergyBased };
// Authored rounds per minute.  Convert to seconds only at scheduling boundaries.
inline double SecondsPerShot(double roundsPerMinute) { return 60.0 / roundsPerMinute; }

struct Cartridge {
    std::string id, chamber, projectileType;
    float massGrams = 0, diameterMm = 0, dragCoefficient = 0, referenceVelocityMps = 0;
    int projectileCount = 1;
    int schemaVersion = 1;
    float damage = 20.f; // formats 1/2: authored muzzle damage and legacy energy scaling
    float damageCoefficient = 0.f; // schema 3: scales J/mm^2 into damage; legacy schemas keep damage
    std::string category, sprite; // category and optional ammo sprite
    // Version 4 explicit, mutually exclusive damage/attack definitions.
    AttackLogic attackLogic = AttackLogic::Hitscan;
    DamageLogic damageLogic = DamageLogic::Legacy;
    float defaultDamage = 20.f; // Hitscan + Legacy, HP per impacting projectile
    int legacyBulletType = 1;  // Hitscan effects classification; not a damage multiplier
    float baseDamage = 20.f;    // Ballistic + ConfigurableBase, HP at full energy
};
struct Compatibility {
    std::string cartridgeId;
    float muzzleVelocityMps = 0;
};
// A reusable action, not a hard-coded weapon class. Behavior IDs are registered
// before loading definitions (builtin: hitscan, none). Custom handlers are
// implemented once in shared game code and referenced by any weapon asset.
struct Attack {
    std::string behavior = "hitscan";
    std::string mode = "auto"; // auto | semi | burst | single
    std::string sound, event;
    float interval = 0.2f, spread = 0.01f, recoilPitch = 0.f, recoilYaw = 0.f;
    int projectiles = 1, ammoCost = 1, burstCount = 3, animation = 0;
};
enum class AssetCondition { Default, Random, FireMode, Cartridge };
struct AssetVariant {
    std::string id, role, path, conditionValue;
    AssetCondition condition = AssetCondition::Default;
};
struct AmmoOverride {
    std::string cartridgeId;
    std::string viewModel, playerModel, worldModel;
    std::string fireSound, reloadSound, sprite, hudSprite, killSprite;
    // Version 4: absent differs from an explicitly-authored zero spread.
    std::optional<float> fireRateRpm, spreadOverride;
};
struct Weapon {
    std::string id, displayName, ammoName, defaultCartridgeId, viewModel, playerModel, worldModel;
    std::string fireSound, reloadSound, pickupSound, sprite, fireEvent, animationExtension = "onehanded";
    Attack primary, secondary;
    std::vector<Compatibility> compatible;
    int runtimeId = -1, clipSize = 0, defaultAmmo = 0, maxCarry = 0;
    int slot = 0, position = 0, weight = 10, flags = 0;
    int fireAnimation = 0, drawAnimation = 0, reloadAnimation = 0;
    bool muzzleFlash = true, fireUnderwater = true, autoReload = true;
    int idleAnimation = -1;
    float idleSeconds = 3.f;
    bool npcUsable = false; // opt-in: NPCs may use this weapon definition
    int bulletType = 1; // existing BULLET_PLAYER_9MM for first-phase legacy trace
    float fireInterval = 0.2f, spread = 0.01f, reloadSeconds = 1.5f, range = 8192.f;
    int schemaVersion = 2; // old canonical writers normalize formats 1/2 to 2
    // Version 4 only; legacy fireInterval/spread/fireSound remain for v1-3.
    float defaultFireRateRpm = 300.f;
    float defaultSpread = 0.01f;
    std::string defaultFireSound;
    std::string category;
    std::vector<AssetVariant> assets;
    std::vector<AmmoOverride> ammoOverrides;
};
struct EffectiveFireSettings {
    float rateRpm = 300.f;
    float spread = 0.01f;
    std::string sound;
    double SecondsBetweenShots() const { return SecondsPerShot(rateRpm); }
};
inline EffectiveFireSettings ResolveFireSettings(const Weapon& weapon,
                                                  const std::string& loadedCartridgeId) {
    EffectiveFireSettings settings;
    if (weapon.schemaVersion >= 4) {
        settings.rateRpm = weapon.defaultFireRateRpm;
        settings.spread = weapon.defaultSpread;
        settings.sound = weapon.defaultFireSound;
        for (const auto& override : weapon.ammoOverrides) {
            if (override.cartridgeId != loadedCartridgeId) continue;
            if (override.fireRateRpm) settings.rateRpm = *override.fireRateRpm;
            if (override.spreadOverride) settings.spread = *override.spreadOverride;
            if (!override.fireSound.empty()) settings.sound = override.fireSound;
            break;
        }
    } else {
        settings.rateRpm = static_cast<float>(60.0 / weapon.fireInterval);
        settings.spread = weapon.spread;
        settings.sound = weapon.fireSound;
    }
    return settings;
}

struct Diagnostic { std::string file, message; int line = 0; };
using ReadFile = std::function<bool(const std::string&, std::string&)>;

class Registry {
public:
    // Read both ordered manifest files from the engine's virtual filesystem.
    // An absent manifest is OK for older mods; an invalid present file is not.
    bool Load(const ReadFile& read);
    const Cartridge* FindCartridge(const std::string& id) const;
    int AmmoReserveLimit(const std::string& cartridgeId) const;
    const Weapon* FindWeapon(const std::string& id) const;
    const Weapon* FindWeaponByRuntimeId(int id) const;
    const std::vector<Weapon>& Weapons() const { return weapons_; }
    const std::vector<Cartridge>& Cartridges() const { return cartridges_; }
    const std::vector<Diagnostic>& Diagnostics() const { return diagnostics_; }
    bool Valid() const { return valid_; }
    // Preflight the legacy ammo slots and six-bit weapon IDs before precaching
    // ANY modular assets. A failed runtime admission must not leave a valid
    // client fingerprint for weapon definitions the server never registered.
    bool ValidateForRuntime(int ammoSlots, int usedAmmoSlots,
        const std::vector<std::string>& occupiedAmmoNames,
        const std::vector<int>& occupiedWeaponIds, std::string& error) const;
    void DisableRuntime(const std::string& reason);
    // Deterministic digest over canonical parsed assets, for client/server validation.
    uint32_t Fingerprint() const;
    // Versioned runtime semantics must agree as well as the authored bytes.
    uint32_t RuntimeFingerprint() const;
private:
    bool ReadManifest(const ReadFile& read, const char* path, const char* folder,
                      const char* extension, std::vector<std::string>& paths);
    void Error(const std::string& path, int line, const std::string& message);
    std::vector<Weapon> weapons_;
    std::vector<Cartridge> cartridges_;
    std::vector<Diagnostic> diagnostics_;
    bool valid_ = false;
};
// Deterministic writers; old assets retain their pre-format-3 canonical bytes.
// A shared whitelist: extension modules must advertise the ID BEFORE Load().
// The game-side module also installs the corresponding execution callback.
bool RegisterBehaviorId(const std::string& id);
bool KnownBehaviorId(const std::string& id);
std::string SerializeCartridge(const Cartridge& cartridge);
std::string SerializeWeapon(const Weapon& weapon);
Registry& SharedRegistry();
} // namespace modular
