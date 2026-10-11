// PrimeXT modular weapon definitions. Shared by game DLLs and standalone tools.
// Copyright (C) 2026 PrimeXT contributors. GPL-3.0-or-later.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace modular {
constexpr int kSchemaVersion = 3;
constexpr int kCartridgeSchemaVersion = 3;
constexpr int kFirstWeaponId = 32; // preserve all legacy 6-bit weapon IDs below this boundary
constexpr int kLastWeaponId = 62; // 63 is WEAPON_SUIT

struct Cartridge {
    std::string id, chamber, projectileType;
    float massGrams = 0, diameterMm = 0, dragCoefficient = 0, referenceVelocityMps = 0;
    int projectileCount = 1;
    int schemaVersion = 1;
    float damage = 20.f; // formats 1/2: authored muzzle damage and legacy energy scaling
    float damageCoefficient = 0.f; // schema 3: scales J/mm^2 into damage; legacy schemas keep damage
    std::string category, sprite; // schema 3 organizational path and optional ammo sprite
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
    std::string category;
    std::vector<AssetVariant> assets;
    std::vector<AmmoOverride> ammoOverrides;
};
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
    // Format-3 runtime semantics must agree as well as the authored bytes.
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
