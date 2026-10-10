// Map entities: weapon_modular (weapon_id), ammo_modular (cartridge_id, amount)
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"
#include "server_weapon_layer_impl.h"
#include "modular_weapons.h"
#include "modular_ammo_policy.h"
#include "modular_item_identity.h"
#include "weapons/modular.h"
#include <cstring>
#include <memory>

class CModularWeapon final : public CBasePlayerWeapon {
    DECLARE_CLASS(CModularWeapon, CBasePlayerWeapon);
public:
    CModularWeapon() {
        m_pWeaponContext = std::make_unique<CModularWeaponContext>(
            std::make_unique<CServerWeaponLayerImpl>(this), nullptr);
    }
    DECLARE_DATADESC();
    // The legacy ExtractAmmo/AddPrimaryAmmo path assumes every magazine uses
    // pszAmmo1, which is only the DEFAULT cartridge of a modular weapon.
    // Keep cartridge identity explicit for both first pickups and duplicates.
    int AddToPlayer(CBasePlayer* player) override {
        if (!player || !m_definition || !CBasePlayerItem::AddToPlayer(player)) return FALSE;
        auto* context = static_cast<CModularWeaponContext*>(m_pWeaponContext.get());
        player->AddWeapon(context->m_iId);
        if (context->m_iDefaultAmmo > 0) {
            const int amount = context->m_iDefaultAmmo;
            const int toClip = modular_ammo::SpawnClipRounds(
                context->m_iClip, m_definition->clipSize, amount);
            if (toClip > 0) {
                // A new, empty weapon starts with its authored default cartridge.
                for (size_t i = 0; i < m_definition->compatible.size(); ++i)
                    if (m_definition->compatible[i].cartridgeId == m_definition->defaultCartridgeId)
                        context->m_loadedCartridgeIndex = static_cast<int>(i);
                context->m_iClip = toClip;
            }
            if (amount > toClip)
                GrantReserve(player, m_definition->defaultCartridgeId.c_str(), amount-toClip);
            context->m_iDefaultAmmo = 0;
        }
        // A dropped gun already contains physical rounds of the saved type.
        // Neither transfer nor reserve mapping may relabel them as default ammo.
        const int loaded = context->m_loadedCartridgeIndex;
        const int selected = context->m_selectedCartridgeIndex;
        if (loaded >= 0 && loaded < static_cast<int>(m_definition->compatible.size()))
            context->m_iPrimaryAmmoType = player->GetAmmoIndex(
                m_definition->compatible[loaded].cartridgeId.c_str());
        if (selected >= 0 && selected < static_cast<int>(m_definition->compatible.size()))
            context->m_iSecondaryAmmoType = player->GetAmmoIndex(
                m_definition->compatible[selected].cartridgeId.c_str());
        return TRUE;
    }

