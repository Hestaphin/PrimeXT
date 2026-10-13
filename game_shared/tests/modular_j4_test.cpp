// Compile: c++ -std=c++17 -Igame_shared game_shared/modular_weapons.cpp
// game_shared/tests/modular_j4_test.cpp -o /tmp/modular_j4_test
#include "modular_weapons.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
int main(int argc,char** argv) {
    assert(argc == 2);
    const std::string root = argv[1];
    modular::ReadFile read = [&](const std::string& path, std::string& data) {
        std::ifstream f(root + "/" + path);
        if (!f) return false;
        std::ostringstream out;out << f.rdbuf();data=out.str();return true;
    };
    modular::Registry r;
    assert(r.Load(read));
    const auto* w=r.FindWeapon("test_pistol");
    assert(w && w->npcUsable && w->compatible.size() == 2);
    const auto baseline=r.Fingerprint();
    assert(baseline != 0);
    auto same = [&](const std::string& path, std::string& data) {
        if (!read(path,data)) return false;
        if (path == "scripts/weapons/test_pistol.weapon") data += "\n# whitespace/comment should not change fingerprint\n";
        return true;
    };
    assert(r.Load(same));
    assert(r.Fingerprint() == baseline);
    auto modified = [&](const std::string& path, std::string& data) {
        if (!read(path,data)) return false;
        if (path == "scripts/weapons/test_pistol.weapon") {
            const auto at = data.find("accept = test_fmj:360");
            assert(at != std::string::npos);
            data.replace(at, std::string("accept = test_fmj:360").size(), "accept = test_fmj:370");
        }
        return true;
    };
    assert(r.Load(modified));
    assert(r.Fingerprint() != baseline);
    auto disabledNpc = [&](const std::string& path, std::string& data) {
        if (!read(path,data)) return false;
        if (path == "scripts/weapons/test_pistol.weapon") {
            const auto at=data.find("npc_usable = 1");
            assert(at != std::string::npos);
            data.replace(at, 14, "npc_usable = 0");
        }
        return true;
    };
    assert(r.Load(disabledNpc) && !r.FindWeapon("test_pistol")->npcUsable);
    assert(r.Fingerprint() != baseline);
    auto invalidNpc = [&](const std::string& path, std::string& data) {
        if (!read(path,data)) return false;
        if (path == "scripts/weapons/test_pistol.weapon") {
            const auto at=data.find("npc_usable = 1");
            data.replace(at,14,"npc_usable = maybe");
        }
        return true;
    };
    assert(!r.Load(invalidNpc) && !r.Valid() && r.Weapons().empty());
    assert(r.Fingerprint() == 0);
    std::cout << "PASS J4: NPC opt-in, canonical fingerprint, velocity delta, invalid config\n";
}
