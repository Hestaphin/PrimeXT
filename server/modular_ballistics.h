#pragma once
#include "modular_weapons.h"
#include "vector.h"
#include "matrix.h"
#include <cstdint>
class CBaseEntity;
// Called once at EndFrame, after physics updates; no per-bullet Think callbacks.
void UpdateModularBallisticManager();
// All impacts resolved server-side against the I.10.10 nearest-hit pipeline.
// Returns false without spawning anything when projectile budget is exhausted.
bool SpawnModularBallisticShot(CBaseEntity* owner, const modular::Weapon& weapon,
    const modular::Cartridge& cartridge, float muzzleVelocityMps, int count,
    const Vector& origin, const matrix3x3& orientation, float spread,
    std::uint32_t seed);
