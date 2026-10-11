// J.10.3 policy regressions; no engine or PhysX dependency.
#include "../modular_ammo_policy.h"
#include <cassert>
#include <cstdio>
int main() {
    // First world pickup: default rounds can fill only an EMPTY new magazine.
    assert(modular_ammo::SpawnClipRounds(0, 17, 30) == 17);
    assert(modular_ammo::SpawnClipRounds(4, 17, 30) == 0);
    assert(modular_ammo::SpawnClipRounds(0, 17, 0) == 0);
    // World pickup FMJ must not fill an existing JHP clip; the runtime's
    // AddDuplicate sends DEFAULT FMJ to its own reserve, never to the clip.
    assert(modular_ammo::ActualGranted(10, 10, 10) == 0); // GiveAmmo slot even if full
    assert(modular_ammo::ActualGranted(4, 7, 10) == 3);   // partial capacity
    assert(modular_ammo::ActualGranted(0, 10, 10) == 10);
    // When only selected JHP has ammo, deployment remains possible.
    assert(modular_ammo::ConsiderReserve(1, 1, 0, false));
    assert(!modular_ammo::ConsiderReserve(2, 1, 0, false));
    assert(modular_ammo::ConsiderReserve(2, 1, 0, true));
    assert(modular_ammo::NpcShotCost(1, 3) == 0);
    assert(modular_ammo::NpcShotCost(15, 3) == 3);
    assert(modular_ammo::NpcShotCost(0, 1) == 0);
    assert(modular_ammo::NpcShotCost(5, 0) == 0);
    std::puts("PASS J.10.3 magazine policy, grants, eligible reserve, NPC ammo cost");
}
