from pathlib import Path
root = Path(__file__).resolve().parents[2]
combat = (root/'server/combat.cpp').read_text()
turret = (root/'server/monsters/baseturret.cpp').read_text()
identity = (root/'server/weapons/weapon_modular.cpp').read_text()
for variant in ('turret', 'miniturret', 'sentry'):
    assert 'TryModularShoot(vecSrc, vecDirToEnemy)' in (root/f'server/monsters/{variant}.cpp').read_text()
assert 'sv_auto_death_ragdoll' in combat and 'ShouldGibMonster( iGib )' in combat
assert 'm_useModularWeapon' in turret and 'modular_rounds' in turret
assert 'EnsureInstanceIds()' in identity
assert 'DEFINE_FIELD(m_weaponUuid, FIELD_STRING)' in identity
assert 'DEFINE_FIELD(m_magazineUuid, FIELD_STRING)' in identity
assert 'DEFINE_FIELD(m_paintProfile, FIELD_STRING)' in identity
print('J.10.5 wiring checks passed')