    int DuplicateAmmoCapacity(const CBasePlayer* player) const override {
        if (!player || !m_definition) return 0;
        const auto* source = static_cast<const CModularWeaponContext*>(m_pWeaponContext.get());
        const char* defaultType = m_definition->defaultCartridgeId.c_str();
        int defaultGranted = 0;
        if (source->m_iDefaultAmmo > 0) {
            defaultGranted = Q_min(source->m_iDefaultAmmo, ReserveCapacity(player, defaultType));
            // The transfer path reaches the magazine only after spawn ammo is
            // exhausted. Report exactly the same reachable offer here.
            if (defaultGranted < source->m_iDefaultAmmo) return defaultGranted;
        }
        if (source->m_iClip <= 0 || source->m_loadedCartridgeIndex < 0 ||
            source->m_loadedCartridgeIndex >= static_cast<int>(m_definition->compatible.size()))
            return defaultGranted;
        const char* loaded = m_definition->compatible[source->m_loadedCartridgeIndex].cartridgeId.c_str();
        int capacity = ReserveCapacity(player, loaded);
        if (!strcmp(defaultType, loaded)) capacity = Q_max(0, capacity-defaultGranted);
        return defaultGranted + Q_min(source->m_iClip, capacity);
    }
    int AddDuplicate(CBasePlayerItem* ownedItem) override {
        return AddDuplicateResult(ownedItem) == ItemPickupResult::Consumed;
    }
    ItemPickupResult AddDuplicateResult(CBasePlayerItem* ownedItem) override {
        if (!ownedItem || !m_definition) return ItemPickupResult::Rejected;
        ItemInfo info{};
        if (!ownedItem->GetItemInfo(&info) || info.iId != m_definition->runtimeId)
            return ItemPickupResult::Rejected;
        auto* owned = static_cast<CBasePlayerWeapon*>(ownedItem);
        CBasePlayer* player = owned->m_pPlayer;
        if (!player) return ItemPickupResult::Rejected;
        auto* source = static_cast<CModularWeaponContext*>(m_pWeaponContext.get());
        int transferred = 0;
        // Placed-map spawn ammo is of the authored DEFAULT type; it must never
        // refill an existing magazine loaded with a different cartridge.
        if (source->m_iDefaultAmmo > 0) {
            const int granted = GrantReserve(player, m_definition->defaultCartridgeId.c_str(),
                                             source->m_iDefaultAmmo);
            source->m_iDefaultAmmo -= granted;
            transferred += granted;
        }
        // A player-dropped weapon instead (or also) carries a typed magazine.
        // Its rounds go to the matching reserve, not to the recipient's clip.
        if (source->m_iDefaultAmmo == 0 && source->m_iClip > 0 &&
            source->m_loadedCartridgeIndex >= 0 &&
            source->m_loadedCartridgeIndex < static_cast<int>(m_definition->compatible.size())) {
            const char* cartridge = m_definition->compatible[source->m_loadedCartridgeIndex].cartridgeId.c_str();
            const int granted = GrantReserve(player, cartridge, source->m_iClip);
            source->m_iClip -= granted;
            transferred += granted;
        }
        // If carrying capacity accepts only part, leave the remainder in the
        // world item for subsequent pickup, rather than destroying the rounds.
        if (transferred <= 0) return ItemPickupResult::Rejected;
        return source->m_iDefaultAmmo == 0 && source->m_iClip <= 0
            ? ItemPickupResult::Consumed : ItemPickupResult::Retained;
    }

