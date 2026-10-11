// Data-driven weapon runtime: shared by game client and server.
#include "modular.h"
#include "weapon_layer.h"
#include "modular_ammo_policy.h"
#include "cartridge_damage.h"
#include <map>
#include <utility>
#ifdef CLIENT_DLL
#include "const.h"
#include "hud.h"
#include "ammohistory.h"
#endif
#ifndef CLIENT_DLL
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "modular_ballistics.h"
#include "modular_damage_scope.h"
#endif

namespace {
using Handlers = std::map<std::string, ModularAttackHandler>;
Handlers& Extensions() { static Handlers handlers; return handlers; }
constexpr int kPrimaryLatch = 1;
constexpr int kSecondaryLatch = 2;
constexpr int kBurstSecondary = 4;
}

bool RegisterModularAttackBehavior(const std::string& id, ModularAttackHandler handler) {
    if (!handler || id == "hitscan" || id == "ballistic" || id == "ammo_cycle" ||
        id == "none" || Extensions().count(id)) return false;
    if (!modular::RegisterBehaviorId(id) && !modular::KnownBehaviorId(id)) return false;
    return Extensions().emplace(id, handler).second;
}

CModularWeaponContext::CModularWeaponContext(std::unique_ptr<IWeaponLayer>&& layer,
                                              const modular::Weapon* definition)
    : CBaseWeaponContext(std::move(layer)), m_definition(nullptr) {
    BindDefinition(definition);
}
void CModularWeaponContext::BindDefinition(const modular::Weapon* definition, bool initializeSpawn) {
    m_definition = definition;
    if (definition) {
        m_iId = definition->runtimeId;
        if (initializeSpawn) {
            m_iDefaultAmmo = definition->defaultAmmo;
            // Default can appear anywhere in the compatibility array.
            m_loadedCartridgeIndex = m_selectedCartridgeIndex = 0;
            for (size_t i=0; i<definition->compatible.size(); ++i)
                if (definition->compatible[i].cartridgeId == definition->defaultCartridgeId)
                    m_loadedCartridgeIndex = m_selectedCartridgeIndex = static_cast<int>(i);
        }
        if (!definition->primary.event.empty())
            m_primaryEvent = m_pLayer->PrecacheEvent(definition->primary.event.c_str());
        if (!definition->secondary.event.empty())
            m_secondaryEvent = m_pLayer->PrecacheEvent(definition->secondary.event.c_str());
    }
    if (definition) {
        m_iSecondaryAmmoType = m_pLayer->FindAmmoType(definition->defaultCartridgeId.c_str());
    }
    if (initializeSpawn) {
        m_assetSeed = 0;
        m_assetAction = 0;
        CancelBurst();
        m_modularFlags &= ~(kPrimaryLatch | kSecondaryLatch);
    }
}
const modular::Weapon* CModularWeaponContext::Definition() const {
    return m_definition ? m_definition : modular::SharedRegistry().FindWeaponByRuntimeId(m_iId);
}
int CModularWeaponContext::GetItemInfo(ItemInfo* info) const {
    const auto* def = Definition();
    if (!info || !def) return 0;
    *info = {};
    info->pszName = def->id.c_str();
    info->pszAmmo1 = def->defaultCartridgeId.c_str();
    info->iMaxAmmo1 = def->maxCarry;
    info->iMaxAmmo2 = -1;
    info->iMaxClip = def->clipSize;
    info->iSlot = def->slot;
    info->iPosition = def->position;
    info->iFlags = def->flags | (def->autoReload ? 0 : ITEM_FLAG_NOAUTORELOAD);
    info->iId = def->runtimeId;
    info->iWeight = def->weight;
    return 1;
}
// Legacy eligibility reads only pszAmmo1 (the definition's default cartridge).
// Data-driven guns can instead reload from their selected cartridge, and can
// cycle through the other compatible cartridges without changing loaded ammo.
bool CModularWeaponContext::IsUseable() {
    const auto* def = Definition();
    if (!def) return false;
    if (m_iClip > 0) return true;
    // Only registered compatible reserves may make an empty gun usable.
    const bool canCycle = def->primary.behavior == "ammo_cycle" ||
                          def->secondary.behavior == "ammo_cycle";
    for (size_t i = 0; i < def->compatible.size(); ++i) {
        if (!modular_ammo::ConsiderReserve(static_cast<int>(i),
                m_selectedCartridgeIndex, m_loadedCartridgeIndex, canCycle)) continue;
        const int ammo = m_pLayer->FindAmmoType(def->compatible[i].cartridgeId.c_str());
        if (ammo > 0 && ammo < MAX_AMMO_SLOTS && m_pLayer->GetPlayerAmmo(ammo) > 0)
            return true;
    }
    return false;
}

