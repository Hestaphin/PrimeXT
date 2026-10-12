# PrimeXT Milestone J.2 — Modular Weapon Runtime

Baseline: uploaded original PrimeXT and Xash3D ZIPs plus the J.1 combined patch.
This milestone implements the **second of six phases** (formerly steps J.8–J.14).
Xash3D source is unchanged.

## Source changes

1. Extend the shared `.weapon` loader/writer to version 2, retaining version-1
   weapon support and version-1 cartridge support.
2. Introduce reusable primary/secondary `Attack` definitions:
   `behavior`, `mode`, `interval`, `spread`, `projectiles`, `ammo_cost`,
   `burst_count`, `recoil_pitch/yaw`, `animation`, `sound`, and `event`.
3. Implement automatic, semi-auto, burst, and single-shot input semantics in
   `CModularWeaponContext`, including independent secondary fire, shared clip
   consumption, timing, deployment/holster/reload cancellation and idle config.
4. Integrate primary/secondary animations, recoil, sounds, muzzle flash and
   existing scripted weapon events. A compatible client event may provide
   shell ejection; J.2 has no built-in shell emitter.
5. Provide `RegisterModularAttackBehavior` / `RegisterBehaviorId` APIs for
   reusable native behavior handlers (game-built code, not arbitrary DLLs).
   Unregistered/unsupported behavior names fail validation or refuse to fire
   without expending ammo.
6. Serialize trigger latches and outstanding burst count into the existing
   weapon prediction data fields `iuser1` and `iuser2`, on both server and
   client. An `OnPreItemPostFrame` hook reads real input *before* the base
   weapon code clears secondary attack, avoiding held-trigger semiauto bugs.
7. Update the sample test pistol with a configurable secondary hitscan action.

## File formats

Both `.weapon` v1 and v2 load. Cartridge definitions stay v1; this was checked
and fixed because increasing the shared maximum version initially rejected
old cartridge files. For parameters and examples see
`documentation/examples/modular-weapon/README_J2.md`.

## Validation performed

- Dependency-free C++17 regression test: **passed** (v1/v2 roundtrip,
  registered behavior IDs, configurable burst count, and 21 atomic rejection
  checks).
- Syntax compilation of six server/shared and three client units: **passed**
  with the project's Linux defines and include paths.
- Incremental and cumulative patch reproductions: see patch validation output.

## Not yet verified / later milestones

- Full compile/link against the project's recursive PhysX dependencies and
  a running Xash3D server/client was not available in this environment.
- Real-time burst cadence, client reconciliation, replay and multiplayer
  prediction still require in-game evaluation; network manifest checks are J.4.
- Save/restore of partially completed bursts and complex cartridge state: J.5.
- Ballistic gravity, drag, muzzle velocity, .phys projectile contact,
  per-cartridge ammunition inventory, and cartridges/magazines: J.3.
- NPC weapon control, network protocol upgrades: J.4.
- Standalone visual Weapon & Cartridge Editor: J.5 (NOT pxmv).
- Full legacy-weapon conversion and integration testing: J.6.

## Patch application

- **Incremental**: Apply J.2 to extracted original ZIPs **after J.1**.
- **Cumulative**: Apply combined J.2 to freshly extracted original ZIPs.
- Use `patch -p1` at the directory containing `PrimeXT-master` and
  `xash3d-fwgs-master`; never apply both variants.
