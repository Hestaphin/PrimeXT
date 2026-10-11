# PrimeXT Milestone J.4 — Player, NPC, Multiplayer and Protocol Integration

J.4 is built on cumulative J.3. The standalone Weapon & Cartridge Editor remains **J.5**, a separate executable (NOT pxmv).

## Load and enable

Install matched `.weapon` and `.cartridge` files under the mod's `scripts/` directories on both server and client. Server loads the manifests at world startup. J.4 sends a canonical FNV-1a registry digest (`ModCfg` user message) and authoritative registered cartridge ID → ammo slot mapping when the player HUD initializes. A client with different assets disables its modular fire prediction and logs a warning. Damage remains server authoritative.

This protocol uses **existing** Xash3D user messages and weapon-data fields; no Xash3D engine source changes are needed for the current ID range. Xash/PrimeXT's 6-bit weapon ID and 32 ammo slots remain hard limits. A new wide-ID network protocol and complete server-side connection rejection are **not** implemented by J.4.

## NPC weapon assignment (human grunt integration)

Set the following map entity keys on `monster_human_grunt`:

```
modular_weapon = test_pistol
modular_cartridge = test_fmj
```

In the weapon definition, add:

```
npc_usable = 1
```

`modular_cartridge` may be omitted, in which case `default_cartridge` is used. The server validates the weapon's NPC opt-in, primary behavior (`ballistic` or `hitscan`), and cartridge compatibility at spawn. Without a valid configuration the grunt continues using its legacy gun. With a valid config, grunt animation fire events use the same cartridge/velocity/projectile simulation as a player's gun, retain the grunt's existing AI aiming/reload timing, and drop the corresponding `weapon_modular` pickup. Models/animations are still the grunt's existing character animation set, not automatically swapped to the configured weapon. Other NPC types/turrets require a call to the common `FireModularNpcWeapon` adapter in their firing code.

The NPC can use the weapon's configured primary action. Player-specific trigger latches, secondary actions, cartridge switching, animation event hooks and per-cartridge NPC reserve inventory are not yet generalized to every monster's AI.

## Player inventory & selection

Player and weapon-box duplicate checks now distinguish modular weapon runtime IDs even though all weapons share `weapon_modular` classname. A user can select the gun with its actual weapon ID (`test_pistol`) rather than relying on the generic classname. Ammo types and selected/loaded cartridge indices use the preexisting prediction fields. Only the active weapon's primary/secondary reserve counts are synchronized in `clientdata_t`; remaining reserves continue to use legacy HUD `AmmoX` messages. Both indices are range checked. Reloads still use a one-cartridge-type clip; mixed magazines/chambers are not implemented.

## Ballistic visuals

`sv_ballistics_tracers 1` enables server-originated `TE_TRACER` segments to nearby clients, throttled to at most one each 50 ms per projectile. The server continues to perform all collision and damage calculations. The default is `0` (visual tracers disabled) to avoid saturating the network. This is an initial observer visualization, **not** interpolated client-side physical-projectile replication. It uses the engine's standard temporary-entity effect protocol.

```
sv_ballistics_enabled 1
sv_ballistics_tracers 1
sv_ballistics_gravity_scale 1
sv_ballistics_air_density 1.225
```

## Remaining limits and acceptance tests

- Full PhysX-enabled compile/link and client-server multiplayer integration have not been verified by portable checks.
- Test custom gun pickup, holster, selection, duplicate pickup and weapon-box packing in-game.
- Test matched/mismatched client definition manifests and interrupted joins/reconnects; the digest is compatibility detection, not cryptographic authentication or a complete anti-cheat handshake.
- Test projectile trajectory visual continuity under high ping. The tracer is not a synchronized client-side bullet simulation.
- Test grunt custom gun firing, friendly-fire AI protection, ammo reload, death drop and map restore.
- Test I.9.6 `_cp` and `.phys` collisions with physical bullets and ragdoll impulses.
- Wider than 6-bit weapon IDs / 32 ammo slots, complete turret/NPC generic loadouts, mixed magazines, projectile persistence and robust reconnect/lag compensation remain follow-up work (J.5/J.6 or protocol extension).

## Portable tests

```
c++ -std=c++17 -Igame_shared game_shared/modular_weapons.cpp game_shared/tests/modular_j4_test.cpp -o /tmp/modular_j4_test
/tmp/modular_j4_test /path/to/J4/sample/assets
```
