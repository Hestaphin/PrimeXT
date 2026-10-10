#!/usr/bin/env python3
"""Actual pickup/box/modular/respawn methods with explicit entity/service adapters.

Uses the real cartridge/weapon registry, ammo policy and UUID generator. Does not
replace SDK linking or engine save-file/runtime acceptance.
"""
from pathlib import Path
import os
import subprocess
import tempfile
from pose_j10_6_2_integration import function

ROOT=Path(__file__).resolve().parents[2]

def main():
    weapon=(ROOT/'server/weapons/weapon_modular.cpp').read_text()
    generic=(ROOT/'server/weapons.cpp').read_text()
    player=(ROOT/'server/player.cpp').read_text()
    rules=(ROOT/'server/gamerules/gamerules.cpp').read_text()
    multiplayer=(ROOT/'server/gamerules/multiplay_gamerules.cpp').read_text()
    shared=(ROOT/'game_shared/weapons/modular.cpp').read_text()
    # Preserve the complete production modular class. Only visibility is opened
    # for test setup and its engine class/registration macros are adapted.
    modular=function(weapon,'class CModularWeapon final')+';'
    modular=modular.replace('private:','public:')
    methods=[modular,
        function(shared,'void CModularWeaponContext::BindDefinition'),
        function(generic,'bool SameInventoryWeaponType'),
        function(generic,'int MaxAmmoCarry'),
        function(rules,'BOOL CGameRules::CanHaveAmmo'),
        function(rules,'BOOL CGameRules::CanHavePlayerItem'),
        function(multiplayer,'BOOL CHalfLifeMultiplay::CanHavePlayerItem'),
        function(player,'int CBasePlayer::AddPlayerItem'),
        function(player,'ItemPickupResult CBasePlayer::TryAddPlayerItem'),
        function(player,'BOOL CBasePlayer::HasPlayerItem'),
        function(generic,'void CBasePlayerItem::DefaultTouch'),
        function(generic,'void CWeaponBox::Touch'),
        function(generic,'BOOL CWeaponBox::IsEmpty'),
    ]
    fixture=(ROOT/'game_shared/tests/fixtures/j10_6_2/inventory_adapter.cpp.in').read_text()
    with tempfile.TemporaryDirectory(prefix='j1062_inventory_') as tmp:
        tmp=Path(tmp);unit=tmp/'inventory.cpp';binary=tmp/'inventory'
        unit.write_text(fixture.replace('@PRODUCTION@','\n\n'.join(methods)))
        args=[os.environ.get('CXX','c++'),'-std=c++17','-O0','-g',
            '-I'+str(ROOT/'game_shared'),str(unit),str(ROOT/'game_shared/modular_weapons.cpp'),
            '-o',str(binary)]
        for command in [args,[str(binary),str(ROOT/'documentation/examples/modular-weapon')]]:
            result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
            if result.returncode: raise RuntimeError((result.stdout+result.stderr)[-7000:])
            if result.stdout.strip(): print(result.stdout.strip())

if __name__=='__main__': main()
