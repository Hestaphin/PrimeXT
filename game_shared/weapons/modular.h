#pragma once
#include "weapon_context.h"
#include "modular_weapons.h"

// Shared callback: registered from built-in game code before loading manifests.
// Modules provide reusable attacks rather than weapon-specific C++ classes.
using ModularAttackHandler = bool (*)(IWeaponLayer&, const modular::Weapon&,
                                    const modular::Attack&, uint32_t seed);
bool RegisterModularAttackBehavior(const std::string& id, ModularAttackHandler handler);

class CModularWeaponContext final : public CBaseWeaponContext {
public:
    CModularWeaponContext(std::unique_ptr<IWeaponLayer>&& layer, const modular::Weapon* definition);
    int GetItemInfo(ItemInfo* info) const override;
    bool Deploy() override;
    bool CanDeploy() override;
    bool IsUseable() override;
    void Holster() override;
    void PrimaryAttack() override;
    void SecondaryAttack() override;
    void Reload() override;
    void WeaponIdle() override;
    void OnPreItemPostFrame() override;
    void OnReloadFinished() override;
    // Index into Weapon::compatible, independently stored for a loaded magazine
    // and the next cartridge selection; zero-based and networked in iuser3/4.
    int m_loadedCartridgeIndex = 0;
    int m_selectedCartridgeIndex = 0;
    bool ShouldWeaponIdle() override { return true; }
    int iItemSlot() override { const auto* def = Definition(); return def ? def->slot : 0; }
    // Only a newly spawned weapon is entitled to the definition's spawn ammo.
    // Restoring or rebinding a saved weapon must preserve its existing ammo.
    void BindDefinition(const modular::Weapon* definition, bool initializeSpawn = true);
    // Serialized through standard weapon_data_t extra fields on client and server.
    // Bit 0: primary latch, bit 1: secondary latch, bit 2: burst secondary.
    int m_modularFlags = 0;
    int m_burstRemaining = 0;
private:
    const modular::Weapon* Definition() const;
    bool FireAction(const modular::Attack& action, bool secondary);
    void Trigger(bool secondary);
    void ContinueBurst();
    void CancelBurst();
    bool CycleCartridge();
    const modular::Compatibility* LoadedCompatibility() const;
    const modular::Weapon* m_definition;
    uint16_t m_primaryEvent = 0, m_secondaryEvent = 0;
};
