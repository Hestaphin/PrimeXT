# J.10.6.9.4 runtime sample

Use this as a standalone sample pack with the matching rebuilt client/server.
Copy its `scripts/` and `sprites/` folders into the mod's game directory. This
replaces the demo manifests; merge manifests deliberately for an existing mod.
Do not install this together with the other `test_pistol` packs: they share
definition names and runtime ID 32. Keep normal Half-Life models/sounds installed.

Create a map entity `weapon_modular` with `weapon_id = test_pistol`, and
`ammo_modular` pickups with `cartridge_id = test_fmj` or `test_jhp`, `amount = 25`.
Walk over the weapon and ammo. Select using `use test_pistol` or the HUD.
The secondary action cycles next reload ammo. Empty the current magazine before
reloading a different cartridge.

The included original IDSP version-2 sprites distinguish FMJ and JHP ammo and
death notices. The HUD list uses the sample's selection icons; a normal
`sprites/test_pistol.txt` can supply crosshairs/other omitted HUD roles. The gun
uses the standard 9mm view/player/world models. Model condition rows deliberately
retain those same paths so sequence numbers stay valid. Replace them with your
own models supporting the draw/fire/reload/idle sequence contract to inspect a
visible model change. No model or audio binaries from Half-Life are redistributed.

FMJ randomly chooses one of two existing firing sounds for each accepted shot.
JHP overrides the firing/reload sound and selection/kill icons. Its override
applies when that ammo is actually loaded; choosing JHP while FMJ remains in the
magazine does not change a FMJ shot. Random selection stays fixed during each
action, including prediction replay. NPC use is enabled for the existing modular
NPC adapter.

Useful acceptance checks:

- Compare impacts close to the muzzle and at range with ballistics enabled.
  Format-3 damage follows the remaining energy without a five-percent floor.
- Set `sv_ballistics_enabled 0`: damage uses the accepted cartridge velocity
  rather than the legacy 9mm damage, and still retains fractional values.
- Reload JHP, fire, cycle to FMJ while rounds remain, and fire again. Both shots
  still use JHP. Empty/reload to change the loaded type.
- Switch away and back; save/reload during a reload and while a shot is in
  flight. Confirm the active action assets and delayed kill icon remain stable.
- Reconnect with matching packs/DLLs; a format-3 J.10.6.9.3 client must fail
  runtime verification rather than predicting the new game with old semantics.

See `documentation/J10_6_9_4_RUNTIME.md` for precedence and compatibility details.
