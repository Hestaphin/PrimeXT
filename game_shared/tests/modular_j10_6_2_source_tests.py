"""Save-field and shared CPU/pickup integration checks; behavior has separate drivers."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
read=lambda path:(ROOT/path).read_text()
weapon=read('server/weapons/weapon_modular.cpp')
rules=read('server/gamerules/gamerules.cpp')
player=read('server/player.cpp')
generic=read('server/weapons.cpp')
draw=read('client/render/gl_studio_draw.cpp')
init=read('client/render/gl_studio_init.cpp')
for field in ('m_spawnCartridge, FIELD_STRING','m_spawnRounds, FIELD_INTEGER',
    'm_definitionId, FIELD_STRING','m_weaponUuid, FIELD_STRING','m_magazineUuid, FIELD_STRING'):
    assert 'DEFINE_FIELD('+field+')' in weapon
respawn=weapon[weapon.index('CBaseEntity* Respawn() override'):weapon.index('private:')]
assert respawn.index('replacement->m_definitionId') < respawn.index('DispatchSpawn(entity)')
assert 'replacement->m_weaponUuid =' not in respawn and 'replacement->m_magazineUuid =' not in respawn
assert 'replacement->m_spawnCartridge = m_spawnCartridge;' in respawn
assert 'replacement->m_spawnRounds = m_spawnRounds;' in respawn
assert 'pWeapon->DuplicateAmmoCapacity(pPlayer)' in rules
assert 'return !pPlayer->HasPlayerItem(pWeapon) || typedCapacity > 0;' in rules
assert 'TryAddPlayerItem(pItem) == ItemPickupResult::Attached' in player
assert 'pItem->AddDuplicateResult(owned)' in player
assert 'm_rgAmmo[i] -= granted;' in generic and 'if (IsEmpty())' in generic
assert 'item->m_pPlayer = nullptr;' in generic
ensure=init[init.index('CStudioModelRenderer::ModelInstance_t* CStudioModelRenderer::StudioEnsureCpuInstance'):
    init.index('bool CStudioModelRenderer::StudioSetupInstance')]
assert 'RI->' not in ensure and 'UpdateInstanceMaterials' not in ensure
assert 'entity->curstate.messagenum < inst.m_cpuLastMessage' in ensure
assert 'inst.m_cpuModelHeader != hdr' in ensure
for name in ('StudioSetBonesExternal','StudioSetRagdollPose','StudioSetPartPose'):
    begin=draw.index('void CStudioModelRenderer'+('::' if name=='StudioSetPartPose' else ' :: ')+name)
    end=draw.find('\nvoid CStudioModelRenderer',begin+5)
    assert 'StudioEnsureCpuInstance' in draw[begin:end]
    assert 'RI->' not in draw[begin:end]
assert 'StudioPreparePoseParameters(setup,*e,dadt,inst.m_poseparameter)' in draw
assert 'StudioApplyRagdollPoseCpu(inst,hdr,*e,root,now,world)' in draw
assert 'StudioApplyPartPosesCpu(inst,hdr,*e,root,now,world)' in draw
print('PASS J.10.6.2 save fields, authored factory, typed results and CPU initialization wiring')
