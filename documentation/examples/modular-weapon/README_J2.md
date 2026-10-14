# J.2 — Modular weapon actions (separate standalone editor remains planned for J.5)

Copy this example's `scripts/` subtree into the active mod's virtual file paths.
The supplied `test_pistol.weapon` illustrates a **version 2 weapon definition**
with two independently configured attacks. Cartridge files remain **version 1**.

The original v1 `.weapon` definitions continue loading unchanged. They retain
an automatic primary hitscan attack and no secondary attack. The v2 serializer
writes explicit action properties and version 2.

## Supported action properties

Prefix each action with `primary_` or `secondary_`:

- `behavior`: `hitscan`, `none`, or a custom behavior ID installed in BOTH client and server binaries before registry loading. Only `hitscan` is executable by default.
- `mode`: `auto`, `semi`, `burst`, `single`.
- `interval`: seconds between shots, 0.01–60.
- `spread`: legacy angular spread (0–1).
- `projectiles`: 1–64 legacy hitscan rays per shot.
- `ammo_cost`: 1–32 **clip rounds** per shot (both actions use the same clip).
- `burst_count`: 2–16 sequential trigger shots (used by `burst` mode).
- `recoil_pitch`, `recoil_yaw`: deterministic punch-angle offsets in degrees.
- `animation`: Studio viewmodel sequence index.
- `sound`: optional weapon sound asset path.
- `event`: optional scripted client event path for visual effects (and any shell ejection coded by that event).

`auto` repeats while holding the trigger; `semi` requires a release before
another shot; `burst` queues a configured number of timed shots, including
shots after the trigger is released; `single` fires once per press and requires
`clip_size = 1` in the v2 definition. An interrupted burst stops on reload,
holster, insufficient clip ammo, or absence of its behavior module.

Weapon-wide options:
`muzzle_flash`, `fire_underwater`, `auto_reload` (`true`/`false`),
`idle_animation` (-1 disables), `idle_seconds`, `animation_extension`.
The legacy `fire_interval`, `spread`, `fire_sound`, `fire_event`, and
`fire_animation` fields provide primary-action defaults so v1 content works.

## Important J.2 limits

- Fired projectiles **still use legacy PrimeXT/I.9.6 instantaneous hitscan**.
  Mass, drag, gravity, and cartridge-specific muzzle velocity become active in J.3.
- Both attacks draw from the weapon's current legacy clip. Cartridge-specific
  magazines/selection/inventory are J.3.
- The `event` property calls existing event playback; actual shell models and
  particle effects require an authored compatible client event handler.
- Animation indices must match the selected viewmodel, and sound/event files
  must be available in the mod. The standalone visual editor is J.5.
- Burst and trigger-latch values are synchronized in `weapon_data_t` for
  prediction; **mid-burst save/load** is not implemented here (J.5).
- No custom attack DLL is loaded from data files. A game-code developer may
  install a reusable handler via `RegisterModularAttackBehavior(id, callback)`
  before loading the registry on both client and server.
- Client/server definition checksum synchronization and actual network
  compatibility tests are deferred to J.4.
