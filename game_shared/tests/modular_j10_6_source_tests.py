"""J.10.6 integration contracts; checks do not replace live PhysX/game tests."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
ballistics = (root / 'server/modular_ballistics.cpp').read_text()
# The pool must be stable before any TraceAttack can synchronously add shots.
assert 'm_shots.reserve(kMaxActive);' in ballistics
assert ballistics.count('m_shots.reserve(kMaxActive);') == 2
assert 'int(m_shots.size())>=kMaxActive' in ballistics
assert 'Impact(shot,tr,body,from,to,contact);' in ballistics
assert 'shot.lastSimulatedTime=gpGlobals->time;' in ballistics
# Staged magazine identity / automatic ragdoll / turret contracts.
weapon = (root / 'server/weapons/weapon_modular.cpp').read_text()
assert 'DEFINE_FIELD(m_weaponUuid, FIELD_STRING)' in weapon
assert 'DEFINE_FIELD(m_magazineUuid, FIELD_STRING)' in weapon
assert 'context->BindDefinition(definition, false)' in weapon
combat = (root / 'server/combat.cpp').read_text()
assert 'sv_auto_death_ragdoll' in combat and 'EnableRagdoll( true )' in combat
npc = (root / 'server/modular_npc.cpp').read_text()
assert 'modular_ammo::NpcShotCost' in npc
print('PASS J.10.6 runtime-storage and integration source contracts')
