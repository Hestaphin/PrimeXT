// Standalone editor invokes the same validated Registry as PrimeXT game DLLs.
#include "modular_weapons.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>
int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: primext-weapon-validate <mod-root>\n"; return 2; }
    const std::filesystem::path root = std::filesystem::absolute(argv[1]);
    modular::Registry registry;
    bool ok=registry.Load([&](const std::string& rel,std::string& data){
        std::ifstream stream(root / rel, std::ios::binary);
        if (!stream) return false;
        std::ostringstream content; content << stream.rdbuf(); data=content.str();
        return true;
    });
    for (const auto& error:registry.Diagnostics())
        std::cerr << error.file << ':' << error.line << ": " << error.message << '\n';
    if (!ok) return 1;
    // Editor cannot know a target mod's pre-existing ammo reservations, but
    // must reject case-insensitive aliases even within its own cartridge set.
    std::string admissionError;
    if (!registry.ValidateForRuntime(32, 0, {}, {}, admissionError)) {
        std::cerr << "Runtime preflight: " << admissionError << '\n';
        return 1;
    }
    std::cout << "VALID: " << registry.Weapons().size() << " weapons, "
              << registry.Cartridges().size() << " cartridges, fingerprint="
              << registry.Fingerprint() << '\n';
    return 0;
}
