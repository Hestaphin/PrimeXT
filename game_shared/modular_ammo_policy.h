#pragma once

// Small engine-independent ammunition policies. Shared by game runtime and
// portable regression tests; cartridge identity is held by the calling weapon.
namespace modular_ammo {
inline int SpawnClipRounds(int currentClip, int capacity, int spawnAmmo) {
    if (currentClip != 0 || capacity <= 0 || spawnAmmo <= 0) return 0;
    return spawnAmmo < capacity ? spawnAmmo : capacity;
}
inline int ActualGranted(int before, int after, int offered) {
    if (offered <= 0 || after <= before) return 0;
    const int change = after - before;
    return change < offered ? change : offered;
}
inline bool ConsiderReserve(int candidate, int selected, int loaded, bool canCycle) {
    return canCycle || candidate == selected || candidate == loaded;
}
inline int NpcShotCost(int available, int configuredCost) {
    return configuredCost > 0 && available >= configuredCost ? configuredCost : 0;
}
}
