// Standalone build:
// Build: c++ -std=c++17 -Igame_shared game_shared/modular_weapons.cpp game_shared/tests/modular_weapons_test.cpp -o /tmp/modular_weapons_test
#include "modular_weapons.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

static std::string ReadDisk(const std::string& p) {
    std::ifstream file(p, std::ios::binary);
    std::ostringstream data;
    data << file.rdbuf();
    return data.str();
}
int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string root = argv[1];
    modular::ReadFile fromDisk = [&](const std::string& path, std::string& data) {
        std::ifstream check(root + "/" + path);
        if (!check) return false;
        data = ReadDisk(root + "/" + path);
        return true;
    };
    modular::Registry registry;
    assert(registry.Load(fromDisk));
    assert(registry.Weapons().size() == 1);
    assert(registry.Cartridges().size() == 2);
    const auto* w = registry.FindWeaponByRuntimeId(32);
    assert(w && w->id == "test_pistol" && w->clipSize == 17);
    assert(w->primary.mode == "semi" && w->primary.behavior == "hitscan");
    assert(w->secondary.mode == "semi" && w->secondary.behavior == "hitscan");
    assert(w->secondary.spread == 0.08f && w->secondary.ammoCost == 1);
    assert(!w->fireUnderwater && w->autoReload);
    assert(w->compatible.size() == 2 && w->defaultCartridgeId == "test_fmj");
    assert(w->compatible[0].cartridgeId == "test_fmj");
    assert(w->compatible[0].muzzleVelocityMps == 360.f);
    assert(!registry.FindWeaponByRuntimeId(63));
    const auto* c = registry.FindCartridge("test_jhp");
    assert(c && c->projectileType == "jhp");
    // Shared serialization is built for the future standalone editor.
    const float originalMass = c->massGrams;
    const auto serializedWeapon = modular::SerializeWeapon(*w);
    const auto serializedCartridge = modular::SerializeCartridge(*c);
    assert(serializedWeapon.find("default_cartridge = test_fmj") != std::string::npos);
    auto roundtrip = [&](const std::string& filename, std::string& text) {
        if (filename == "scripts/weapons/test_pistol.weapon") { text = serializedWeapon; return true; }
        if (filename == "scripts/cartridges/test_jhp.cartridge") { text = serializedCartridge; return true; }
        return fromDisk(filename, text);
    };
    assert(registry.Load(roundtrip));
    assert(registry.FindWeaponByRuntimeId(32)->compatible[1].muzzleVelocityMps == 335.f);
    assert(registry.FindWeaponByRuntimeId(32)->secondary.mode == "semi");
    assert(registry.FindCartridge("test_jhp")->massGrams == originalMass);

    // Version-1 weapons from J.1 must retain their original primary hitscan
    // behavior with no configured secondary attack.
    auto legacy = [&](const std::string& filename, std::string& text) {
        if (!fromDisk(filename, text)) return false;
        if (filename == "scripts/weapons/test_pistol.weapon") {
            const auto firstNewField = text.find("# Reusable shared weapon actions");
            assert(firstNewField != std::string::npos);
            text.erase(firstNewField);
            const auto version = text.find("version = 2");
            assert(version != std::string::npos);
            text.replace(version, 11, "version = 1");
        }
        return true;
    };
    assert(registry.Load(legacy));
    assert(registry.FindWeaponByRuntimeId(32)->primary.mode == "auto");
    assert(registry.FindWeaponByRuntimeId(32)->secondary.behavior == "none");
    // Extension behavior IDs are explicitly registered before loading assets.
    assert(modular::RegisterBehaviorId("test_shared_module"));
    assert(modular::KnownBehaviorId("test_shared_module"));
    assert(!modular::RegisterBehaviorId("invalid/name"));
    // Burst and secondary actions are parsed entirely from data, with no new
    // C++ weapon class. Verify the fire-control configuration survives reload.
    auto burst = [&](const std::string& filename, std::string& text) {
        if (!fromDisk(filename, text)) return false;
        if (filename == "scripts/weapons/test_pistol.weapon") {
            const auto pos = text.find("primary_mode = semi");
            assert(pos != std::string::npos);
            text.replace(pos, 19, "primary_mode = burst\nprimary_burst_count = 4");
        }
        return true;
    };
    assert(registry.Load(burst));
    assert(registry.FindWeaponByRuntimeId(32)->primary.mode == "burst");
    assert(registry.FindWeaponByRuntimeId(32)->primary.burstCount == 4);
    // Legacy mod: no manifests should be harmless.
    assert(registry.Load([](const std::string&, std::string&) { return false; }));
    assert(registry.Weapons().empty());

    auto checkInvalid = [&](const std::string& path, const std::string& oldValue, const std::string& newValue) {
        const auto mocked = [&](const std::string& filename, std::string& contents) {
            if (!fromDisk(filename, contents)) return false;
            if (filename == path) {
                const auto at = contents.find(oldValue);
                assert(at != std::string::npos);
                contents.replace(at, oldValue.size(), newValue);
            }
            return true;
        };
        assert(!registry.Load(mocked));
        assert(!registry.Valid());
        assert(registry.Weapons().empty() && registry.Cartridges().empty());
        assert(!registry.Diagnostics().empty());
    };
    const std::string wp = "scripts/weapons/test_pistol.weapon";
    const std::string cp = "scripts/cartridges/test_jhp.cartridge";
    checkInvalid(wp, "runtime_id = 32", "runtime_id = 63"); // suit-reserved
    checkInvalid(wp, "slot = 1", "slot = 5"); // unsafe array access
    checkInvalid(wp, "accept = test_fmj:360", "accept = missing:360");
    checkInvalid(wp, "accept = test_fmj:360", "accept = test_fmj:nan");
    checkInvalid(wp, "view_model = models/v_9mmhandgun.mdl", "view_model = ../private");
    checkInvalid(wp, "fire_interval = 0.3", "fire_intervl = 0.3"); // typo
    checkInvalid(wp, "version = 2", "version = 999");
    checkInvalid(wp, "default_cartridge = test_fmj", "default_cartridge = missing");
    checkInvalid(cp, "mass_grams = 7.45", "mass_grams = -1");
    checkInvalid(cp, "projectile = jhp", "projectile = jhp\nunknown = abc");
    checkInvalid("scripts/weapons/manifest.txt", "test_pistol", "test_pistol\ntest_pistol");
    checkInvalid("scripts/cartridges/manifest.txt", "test_jhp", "missing_file");
    checkInvalid(wp, "primary_mode = semi", "primary_mode = unknown");
    checkInvalid(wp, "secondary_behavior = hitscan", "secondary_behavior = missing_module");
    checkInvalid(wp, "secondary_projectiles = 1", "secondary_projectiles = 0");
    checkInvalid(wp, "secondary_ammo_cost = 1", "secondary_ammo_cost = 999");
    checkInvalid(wp, "primary_recoil_pitch = -2.0", "primary_recoil_pitch = nan");
    checkInvalid(wp, "secondary_sound = weapons/pl_gun3.wav", "secondary_sound = ../unsafe.wav");
    checkInvalid(wp, "animation_extension = onehanded", "animation_extension = ../../unsafe");
    checkInvalid(wp, "fire_underwater = false", "fire_underwater = maybe");
    checkInvalid(wp, "primary_mode = semi", "primary_mode = single");
    std::cout << "PASS: v1/v2 roundtrip, registered behavior IDs, 21 invalid/atomic-rejection cases\n";
}
