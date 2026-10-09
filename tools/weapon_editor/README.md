# PrimeXT Weapon & Cartridge Editor — J.10.5

Standalone authoring application, **not** part of `pxmv`. It does not require
launching Xash3D, PrimeXT, or the physics asset editor.

## Requirements and launch

- Python 3.10 or later with Tcl/Tk (`python3-tk` on many Linux distributions).
- C++17 compiler and CMake 3.16+ for the **native authoritative validator**.

```sh
cd PrimeXT-master
cmake -S tools/weapon_editor -B tools/weapon_editor/build
cmake --build tools/weapon_editor/build
python3 tools/weapon_editor/launch.py /path/to/mod-directory
```

On Windows, build the validator with CMake and run `py tools/weapon_editor/launch.py C:\\path\\to\\mod`.
To bundle a standalone `.exe` without asking end-users to install Python,
PyInstaller can package `launch.py` and `app.py`/`project.py` along with the built
`primext-weapon-validate.exe` (PyInstaller is an optional distribution tool and
is **not** bundled here).

## Workflow

1. **Open Mod** to open a mod directory containing `scripts/weapons/manifest.txt`
   and `scripts/cartridges/manifest.txt`. An empty directory is also supported.
2. Create cartridges, edit projectile properties and save them in the same library.
3. Create a weapon and choose its allowed cartridges. Set muzzle velocity separately
   for each allowed cartridge; one cartridge may be used in many different guns.
4. Configure primary and secondary attacks, assets, animation sequence indices,
   player/NPC support (`npc_usable`), magazine and reserve values.
5. Preview ballistic flight using the current default cartridge and muzzle velocity.
   Use **Model / Animations** to inspect a GoldSrc Studio v10 `.mdl` file's sequence
   list and orbit its bounding box in a self-contained preview.
6. Choose **Save / Export**. A complete staging folder is checked using the
   game's actual C++ `modular::Registry` parser before the manifests and files
   are replaced. The editor never writes source code or recompiles weapons.

## Important limits

- This release supports the J.4 registry: weapon IDs 32–62; suit ID 63 reserved;
  existing ammunition slot limit is 32 including built-in ammo. The editor cannot
  automatically expand the Xash network protocol.
- No native weapon extension DLL authoring yet. Data files can use registered
  built-in behaviors (`hitscan`, `ballistic`, `ammo_cycle`, `none`).
- Preview is **not** the game simulation: no wind, impacts, penetration or moving
  targets, and uses default air density. The existing standalone MDL preview shows
  bounds and sequence names, **not** textured/skinned mesh animation.
- J.5 preview/export does not change the game's active weapon definitions while
  running; restart/load the game to pick up new assets.
- Saving is validated before writes but a complete power-failure-atomic transaction
  across multiple files is not possible; keep projects under version control.
- Do not change definition IDs/runtime IDs while attempting to restore a saved game.

## Tests

```sh
python3 -m unittest discover -s tools/weapon_editor/tests -v
```

## J.10.5 validator discovery

The editor now checks `build/primext-weapon-validate.exe` as well as
`build/Release/primext-weapon-validate.exe` and other configurations.
This matches the supported Windows launcher layouts.

Runtime weapon/magazine UUIDs are owned by game weapon instances.
They do not change the editor's authored file schema.
