# PrimeXT Milestone J.1 — foundation for modular weapons (formerly J.1–J.7)

Baseline: original PrimeXT + Xash3D ZIPs and the I.9.6 combined patch. No changes to Xash3D in this phase.

## Shipped in J.1

- Shared, dependency-free C++17 v1 loader, deterministic writers and registries for `.weapon` and `.cartridge` definitions.
- Deterministic manifest-based discovery through the engine's virtual-file loader:
  `scripts/weapons/manifest.txt` and `scripts/cartridges/manifest.txt`.
- Strict ID, numeric, version, path, duplicate, compatibility and resource validation.
  A malformed definition rejects the entire registry, not a random subset.
- Client and server build the same definition registry using the same parser.
- Server precaches declared models/sounds/assets, registers generic ItemInfo and
  legacy-compatible ammo names alongside the untouched built-in weapon list.
- New generic player weapon context, which can be reused without a C++ class for
  every new gun. The J.1 firing implementation intentionally uses existing
  PrimeXT hitscan and existing I.9.6 collision/impact code.
- Generic map entities `weapon_modular` and `ammo_modular`.
- Portable C++ unit/regression test and sample files.

## Installation / authoring

Copy the contents of `documentation/examples/modular-weapon/` to your mod directory.
Every manifest line contains one bare asset ID (not a filename). The editor in
J.5 will generate both the definitions and manifests. Files have no automatic
native-code loading. To activate the sample in a map, place a `weapon_modular`
entity with key `weapon_id` = `test_pistol`. An `ammo_modular` entity accepts
`cartridge_id` = `test_fmj`, `amount` = `25`.

The `.weapon` format is deliberately line-oriented `key = value`, plus repeatable
`accept = cartridge_id:muzzle_velocity_mps` entries. The cartridge version is
`version = 1`; fields for mass, diameter, reference velocity, and drag are in
physical units. Sample files list all required properties. Optional keys are
`weight`, `flags`, `fire_animation`, `draw_animation`, `reload_animation`,
`fire_sound`, `reload_sound`, `pickup_sound`, `sprite`, and `fire_event`.
The client and server must have matching manifests and definition files.

Runtime IDs **32–62** are allowed at this stage; ID 63 is `WEAPON_SUIT`.
Inventory slots **0–4** are allowed. Legacy ammo has 32 slots including index 0;
registration stops safely when slots run out. Weapon resource IDs are not yet
unlimited; expanded protocol/ID handling is scheduled for J.4.

## Important limitations: planned subsequent phases

- Cartridge mass, aerodynamic drag and weapon-specific muzzle velocity are
  **parsed and validated but not yet used in gameplay**. J.3 implements physical
  bullet flight, cartridge selection, unique reserve inventory and magazines.
- J.1 handles basic player firing; generic NPC equipment/attack wiring is J.4.
- Only basic legacy hitscan, a primary fire action and standard reload are live.
  The broader firing-mode and behavior-module system is J.2.
- The cartridge pickup temporarily maps to the first compatible weapon's legacy
  `ammo_name`. Do not interpret it as cartridge-specific ammo inventory.
- Multiplayer definitions are not yet synchronized or checked; J.4 will provide
  manifest checksums and mismatch protection.
- J.1 save/restore stores the generic weapon ID and context fields, but complex
  ammunition/ballistic state persistence is J.5. In-game restore needs testing.
- Precache model/sound paths must exist in the selected mod/game search paths.
- The standalone Weapon Editor is J.5, **not pxmv**.

## Checks

Portable test:

```sh
c++ -std=c++17 -Igame_shared game_shared/modular_weapons.cpp \\
  game_shared/tests/modular_weapons_test.cpp -o /tmp/modular_weapons_test
/tmp/modular_weapons_test documentation/examples/modular-weapon
```

Full PhysX-enabled gameplay, resource precache, prediction and network behavior
still require a complete build/dependency checkout and an actual Xash3D runtime.
