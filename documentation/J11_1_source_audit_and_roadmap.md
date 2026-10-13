# J.11.1 — Source audit, v4 schema, and five-milestone roadmap

Source baseline: the original PrimeXT and Xash3D-FWGS ZIPs with cumulative J.10.6.9.9 applied. This document describes **source code**, not a successful Windows or game runtime test.

## New terminology and milestones

| Milestone | Stages | Scope |
|---|---|---|
| **J.11.1** | 1–2 | Audit and new v4 cartridge/weapon schema, strict parser, deterministic serializer, optional overrides and tests |
| **J.11.2** | 3–4 | Damage calculations and actual ammo-driven hitscan/ballistic shot dispatch (player/NPC), with correct collision/effects |
| **J.11.3** | 5–6 | Effective RPM/spread/sound resolver in runtime; editor Properties and action UI |
| **J.11.4** | 7–8 | Weapon Cartridges tab override controls and camera/pxmv-style preview |
| **J.11.5** | 9–10 | Transactional IDs/category rename, detailed beginner help and full system regression |

User-facing `fire_rate` and `default_fire_rate` values are **RPM**, not seconds; `60 / rpm` converts to a cooldown. In Version 4 the old `fire_interval`-style authored fields are forbidden. Existing v1–3 parsing is temporarily retained, not the final design or a requirement to preserve indefinitely.

## J.10.6.9.9 audit — concrete ownership and conflicts

| Source | Existing J.10.6.9.9 behavior | Required later change |
|---|---|---|
| `game_shared/modular_weapons.h/.cpp` | `Weapon::fireInterval`, `Weapon::spread`, `Weapon::fireSound`, weapon-wide `bulletType`; `Attack::interval` / `spread` / `sound`; v1–3 cartridge damage based on version | v4 separate cartridge `attackLogic`/`damageLogic`, weapon defaults in RPM, independently optional ammo overrides; future remove historical execution aliases |
| `game_shared/cartridge_damage.h` | `schemaVersion >= 3` selects energy-density calculation; v1–2 uses nominal fixed damage and energy scaling | J.11.2 implement explicit Damage Logic dispatch, authoritative authored Hitscan damage and configurable ballistic damage |
| `game_shared/weapons/modular.cpp` | `FireAction` chooses hitscan/ballistic from `action.behavior`; uses action spread/interval and weapon bullet type; conditional fire sounds | J.11.2 dispatch based on **loaded** cartridge; J.11.3 use effective RPM, spread, sound and preserve ammo-cycle semantics |
| `server/server_weapon_layer_impl.cpp` | Hitscan accepts explicit damage argument, uses `bulletType` for impact sound/decal and fallback skill damage | J.11.2 pass explicit `default_damage` for v4 Hitscan; retain original non-modular weapon skill values and impact behavior |
| `server/modular_ballistics.cpp` | `MuzzleDamage`/`ImpactDamage` and `damageCoefficient`; uses weapon-wide bullet type | J.11.2 switch damage math to Damage Logic; independent impact classification for ballistic shots |
| `server/modular_npc.cpp` | NPC firing dispatch uses weapon behavior and bullet type | J.11.2/3 use loaded cartridge and per-weapon overrides |
| `game_shared/modular_asset_selection.h` / `modular_asset_resources.h` | Variant assets, ammo-specific sounds/models, precaching | J.11.3 ensure explicit `ammo.<id>.fire_sound` override → `default_fire_sound` precedence, precache all used assets |
| `tools/weapon_editor/pxwe_project.cpp` | Definitions have v1–3 keys; category trees, safe save, undo; damage graphs assume schemaVersion | J.11.3/4 present explicit v4 fields and new models without stale schema-version damage rules |
| `tools/weapon_editor/pxwe_properties.cpp` | Property help and value choices include old firing interval in seconds, weapon bullet type and current sound aliases | J.11.3 detailed RPM labels, cartridge Attack/Damage Logic and disabled incompatible fields |
| `tools/weapon_editor/pxwe_win32.cpp`, `pxwe_preview.cpp` | Native property panes and Studio preview; camera controls more limited than pxmv | J.11.4 enhance viewer and confirm no blank panel regression |
| `utils/pxmv/` | Arcball/first-person/model navigation references | J.11.4 reuse tested ideas without changing exported model scale/collision |

## J.11.1 shared schema implementation

- Version-4 `.cartridge` requires explicit `attack_logic` + `damage_logic`; only `hitscan + legacy`, `ballistic + configurable_base` and `ballistic + energy_based` are legal. Inactive damage keys are rejected, as are nonfinite/zero/negative values where invalid.
- Version-4 `.weapon` requires `default_fire_rate` (1..6000 RPM) and `default_spread` (0..1), optionally `default_fire_sound`. It cannot author old `fire_interval`, weapon-wide `bullet_type`, or secondary per-action interval/spread/sound aliases.
- Weapon ammo overrides support `ammo.<id>.fire_rate` (1..6000 RPM), `ammo.<id>.spread` (0..1), and existing `ammo.<id>.fire_sound`; absent and explicitly zero spread remain distinguishable.
- The v4 `fire_loaded_cartridge` is registered as a valid action ID **for parsing**, but no execution callback is installed yet. v4 weapon definitions reject old `hitscan` and `ballistic` as button behaviors.
- Version-4 weapons can accept only v4 cartridges, and vice versa. The parser verifies energy-based damage at the compatible muzzle velocity, and shared writers preserve authored definitions and deterministic fingerprints.
- **Fail-closed runtime**: J.11.1 `ValidateForRuntime()` rejects v4 definitions with a J.11.2-required diagnostic. This is intentional; J.11.1 is not playable with new v4 definitions, and must not accidentally execute the old algorithm on them.

## Stage boundary and next work

In J.11.1 the editor still shows several old v1–3 labels and does not yet offer new v4 property widgets. Runtime code still uses old timing, spread, sound and damage. **Only the shared v4 parser/serializer/type/validation foundation is implemented**, and old original Half-Life weapons are unchanged. Do not claim completed v4 shooting, UI, rename, model viewing or help until later milestones.

Run `g++ -std=c++17 -Igame_shared game_shared/modular_weapons.cpp game_shared/tests/modular_j11_1_schema_tests.cpp -o j11_1_test && ./j11_1_test` plus the existing regression suite before continuing.
