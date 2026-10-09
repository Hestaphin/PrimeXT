// Portable J.6 regression: atomically admit modular IDs/ammo names into legacy
// game limits, preserving old weapon and ammo registrations.
#include "modular_weapons.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    assert(argc == 2);
    const std::string root = argv[1];
    const modular::ReadFile load = [&](const std::string& filename, std::string& text) {
        std::ifstream source(root + '/' + filename);
        if (!source) return false;
        std::ostringstream buffer;
        buffer << source.rdbuf();
        text = buffer.str();
        return true;
    };
    modular::Registry registry;
    assert(registry.Load(load));
    assert(registry.Valid());
    const auto* gun = registry.FindWeapon("test_pistol");
    assert(gun && gun->runtimeId == 32);
    const uint32_t fingerprint = registry.Fingerprint();
    std::string error;
    assert(registry.ValidateForRuntime(32, 16, {"9mm", "buckshot"}, {4, 7}, error));
    assert(error.empty() && registry.Fingerprint() == fingerprint);
    // Legacy ammo names are case-insensitive at runtime.
    assert(!registry.ValidateForRuntime(32, 16, {"TEST_FMJ"}, {}, error));
    assert(error.find("test_fmj") != std::string::npos);
    assert(!registry.ValidateForRuntime(32, 30, {}, {}, error));
    assert(error.find("slots") != std::string::npos);
    assert(!registry.ValidateForRuntime(32, 16, {}, {32}, error));
    assert(error.find("weapon ID") != std::string::npos);
    // A no-legacy-name configuration must still reject two cartridge assets
    // whose case differs, because both would map to a single engine ammo ID.
    const modular::ReadFile duplicateCase = [&](const std::string& filename, std::string& text) {
        if (filename == "scripts/cartridges/manifest.txt") {
            text = "test_fmj\ntest_jhp\nTEST_FMJ\n";
            return true;
        }
        if (filename == "scripts/cartridges/TEST_FMJ.cartridge") {
            if (!load("scripts/cartridges/test_fmj.cartridge", text)) return false;
            const size_t pos = text.find("id = test_fmj");
            assert(pos != std::string::npos);
            text.replace(pos, sizeof("id = test_fmj") - 1, "id = TEST_FMJ");
            return true;
        }
        return load(filename, text);
    };
    assert(registry.Load(duplicateCase));
    assert(!registry.ValidateForRuntime(32, 16, {}, {}, error));
    assert(error.find("TEST_FMJ") != std::string::npos);
    registry.DisableRuntime(error);
    assert(!registry.Valid());
    assert(registry.Weapons().empty() && registry.Cartridges().empty());
    assert(registry.Fingerprint() == 0);
    assert(!registry.ValidateForRuntime(32, 16, {}, {}, error));
    std::cout << "PASS J.6: admission, legacy conflicts, overflow, alias, atomic disable\n";
}