bool CModularWeaponContext::CanDeploy() { return IsUseable(); }

bool CModularWeaponContext::Deploy() {
#ifdef CLIENT_DLL
    extern bool ModularClientDefinitionsVerified();
    if (!ModularClientDefinitionsVerified()) return false;
#endif
    const auto* def = Definition();
    if (!def || !CanDeploy()) return false;
    CancelBurst();
    m_modularFlags = 0;
    PinAssets(modular::AssetAction::Deploy, m_pLayer->GetRandomSeed());
    const auto selection = AssetsFor(modular::AssetAction::Deploy, m_assetSeed);
    const auto view = modular::SelectAsset(*def, selection, "view_model");
    const auto player = modular::SelectAsset(*def, selection, "player_model");
    char* playerPath = const_cast<char*>(player.c_str());
#ifndef CLIENT_DLL
    playerPath = const_cast<char*>(STRING(ALLOC_STRING(player.c_str())));
#endif
    return DefaultDeploy(const_cast<char*>(view.c_str()),
                         playerPath,
                         def->drawAnimation, const_cast<char*>(def->animationExtension.c_str()));
}
void CModularWeaponContext::Holster() {
    CancelBurst();
    m_modularFlags = 0;
    CBaseWeaponContext::Holster();
}
void CModularWeaponContext::CancelBurst() {
    m_burstRemaining = 0;
    m_modularFlags &= ~kBurstSecondary;
}
const modular::Compatibility* CModularWeaponContext::LoadedCompatibility() const {
    const auto* def = Definition();
    if (!def || m_loadedCartridgeIndex < 0 ||
        m_loadedCartridgeIndex >= static_cast<int>(def->compatible.size())) return nullptr;
    return &def->compatible[m_loadedCartridgeIndex];
}

modular::AssetSelection CModularWeaponContext::AssetsFor(modular::AssetAction action, uint32_t seed) const {
    modular::AssetSelection selection;
    selection.seed = seed;
    selection.action = action;
    const auto* def = Definition();
    if (!def) return selection;
    const int index = action == modular::AssetAction::Reload ||
        (action == modular::AssetAction::AmmoChange && m_iClip == 0) ?
        m_selectedCartridgeIndex : m_loadedCartridgeIndex;
    if (index >= 0 && index < int(def->compatible.size())) selection.cartridgeId = def->compatible[index].cartridgeId;
    selection.mode = action == modular::AssetAction::Secondary ? def->secondary.mode : def->primary.mode;
    return selection;
}
void CModularWeaponContext::PinAssets(modular::AssetAction action, uint32_t seed) {
    m_assetSeed = seed;
    m_assetAction = int(action);
    ApplyPinnedAssets();
}
void CModularWeaponContext::ApplyPinnedAssets() {
    const auto* def = Definition();
    if (!def || !modular::ValidAssetAction(m_assetAction)) return;
#ifdef CLIENT_DLL
    extern bool ModularClientDefinitionsVerified();
    if (!ModularClientDefinitionsVerified()) return;
#endif
    const auto selection = AssetsFor(modular::AssetAction(m_assetAction), m_assetSeed);
    const auto view = modular::SelectAsset(*def, selection, "view_model");
    if (!view.empty()) m_pLayer->SetPlayerViewmodel(view);
#ifdef CLIENT_DLL
    const auto hud = modular::SelectAsset(*def, selection, "hud_sprite", "sprite");
    const auto* cartridge = modular::SharedRegistry().FindCartridge(selection.cartridgeId);
    gWR.SetModularSprites(m_iId, hud.c_str(), cartridge ? cartridge->sprite.c_str() : "");
#else
    auto* gun = m_pLayer->GetWeaponEntity();
    if (!gun) return;
    const auto world = modular::SelectAsset(*def, selection, "world_model");
    if (!world.empty() && world != STRING(gun->pev->model)) SET_MODEL(gun->edict(), world.c_str());
    if (gun->m_pPlayer) {
        const auto player = modular::SelectAsset(*def, selection, "player_model");
        if (!player.empty() && player != STRING(gun->m_pPlayer->pev->weaponmodel))
            gun->m_pPlayer->pev->weaponmodel = ALLOC_STRING(player.c_str());
    }
#endif
}

