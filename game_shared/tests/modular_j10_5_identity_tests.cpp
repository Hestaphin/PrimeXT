#include "../modular_item_identity.h"
#include <cassert>
#include <set>
#include <string>
int main() {
    using namespace modular_identity;
    std::set<std::string> seen;
    for (int i = 0; i < 128; ++i) {
        const auto id = NewUuid();
        assert(IsUuid(id));
        assert(seen.insert(id).second);
    }
    assert(!IsUuid("weapon_modular"));
    const auto gun = NewUuid(), mag = NewUuid();
    assert(gun != mag);
    MagazineRecord data{ mag, "jhp", {"jhp", "fmj", "jhp"}, 3};
    assert(ValidMagazine(data));
    assert(data.rounds[0] == "jhp" && data.rounds[1] == "fmj");
    data.rounds.push_back("fmj");
    assert(!ValidMagazine(data));
    data.rounds.pop_back();
    data.uuid = "invalid";
    assert(!ValidMagazine(data));
    assert(CarryAcrossLevel(MapExitOwner::PlayerCarried));
    assert(!CarryAcrossLevel(MapExitOwner::NpcCarried));
    assert(!CarryAcrossLevel(MapExitOwner::WorldUnclaimed));
}
