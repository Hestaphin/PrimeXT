"""J.10.3 source-integration checks, supplementing C++ policy regressions."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
get = lambda p: (root / p).read_text()
weapon = get('server/weapons/weapon_modular.cpp')
shared = get('game_shared/weapons/modular.cpp')
npc = get('server/modular_npc.cpp')
grunt = get('server/monsters/hgrunt.cpp')
assert 'int AddDuplicate(CBasePlayerItem* ownedItem) override' in weapon
assert 'int AddToPlayer(CBasePlayer* player) override' in weapon
assert 'source->m_loadedCartridgeIndex].cartridgeId.c_str()' in weapon
assert 'm_definition->defaultCartridgeId.c_str()' in weapon
assert 'source->m_iClip -= granted' in weapon
assert 'source->m_iDefaultAmmo -= granted' in weapon
assert 'source->m_iDefaultAmmo == 0 && source->m_iClip <= 0' in weapon
assert '? ItemPickupResult::Consumed : ItemPickupResult::Retained' in weapon
assert 'player->GetAmmoIndex(' in weapon
assert 'context->BindDefinition(definition, false)' in weapon
assert 'void CModularWeaponContext::BindDefinition(const modular::Weapon* definition, bool initializeSpawn)' in shared
assert 'if (initializeSpawn)' in shared
assert 'bool CModularWeaponContext::CanDeploy() { return IsUseable(); }' in shared
assert 'modular_ammo::ConsiderReserve' in shared
assert 'modular_ammo::NpcShotCost(availableRounds, attack.ammoCost)' in npc
assert 'return attack.ammoCost;' in npc
assert grunt.count('if (consumed > 0) m_cAmmoLoaded -= consumed;') == 2
assert grunt.count('DropModularNpcWeapon(STRING(m_iszModularWeapon)') == 2
assert 'context->m_iDefaultAmmo = 0;' in weapon
assert 'context->m_loadedCartridgeIndex = context->m_selectedCartridgeIndex = cartridgeIndex' in weapon
print('PASS J.10.3 source integration contracts')