bool CModularWeaponContext::CycleCartridge() {
    const auto* def = Definition();
    if (!def || def->compatible.size() < 2 || m_fInReload || m_selectedCartridgeIndex < 0 ||
        m_selectedCartridgeIndex >= int(def->compatible.size())) return false;
    m_selectedCartridgeIndex = (m_selectedCartridgeIndex + 1) % static_cast<int>(def->compatible.size());
    m_iSecondaryAmmoType = m_pLayer->FindAmmoType(
        def->compatible[m_selectedCartridgeIndex].cartridgeId.c_str());
    PinAssets(modular::AssetAction::AmmoChange, m_pLayer->GetRandomSeed());
#ifndef CLIENT_DLL
    CBasePlayerWeapon* gun = m_pLayer->GetWeaponEntity();
    if (gun && gun->m_pPlayer)
        ClientPrint(gun->m_pPlayer->pev,HUD_PRINTCENTER,
            UTIL_VarArgs("Next reload: %s",def->compatible[m_selectedCartridgeIndex].cartridgeId.c_str()));
#endif
    return true;
}

void CModularWeaponContext::OnReloadFinished() {
    m_loadedCartridgeIndex = m_selectedCartridgeIndex;
    const auto* compatibility = LoadedCompatibility();
    if (compatibility) m_iPrimaryAmmoType = m_pLayer->FindAmmoType(compatibility->cartridgeId.c_str());
}

