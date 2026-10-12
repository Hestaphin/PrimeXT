"""Real HUD sprite loading/override methods with adapted sprite services."""
from pathlib import Path
import os
import subprocess
import tempfile
from pose_j10_6_2_integration import function

ROOT=Path(__file__).resolve().parents[2]

def main():
    source=(ROOT/'client/ammo.cpp').read_text()
    production='\n'.join(function(source,sig) for sig in [
        'static bool ModularSpriteIsList','static void LoadModularIcon',
        'void WeaponsResource::SetModularSprites','void WeaponsResource :: LoadWeaponSprites'])
    fixture=(ROOT/'game_shared/tests/fixtures/j10_6_9_4/sprite_adapter.cpp.in').read_text()
    with tempfile.TemporaryDirectory(prefix='j10694_sprites_') as directory:
        temporary=Path(directory);unit=temporary/'sprites.cpp';binary=temporary/'sprites'
        unit.write_text(fixture.replace('@PRODUCTION@',production))
        flags=['-fsanitize=undefined','-fno-sanitize-recover=all'] if os.environ.get('J10694_UBSAN') else []
        for command in [[os.environ.get('CXX','c++'),'-std=c++17',*flags,
            '-I'+str(ROOT/'client'),str(unit),'-o',str(binary)],[str(binary)]]:
            result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
            if result.returncode:raise RuntimeError((result.stdout+result.stderr)[-6000:])
            if result.stdout.strip():print(result.stdout.strip(),flush=True)

if __name__=='__main__':main()