    // Rebind to the registry after the generic entity/weapon state is restored.
    // The save records definition identity and independently selected/loaded
    // cartridge indexes, not raw definition pointers.
    int Restore(CRestore& restore) override {
        const int status = BaseClass::Restore(restore);
        if (!status) return status;
        // Older save files do not contain instance identities. Assign them
        // once; never regenerate identities from a valid restored snapshot.
        EnsureInstanceIds();
        const char* id = FStringNull(m_definitionId) ? "" : STRING(m_definitionId);
        const modular::Weapon* definition = modular::SharedRegistry().FindWeapon(id);
        auto* context = static_cast<CModularWeaponContext*>(m_pWeaponContext.get());
        if (!definition || context->m_iId != definition->runtimeId) {
            ALERT(at_warning, "weapon_modular restore: definition mismatch '%s'\n", id);
            UTIL_Remove(this);
            return status;
        }
        const int loaded = context->m_loadedCartridgeIndex;
        const int selected = context->m_selectedCartridgeIndex;
        const int flags = context->m_modularFlags;
        const int burst = context->m_burstRemaining;
        m_definition = definition;
        context->BindDefinition(definition, false); // saved m_iDefaultAmmo must not be reset
        const int count = static_cast<int>(definition->compatible.size());
        if (loaded < 0 || loaded >= count || selected < 0 || selected >= count ||
            burst < 0 || burst > 16 || flags < 0 || flags > 7) {
            ALERT(at_warning, "weapon_modular restore: damaged cartridge/trigger state for '%s'\n", id);
            UTIL_Remove(this);
            return status;
        }
        context->m_loadedCartridgeIndex = loaded;
        context->m_selectedCartridgeIndex = selected;
        context->m_modularFlags = flags;
        context->m_burstRemaining = burst;
        // Clip type and next-reload selection must be resolved separately.
        context->m_iPrimaryAmmoType = CBasePlayer::GetAmmoIndex(
            definition->compatible[loaded].cartridgeId.c_str());
        context->m_iSecondaryAmmoType = CBasePlayer::GetAmmoIndex(
            definition->compatible[selected].cartridgeId.c_str());
        return status;
    }
    void KeyValue(KeyValueData* kv) override {
        if (!strcmp(kv->szKeyName, "weapon_id")) {
            m_definitionId = ALLOC_STRING(kv->szValue);
            kv->fHandled = TRUE;
        } else if (!strcmp(kv->szKeyName, "loaded_cartridge")) {
            m_spawnCartridge = ALLOC_STRING(kv->szValue);
            kv->fHandled = TRUE;
        } else if (!strcmp(kv->szKeyName, "loaded_rounds")) {
            m_spawnRounds = atoi(kv->szValue);
            kv->fHandled = TRUE;
        } else if (!strcmp(kv->szKeyName, "paint_profile")) {
            // Cosmetic profile metadata can survive pickup and save/restore;
            // renderer-side decals/paint application is a separate feature.
            m_paintProfile = ALLOC_STRING(kv->szValue);
            kv->fHandled = TRUE;
        } else CBasePlayerWeapon::KeyValue(kv);
    }
    void Spawn() override {
        EnsureInstanceIds();
        const char* id = FStringNull(m_definitionId) ? "" : STRING(m_definitionId);
        m_definition = modular::SharedRegistry().FindWeapon(id);
        if (!m_definition ||
            CBaseWeaponContext::ItemInfoArray[m_definition->runtimeId].iId != m_definition->runtimeId) {
            ALERT(at_warning, "weapon_modular: missing or unregistered weapon_id '%s'\n", id);
            UTIL_Remove(this);
            return;
        }
        auto* context = static_cast<CModularWeaponContext*>(m_pWeaponContext.get());
        context->BindDefinition(m_definition);
        // An NPC-dropped gun is a *used* magazine, not a fresh world spawn.
        // Apply its exact cartridge and remaining count before it can be picked up.
        if (!FStringNull(m_spawnCartridge)) {
            int cartridgeIndex = -1;
            for (size_t i = 0; i < m_definition->compatible.size(); ++i)
                if (m_definition->compatible[i].cartridgeId == STRING(m_spawnCartridge))
                    cartridgeIndex = static_cast<int>(i);
            if (cartridgeIndex < 0 || m_spawnRounds < 0 || m_spawnRounds > m_definition->clipSize) {
                ALERT(at_warning, "weapon_modular: invalid dropped cartridge/magazine\n");
                UTIL_Remove(this);
                return;
            }
            context->m_loadedCartridgeIndex = context->m_selectedCartridgeIndex = cartridgeIndex;
            context->m_iClip = m_spawnRounds;
            context->m_iDefaultAmmo = 0;
            context->m_iPrimaryAmmoType = CBasePlayer::GetAmmoIndex(STRING(m_spawnCartridge));
            context->m_iSecondaryAmmoType = context->m_iPrimaryAmmoType;
        }
        Precache();
        SET_MODEL(edict(), m_definition->worldModel.c_str());
        FallInit();
    }
    void Precache() override {
        if (m_definition) {
            PRECACHE_MODEL(m_definition->viewModel.c_str());
            PRECACHE_MODEL(m_definition->playerModel.c_str());
            PRECACHE_MODEL(m_definition->worldModel.c_str());
        }
    }
    CBaseEntity* Respawn() override {
        const char* id = FStringNull(m_definitionId) ? "" : STRING(m_definitionId);
        if (!modular::SharedRegistry().FindWeapon(id)) return nullptr;
        // Create() dispatches Spawn immediately. Install the immutable spawn
        // recipe before dispatching it; never copy the consumed runtime clip.
        CBaseEntity* created = CreateEntityByName(const_cast<char*>(STRING(pev->classname)));
        if (!created) return nullptr;
        CModularWeapon* replacement = static_cast<CModularWeapon*>(created);
        replacement->m_definitionId = m_definitionId;
        replacement->m_spawnCartridge = m_spawnCartridge;
        replacement->m_spawnRounds = m_spawnRounds;
        replacement->m_paintProfile = m_paintProfile;
        replacement->pev->owner = pev->owner;
        replacement->pev->spawnflags = pev->spawnflags;
        replacement->pev->body = pev->body;
        replacement->pev->skin = pev->skin;
        replacement->pev->scale = pev->scale;
        replacement->pev->target = pev->target;
        replacement->pev->targetname = pev->targetname;
        replacement->SetAbsOrigin(g_pGameRules->VecWeaponRespawnSpot(this));
        replacement->SetAbsAngles(GetAbsAngles());
        replacement->m_fSetAngles = TRUE;
        edict_t* entity = replacement->edict();
        const int spawnStatus = DispatchSpawn(entity);
        replacement = static_cast<CModularWeapon*>(CBaseEntity::Instance(entity));
        if (spawnStatus < 0 || !replacement || (replacement->pev->flags & FL_KILLME)) {
            if (replacement) UTIL_Remove(replacement);
            return nullptr;
        }
        replacement->pev->effects |= EF_NODRAW;
        replacement->SetTouch(NULL);
        replacement->SetThink(&CBasePlayerItem::AttemptToMaterialize);
        replacement->pev->nextthink = g_pGameRules->FlWeaponRespawnTime(this);
        return replacement;
    }
private:
    static int ReserveCapacity(const CBasePlayer* player, const char* cartridge) {
        if (!player || !cartridge) return 0;
        const int index = CBasePlayer::GetAmmoIndex(cartridge);
        const int limit = modular::SharedRegistry().AmmoReserveLimit(cartridge);
        if (index <= 0 || index >= MAX_AMMO_SLOTS || limit <= 0) return 0;
        return Q_max(0, limit-Q_max(0, player->m_rgAmmo[index]));
    }
    void EnsureInstanceIds() {
        if (FStringNull(m_weaponUuid) || !modular_identity::IsUuid(STRING(m_weaponUuid)))
            m_weaponUuid = ALLOC_STRING(modular_identity::NewUuid().c_str());
        if (FStringNull(m_magazineUuid) || !modular_identity::IsUuid(STRING(m_magazineUuid)))
            m_magazineUuid = ALLOC_STRING(modular_identity::NewUuid().c_str());
    }
    static int GrantReserve(CBasePlayer* player, const char* cartridge, int amount) {
        if (!player || !cartridge || amount <= 0) return 0;
        const int index = player->GetAmmoIndex(cartridge);
        const int limit = modular::SharedRegistry().AmmoReserveLimit(cartridge);
        if (index <= 0 || index >= MAX_AMMO_SLOTS || limit <= 0) return 0;
        const int before = player->m_rgAmmo[index];
        player->GiveAmmo(amount, const_cast<char*>(cartridge), limit);
        // GiveAmmo can return a positive slot even when zero rounds were added.
        const int after = player->m_rgAmmo[index];
        return modular_ammo::ActualGranted(before, after, amount);
    }
    // Physical instance IDs, distinct from the .weapon/.cartridge definition.
    // Persist across pickups and in-map save/restore. Cross-level transfer
    // requires a later explicit inventory snapshot/restore integration.
    string_t m_weaponUuid = iStringNull;
    string_t m_magazineUuid = iStringNull;
    string_t m_paintProfile = iStringNull;
    string_t m_definitionId = iStringNull;
    // Authored spawn recipe, retained independently of mutable runtime ammo.
    // Old saves default to the definition's ordinary default-ammo spawn.
    string_t m_spawnCartridge = iStringNull;
    int m_spawnRounds = 0;
    const modular::Weapon* m_definition = nullptr;
};
LINK_ENTITY_TO_CLASS(weapon_modular, CModularWeapon);
BEGIN_DATADESC(CModularWeapon)
    DEFINE_FIELD(m_definitionId, FIELD_STRING),
    DEFINE_FIELD(m_weaponUuid, FIELD_STRING),
    DEFINE_FIELD(m_magazineUuid, FIELD_STRING),
    DEFINE_FIELD(m_paintProfile, FIELD_STRING),
    DEFINE_FIELD(m_spawnCartridge, FIELD_STRING),
    DEFINE_FIELD(m_spawnRounds, FIELD_INTEGER),