// Return true only after a shot has been executed. Unregistered behaviors never
// expend ammo or produce misleading audiovisual feedback.
bool CModularWeaponContext::FireAction(const modular::Attack& action, bool secondary) {
    const auto* def = Definition();
#ifdef CLIENT_DLL
    extern bool ModularClientDefinitionsVerified();
    if (!ModularClientDefinitionsVerified()) return false;
#endif
    if (!def || action.behavior == "none" || m_fInReload) return false;
    if (action.behavior == "ammo_cycle") return CycleCartridge();
    if (!def->fireUnderwater && m_pLayer->GetPlayerWaterlevel() == 3) return false;
    if (m_iClip < action.ammoCost) return false;
    ModularAttackHandler extension = nullptr;
    if (action.behavior != "hitscan" && action.behavior != "ballistic") {
        const auto found = Extensions().find(action.behavior);
        if (found == Extensions().end()) return false;
        extension = found->second;
    }
    const uint32_t seed = m_pLayer->GetRandomSeed();
    const auto assetAction = secondary ? modular::AssetAction::Secondary : modular::AssetAction::Primary;
    const auto selection = AssetsFor(assetAction, seed);
#ifndef CLIENT_DLL
    auto* sourceGun = m_pLayer->GetWeaponEntity();
    ScopedModularDamage damageSource({sourceGun && sourceGun->m_pPlayer ? sourceGun->m_pPlayer->pev : nullptr,
        def, modular::SelectAsset(*def, selection, "kill_sprite")});
#endif
    const Vector src = m_pLayer->GetGunPosition();
    matrix3x3 orientation = m_pLayer->GetCameraOrientation();
    if (extension) {
        if (!extension(*m_pLayer, *def, action, seed)) return false;
    } else if (action.behavior == "ballistic") {
        const auto* compatibility = LoadedCompatibility();
        const auto* cartridge = compatibility ?
            modular::SharedRegistry().FindCartridge(compatibility->cartridgeId) : nullptr;
        if (!cartridge) return false;
        const int projectileCount = action.projectiles * cartridge->projectileCount;
        if (projectileCount > 256 || projectileCount <= 0) return false;
#ifndef CLIENT_DLL
        CBasePlayerWeapon* gun = m_pLayer->GetWeaponEntity();
        if (!gun || !gun->m_pPlayer) return false;
        if (CVAR_GET_FLOAT("sv_ballistics_enabled") > 0) {
            if (!SpawnModularBallisticShot(gun->m_pPlayer, *def, *cartridge,
                compatibility->muzzleVelocityMps, projectileCount, src,
                orientation, action.spread, seed, &selection)) return false;
        } else {
            m_pLayer->FireBullets(projectileCount,src,orientation,def->range,
                                  action.spread,def->bulletType,seed,
                                  cartridge->schemaVersion >= 3 ? float(modular::MuzzleDamage(*cartridge, compatibility->muzzleVelocityMps)) : 0.f);
        }
#endif
        // The client predicts the firing state and audiovisual effects, never
        // collision/damage. Server is authoritative for actual bullet flight.
    } else {
        const auto* compatibility = LoadedCompatibility();
        const auto* cartridge = compatibility ? modular::SharedRegistry().FindCartridge(compatibility->cartridgeId) : nullptr;
        const bool densityDamage = cartridge && cartridge->schemaVersion >= 3;
        const int count = action.projectiles * (densityDamage ? cartridge->projectileCount : 1);
        if (count <= 0 || count > 256) return false;
        m_pLayer->FireBullets(count, src, orientation, def->range,
                              action.spread, def->bulletType, seed, densityDamage ?
                              float(modular::MuzzleDamage(*cartridge, compatibility->muzzleVelocityMps)) : 0.f);
    }
    m_iClip -= action.ammoCost;
    PinAssets(assetAction, seed);
    SendWeaponAnim(action.animation);
    if (action.recoilPitch != 0.f || action.recoilYaw != 0.f)
        m_pLayer->AddPlayerPunchangle(action.recoilPitch, action.recoilYaw, 0.f);
#ifndef CLIENT_DLL
    CBasePlayerWeapon* entity = m_pLayer->GetWeaponEntity();
    if (entity && entity->m_pPlayer) {
        entity->m_pPlayer->SetAnimation(PLAYER_ATTACK1);
        if (def->muzzleFlash) entity->m_pPlayer->pev->effects |= EF_MUZZLEFLASH;
        const auto sound = modular::SelectAsset(*def, selection,
            secondary ? "secondary_sound" : "primary_sound", "fire_sound");
        if (!sound.empty())
            EMIT_SOUND(entity->m_pPlayer->edict(), CHAN_WEAPON, sound.c_str(), 1.f, ATTN_NORM);
    }
#endif
    const uint16_t eventIndex = secondary ? m_secondaryEvent : m_primaryEvent;
    if (eventIndex && m_pLayer->ShouldRunFuncs()) {
        WeaponEventParams params{};
        params.flags = WeaponEventFlags::NotHost;
        params.eventindex = eventIndex;
        params.origin = src;
        params.angles = orientation.GetAngles();
        m_pLayer->PlaybackWeaponEvent(params);
    }
    m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + def->idleSeconds;
    return true;
}
void CModularWeaponContext::Trigger(bool secondary) {
    const auto* def = Definition();
    if (!def) return;
    const auto& action = secondary ? def->secondary : def->primary;
    if (action.behavior == "none") return;
    // One burst has priority over additional trigger inputs.
    if (m_burstRemaining) {
        ContinueBurst();
        return;
    }
    const int latch = secondary ? kSecondaryLatch : kPrimaryLatch;
    if (action.mode != "auto" && (m_modularFlags & latch)) return;
    if (action.mode != "auto") m_modularFlags |= latch;
    if (action.mode == "burst") {
        m_burstRemaining = action.burstCount;
        if (secondary) m_modularFlags |= kBurstSecondary;
        else m_modularFlags &= ~kBurstSecondary;
        ContinueBurst();
        return;
    }
    if (!FireAction(action, secondary) && m_fFireOnEmpty) PlayEmptySound();
    // Even a refused action consumes a short debounce (no zero-time busy loop).
    const float delay = action.interval;
    m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(delay);
    m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + delay;
}
void CModularWeaponContext::ContinueBurst() {
    if (!m_burstRemaining || !CanAttack(m_flNextPrimaryAttack)) return;
    const auto* def = Definition();
    if (!def || m_fInReload) { CancelBurst(); return; }
    const bool secondary = (m_modularFlags & kBurstSecondary) != 0;
    const auto& action = secondary ? def->secondary : def->primary;
    if (!FireAction(action, secondary)) {
        CancelBurst();
        if (m_fFireOnEmpty) PlayEmptySound();
        return;
    }
    --m_burstRemaining;
    if (m_burstRemaining == 0 || m_iClip < action.ammoCost) CancelBurst();
    m_flNextPrimaryAttack = GetNextPrimaryAttackDelay(action.interval);
    m_flNextSecondaryAttack = m_pLayer->GetWeaponTimeBase(UsePredicting()) + action.interval;
}
void CModularWeaponContext::PrimaryAttack() { Trigger(false); }
void CModularWeaponContext::SecondaryAttack() { Trigger(true); }
void CModularWeaponContext::Reload() {
#ifdef CLIENT_DLL
    extern bool ModularClientDefinitionsVerified();
    if (!ModularClientDefinitionsVerified()) return;
#endif
    const auto* def = Definition();
    if (!def || m_fInReload) return;
    // Reject invalid indices before any array access (network/save corruption).
    if (m_selectedCartridgeIndex < 0 ||
        m_selectedCartridgeIndex >= static_cast<int>(def->compatible.size())) return;
    // Ensure both client and server resolve the selected ammo reserve by ID.
    m_iSecondaryAmmoType = m_pLayer->FindAmmoType(
        def->compatible[m_selectedCartridgeIndex].cartridgeId.c_str());
    if (m_iClip > 0 && m_selectedCartridgeIndex != m_loadedCartridgeIndex)
        return; // do not silently mix cartridges when topping up
    if (m_iClip == 0) {
        if (m_selectedCartridgeIndex < 0 ||
            m_selectedCartridgeIndex >= static_cast<int>(def->compatible.size())) return;
        // Client prediction uses the last announced reserve index until J.4's
        // extended reserve sync; server owns authoritative ammo selection.
        const int ammoType = m_pLayer->FindAmmoType(
            def->compatible[m_selectedCartridgeIndex].cartridgeId.c_str());
        if (ammoType <= 0 || ammoType >= MAX_AMMO_SLOTS) return;
        m_iPrimaryAmmoType = ammoType;
    }
    if (m_iClip >= def->clipSize || m_pLayer->GetPlayerAmmo(m_iPrimaryAmmoType) <= 0) return;
    PinAssets(modular::AssetAction::Reload, m_pLayer->GetRandomSeed());
    if (DefaultReload(def->clipSize, def->reloadAnimation, def->reloadSeconds)) {
        CancelBurst();
#ifndef CLIENT_DLL
        CBasePlayerWeapon* entity = m_pLayer->GetWeaponEntity();
        const auto sound = modular::SelectAsset(*def,
            AssetsFor(modular::AssetAction::Reload, m_assetSeed), "reload_sound");
        if (entity && entity->m_pPlayer && !sound.empty())
            EMIT_SOUND(entity->m_pPlayer->edict(), CHAN_ITEM, sound.c_str(), 1.f, ATTN_NORM);
#endif
    }
}
void CModularWeaponContext::OnPreItemPostFrame() {
    ApplyPinnedAssets();
    // Must run before CBaseWeaponContext clears IN_ATTACK2: clearing a latched
    // semi-auto secondary action in WeaponIdle would turn it into full-auto.
    if (!m_pLayer->CheckPlayerButtonFlag(IN_ATTACK)) m_modularFlags &= ~kPrimaryLatch;
    if (!m_pLayer->CheckPlayerButtonFlag(IN_ATTACK2)) m_modularFlags &= ~kSecondaryLatch;
}
void CModularWeaponContext::WeaponIdle() {
    const auto* def = Definition();
    if (m_burstRemaining) ContinueBurst();
    ResetEmptySound();
    if (def && m_flTimeWeaponIdle <= m_pLayer->GetWeaponTimeBase(UsePredicting())) {
        if (def->idleAnimation >= 0 && !m_burstRemaining) SendWeaponAnim(def->idleAnimation);
        m_flTimeWeaponIdle = m_pLayer->GetWeaponTimeBase(UsePredicting()) + def->idleSeconds;
    }
}
