"""Portable source contract checks, not a substitute for an engine runtime test."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
def source(path): return (ROOT/path).read_text()

def main():
    combat = source('server/modular_ballistics.cpp')
    weapons = source('server/weapons.cpp')
    registry = source('game_shared/modular_weapons.cpp')
    npc = source('server/modular_npc.cpp')
    mod_weapon = source('server/weapons/weapon_modular.cpp')
    assert 'const int previousTraceFlags = gpGlobals->trace_flags' in combat
    assert 'gpGlobals->trace_flags = previousTraceFlags' in combat
    assert 'ClearMultiDamage(); // delayed projectile' in combat
    assert 's.tracerPosition=GamePoint(s.state.position)' in combat and 'shot.tracerPosition = to' in combat
    assert 'ValidState(s.state,s.physics,s.maxMeters,s.originalEnergy)' in combat
    assert 'registry.ValidateForRuntime(MAX_AMMO_SLOTS, giAmmoIndex' in weapons
    assert 'registry.DisableRuntime(admissionError)' in weapons
    assert 'cartridge duplicates an existing/other ammo name' in registry
    assert 'SpawnModularBallisticShot' in npc
    assert 'LINK_ENTITY_TO_CLASS(weapon_modular, CModularWeapon)' in mod_weapon
    for legacy in ('weapon_9mmhandgun', 'weapon_9mmAR', 'weapon_357', 'weapon_shotgun'):
        assert f'UTIL_PrecacheOtherWeapon( "{legacy}" )' in weapons, legacy
    for specialized in ('gauss','egon','rpg','crossbow','shotgun'):
        assert (ROOT/'game_shared/weapons'/f'{specialized}.cpp').is_file()
    assert (ROOT/'tools/weapon_editor/launch.py').exists()
    assert (ROOT/'documentation/examples/modular-legacy-profiles/README.md').exists()
    print('PASS J.6 source contracts: collision flags, delayed damage, admission, NPC, legacy preserved')

if __name__=='__main__': main()
