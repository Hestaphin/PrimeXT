// Data-driven NPC weapon adapter. AI handles aiming/attack schedules; projectile
// ownership, damage and physics are shared with player-fired ballistic rounds.
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "modular_npc.h"
#include "modular_weapons.h"
#include "modular_ballistics.h"
#include "modular_ammo_policy.h"
#include "cartridge_damage.h"
#include "modular_damage_scope.h"
#include "matrix.h"
#include <cmath>
#include <string>

int FireModularNpcWeapon(CBaseEntity* shooter, const char* weaponId,
                          const char* cartridgeId, const Vector& source,
                          const Vector& aimDirection, int availableRounds) {
    if (!shooter || !weaponId || !weaponId[0]) return false;
    const auto* weapon = modular::SharedRegistry().FindWeapon(weaponId);
    if (!weapon || !weapon->npcUsable) return false;
    const auto& attack = weapon->primary;
    if (attack.behavior != "ballistic" && attack.behavior != "hitscan") return 0;
    if (!modular_ammo::NpcShotCost(availableRounds, attack.ammoCost)) return 0;
    const char* selected = cartridgeId && cartridgeId[0] ? cartridgeId : weapon->defaultCartridgeId.c_str();
    const modular::Compatibility* compatibility = nullptr;
    for (const auto& c : weapon->compatible)
        if (c.cartridgeId == selected) { compatibility = &c; break; }
    const auto* cartridge = modular::SharedRegistry().FindCartridge(selected);
    if (!compatibility || !cartridge) return false;
    const int projectiles = attack.projectiles * cartridge->projectileCount;
    if (projectiles < 1 || projectiles > 256) return false;
    const Vector forward = aimDirection.Normalize();
    if (forward == g_vecZero) return false;
    const uint32_t seed = static_cast<uint32_t>(RANDOM_LONG(0, 32767));
    const modular::AssetSelection selection{cartridge->id, attack.mode, seed, modular::AssetAction::Primary};
    ScopedModularDamage damageSource({shooter->pev, weapon,
        modular::SelectAsset(*weapon, selection, "kill_sprite")});
    if (attack.behavior == "ballistic" && CVAR_GET_FLOAT("sv_ballistics_enabled") > 0) {
        const matrix3x3 orientation(UTIL_VecToAngles(forward));
        // Projectiles remain server-authoritative and can hit their NPC shooter.
        // The owner's edict is excluded by the existing I.10.10 resolver.
        if (!SpawnModularBallisticShot(shooter, *weapon, *cartridge,
                compatibility->muzzleVelocityMps, projectiles, source,
                orientation, attack.spread, seed, &selection)) return false;
    } else {
        shooter->FireBullets(projectiles, source, forward,
            Vector(attack.spread, attack.spread, attack.spread),
            weapon->range, weapon->bulletType, 0, cartridge->schemaVersion >= 3 ?
            float(modular::MuzzleDamage(*cartridge, compatibility->muzzleVelocityMps)) : 0.f);
    }
    const auto sound = modular::SelectAsset(*weapon, selection, "primary_sound", "fire_sound");
    if (!sound.empty()) EMIT_SOUND(shooter->edict(), CHAN_WEAPON, sound.c_str(), 1.f, ATTN_NORM);
    if (weapon->muzzleFlash) shooter->pev->effects |= EF_MUZZLEFLASH;
    return attack.ammoCost;
}

CBaseEntity* DropModularNpcWeapon(const char* weaponId, const char* cartridgeId,
                                 int magazineRounds, const Vector& source,
                                 const Vector& angles) {
    const auto* weapon = weaponId ? modular::SharedRegistry().FindWeapon(weaponId) : nullptr;
    if (!weapon || !weapon->npcUsable) return nullptr;
    edict_t* entity = CREATE_NAMED_ENTITY(MAKE_STRING("weapon_modular"));
    if (FNullEnt(entity)) return nullptr;
    KeyValueData key{};
    key.szClassName = const_cast<char*>("weapon_modular");
    key.szKeyName = const_cast<char*>("weapon_id");
    key.szValue = const_cast<char*>(weaponId);
    DispatchKeyValue(entity, &key); // must set identity BEFORE Spawn validates it
    key.szKeyName = const_cast<char*>("loaded_cartridge");
    key.szValue = const_cast<char*>(cartridgeId && cartridgeId[0] ? cartridgeId : weapon->defaultCartridgeId.c_str());
    DispatchKeyValue(entity, &key);
    key.szKeyName = const_cast<char*>("loaded_rounds");
    const int boundedRounds = magazineRounds < 0 ? 0 :
        (magazineRounds > weapon->clipSize ? weapon->clipSize : magazineRounds);
    const std::string rounds = std::to_string(boundedRounds);
    key.szValue = const_cast<char*>(rounds.c_str());
    DispatchKeyValue(entity, &key);
    entity->v.origin = source;
    entity->v.angles = angles;
    if (DispatchSpawn(entity) < 0) return nullptr;
    return CBaseEntity::Instance(entity);
}
