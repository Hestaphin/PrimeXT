#pragma once
#include "vector.h"
class CBaseEntity;
// Use the same .weapon/.cartridge definitions as players, from NPC AI fire events.
// Returns the *authored ammunition cost* when a shot succeeds, otherwise zero.
// Insufficient magazine rounds must not spawn any projectiles or produce effects.
int FireModularNpcWeapon(CBaseEntity* shooter, const char* weaponId,
                          const char* cartridgeId, const Vector& source,
                          const Vector& aimDirection, int availableRounds);
// Create a conventional player-pickup of the NPC's data-driven weapon.
CBaseEntity* DropModularNpcWeapon(const char* weaponId, const char* cartridgeId,
                                 int magazineRounds, const Vector& source,
                                 const Vector& angles);
