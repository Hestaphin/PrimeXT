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
#include "modular_npc_cadence.h"
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
    const bool fireLoaded = weapon->schemaVersion >= 4 &&
        attack.behavior == "fire_loaded_cartridge";
    if (!fireLoaded && attack.behavior != "ballistic" && attack.behavior != "hitscan") return 0;
    if (!modular_ammo::NpcShotCost(availableRounds, attack.ammoCost)) return 0;
    const char* selected = cartridgeId && cartridgeId[0] ? cartridgeId : weapon->defaultCartridgeId.c_str();
    const modular::Compatibility* compatibility = nullptr;
    for (const auto& c : weapon->compatible)
        if (c.cartridgeId == selected) { compatibility = &c; break; }
    const auto* cartridge = modular::SharedRegistry().FindCartridge(selected);
    if (!compatibility || !cartridge || (fireLoaded && cartridge->schemaVersion != 4)) return 0;
    const bool ballistic = fireLoaded ? cartridge->attackLogic == modular::AttackLogic::Ballistic :
        attack.behavior == "ballistic";
    // When ballistics is disabled, the same loaded cartridge fires through
    // the existing hitscan fallback, retaining its muzzle damage and effects.
    const int projectiles = attack.projectiles * cartridge->projectileCount;
    if (projectiles < 1 || projectiles > 256) return false;
    const Vector forward = aimDirection.Normalize();
    if (forward == g_vecZero) return false;
    const auto settings = fireLoaded ? modular::ResolveFireSettings(*weapon, cartridge->id) :
        modular::EffectiveFireSettings{};
    const float shotSpread = fireLoaded ? settings.spread : attack.spread;
    // Ammo RPM is a safety ceiling for NPC shots; existing AI schedules and
    // animation events may choose a slower cadence. Suppressed requests do not
    // consume ammunition, RNG, create projectiles, or play effects.
    const auto identity = reinterpret_cast<std::uintptr_t>(shooter->edict());
    const int serial = shooter->edict()->serialnumber;
    const double shotTime = gpGlobals->time;
    if (fireLoaded && !modular::SharedNpcFireCadence().Ready(identity, serial, weapon->id, shotTime)) return 0;
    const uint32_t seed = static_cast<uint32_t>(RANDOM_LONG(0, 32767));
    const modular::AssetSelection selection{cartridge->id, attack.mode, seed, modular::AssetAction::Primary};
    ScopedModularDamage damageSource({shooter->pev, weapon,
        modular::SelectAsset(*weapon, selection, "kill_sprite")});
    if (ballistic && CVAR_GET_FLOAT("sv_ballistics_enabled") > 0) {
        const matrix3x3 orientation(UTIL_VecToAngles(forward));
        // Projectiles remain server-authoritative and can hit their NPC shooter.
        // The owner's edict is excluded by the existing I.10.10 resolver.
        if (!SpawnModularBallisticShot(shooter, *weapon, *cartridge,
                compatibility->muzzleVelocityMps, projectiles, source,
                orientation, shotSpread, seed, &selection)) return false;
    } else {
        shooter->FireBullets(projectiles, source, forward,
            Vector(shotSpread, shotSpread, shotSpread),
            weapon->range, fireLoaded ? cartridge->legacyBulletType : weapon->bulletType, 0,
            (fireLoaded || cartridge->schemaVersion >= 3) ?
            float(modular::MuzzleDamage(*cartridge, compatibility->muzzleVelocityMps)) : 0.f);
    }
    const auto sound = modular::SelectAsset(*weapon, selection, "primary_sound", "fire_sound");
    if (!sound.empty()) EMIT_SOUND(shooter->edict(), CHAN_WEAPON, sound.c_str(), 1.f, ATTN_NORM);
    if (weapon->muzzleFlash) shooter->pev->effects |= EF_MUZZLEFLASH;
    if (fireLoaded)
        modular::SharedNpcFireCadence().Record(identity, serial, weapon->id, shotTime, settings.rateRpm);
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