#define MODULAR_CONTEXT_FIELD(member) \
    DEFINE_CUSTOM_FIELD(member, FIELD_INTEGER, \
        [](CBaseEntity* entity, void* bytes, size_t size) { \
            const auto* context = static_cast<CModularWeaponContext*>( \
                static_cast<CModularWeapon*>(entity)->m_pWeaponContext.get()); \
            std::memcpy(bytes, &context->member, size); \
        }, \
        [](CBaseEntity* entity, const void* bytes, size_t size) { \
            auto* context = static_cast<CModularWeaponContext*>( \
                static_cast<CModularWeapon*>(entity)->m_pWeaponContext.get()); \
            std::memcpy(&context->member, bytes, size); \
        })
    MODULAR_CONTEXT_FIELD(m_loadedCartridgeIndex),
    MODULAR_CONTEXT_FIELD(m_selectedCartridgeIndex),
    MODULAR_CONTEXT_FIELD(m_modularFlags),
    MODULAR_CONTEXT_FIELD(m_burstRemaining),
#undef MODULAR_CONTEXT_FIELD
END_DATADESC()

class CModularAmmo final : public CBasePlayerAmmo {
    DECLARE_CLASS(CModularAmmo, CBasePlayerAmmo);
public:
    DECLARE_DATADESC();
    void KeyValue(KeyValueData* kv) override {
        if (!strcmp(kv->szKeyName, "cartridge_id")) {
            m_cartridgeId = ALLOC_STRING(kv->szValue); kv->fHandled = TRUE;
        } else if (!strcmp(kv->szKeyName, "amount")) {
            m_amount = atoi(kv->szValue); kv->fHandled = TRUE;
        } else CBasePlayerAmmo::KeyValue(kv);
    }
    void Spawn() override {
        const char* cartridge = FStringNull(m_cartridgeId) ? "" : STRING(m_cartridgeId);
        if (!modular::SharedRegistry().FindCartridge(cartridge) || m_amount <= 0 || m_amount > 255) {
            ALERT(at_warning, "ammo_modular: invalid cartridge_id or amount\n");
            UTIL_Remove(this); return;
        }
        // Explicit default until cartridge world-model assets are introduced.
        PRECACHE_MODEL("models/w_9mmclip.mdl");
        SET_MODEL(edict(), "models/w_9mmclip.mdl");
        CBasePlayerAmmo::Spawn();
    }
    BOOL AddAmmo(CBaseEntity* other) override {
        if (!other || !other->IsPlayer()) return FALSE;
        // J.3: preserve cartridge identity all the way into the player's
        // reserve ammo array. Each cartridge has its own registered ammo name.
        const char* id = STRING(m_cartridgeId);
        const int limit = modular::SharedRegistry().AmmoReserveLimit(id);
        if (limit <= 0) return FALSE;
        CBasePlayer* player = static_cast<CBasePlayer*>(other);
        const int index = player->GetAmmoIndex(id);
        if (index <= 0 || index >= MAX_AMMO_SLOTS) return FALSE;
        const int before = player->m_rgAmmo[index];
        player->GiveAmmo(m_amount, const_cast<char*>(id), limit);
        // The legacy GiveAmmo API can return an ID when *no* ammo was added.
        return modular_ammo::ActualGranted(before, player->m_rgAmmo[index], m_amount) > 0;
    }
private:
    string_t m_cartridgeId = iStringNull;
    int m_amount = 1;
};
LINK_ENTITY_TO_CLASS(ammo_modular, CModularAmmo);

BEGIN_DATADESC(CModularAmmo)
    DEFINE_FIELD(m_cartridgeId, FIELD_STRING),
    DEFINE_FIELD(m_amount, FIELD_INTEGER),
END_DATADESC()
