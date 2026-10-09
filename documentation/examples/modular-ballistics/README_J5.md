# PrimeXT J.5 — Persistence, Validation & Standalone Weapon/Cartridge Editor

J.5 applies after J.4, without modifying Xash3D or `pxmv`.

## Install and run editor

The editor is an independent Tk desktop application (`tools/weapon_editor`).
It works without launching the game and shares the **real C++ modular registry parser**
by building the separate `primext-weapon-validate` console executable.

```sh
cmake -S tools/weapon_editor -B tools/weapon_editor/build
cmake --build tools/weapon_editor/build
python3 tools/weapon_editor/launch.py /path/to/mod
```

On Windows, use `py tools/weapon_editor/launch.py <mod-dir>` after building
`primext-weapon-validate.exe` using CMake; a distributable binary can be built
using a Python packager such as PyInstaller (not required for running from source).

The **Weapon** pages edit common state, action behavior/modes, NPC usability,
animations, sounds, sprites and weapon models. The **Cartridge** page edits
caliber/chamber, bullet type, projectile mass/diameter/count, aerodynamic drag,
reference velocity and base damage. The **Compatibility** page enables the
cartridges usable by the selected weapon, edits muzzle velocity for each pairing
and selects a default cartridge. One cartridge can be used by multiple guns.

**Save / Export** creates `scripts/weapons/*.weapon`,
`scripts/cartridges/*.cartridge`, and their ordered `manifest.txt` files. A
staging directory is checked with the actual C++ `modular::Registry` validation
**before** updating the mod directory. IDs and names follow the J.4 schema.

Ballistic test shows independent gravity/drag flight. The model tab reads model
header/bounds and sequence labels from a GoldSrc Studio v10 `.mdl` without
launching `pxmv`; it is a **bounding-box visualization, not rendered mesh**.
It cannot render textured or skinned animated surfaces in J.5.

## Runtime persistence

- `weapon_modular` saves the weapon definition ID and four additional context
  fields: loaded cartridge index, next-reload selection, trigger flags and
  remaining burst shots. Existing weapon clip/ammo timers come from base class.
  `m_fInReload` is now included in saved base-weapon context fields.
- On restore, the weapon definition is looked up by ID; the saved numeric
  runtime ID and cartridge indices are verified before reconstructing both
  primary loaded-ammo and selected next-reload ammo types.
- `modular_ballistic_projectile` now persists owner handle, ballistic state,
  physical constants, time/distance, damage, tracer state and the weapon registry
  fingerprint. It re-arms its Think callback and active-projectile counter after
  restore, rather than simulating the time spent loading.
- Projectile restoration is refused if the definition fingerprint differs,
  state is malformed, owner is invalid or count limit is exceeded.
- Spawning new projectiles additionally verifies the weapon/cartridge are
  server-registered references, that ammunition is compatible, the muzzle
  velocity matches the definition, that projectile count is within configured
  per-trigger maximums and that the existing 256 active-projectile budget holds.

## Source-level tests

```sh
python3 -m unittest discover -s tools/weapon_editor/tests -v
```

The portable C++ modular-registry tests from J.2 and J.4, the J.3 trajectory
unit test and changed translation-unit syntax checks should be rerun.

## Known limitations / J.6 responsibilities

- Full PhysX-linked gameplay and multiplayer restore tests have **not** yet been
  demonstrated. The J.5 portable tests cannot prove save/restore correctness in
  the engine, especially entity restore-order interactions.
- The model/animation preview is metadata and an interactive rotated 3D bounds
  diagram; it does not render mesh triangles, materials, bone animation or actual
  muzzle attachment positions. A true GPU/model viewer is future tool work.
- Mixed-cartridge magazines, discrete chambers, universal NPC/turret schedules,
  projectile interpolation/lag compensation and an expanded Xash network ID
  protocol are not finished.
- Definitions must be identical between server and client and must not change
  between saving and restoring a game. Fingerprint mismatch rejects active
  projectiles; modular weapons are matched by ID/runtime ID, not a fully
  versioned save manifest.
- This release's desktop UI requires Python + Tk. It is a **separate program**
  from `pxmv`, but a prebuilt platform-specific `.exe` is not included in the
  source patch. The CMake validator is a native compiled executable.
- The editor uses a staging/validation scheme to avoid invalid exports; it is
  not an ACID transaction across multiple files if the process crashes while
  replacing them. Keep definitions under version control.
