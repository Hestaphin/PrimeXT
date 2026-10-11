# J.10.3 — Modular weapon, cartridge and NPC ammunition repairs

Base: J.10.2 (PrimeXT I.10.10 physics + J.10.2 ballistic/PhysX source). Xash source and ballistic manager unchanged.

## Corrections

- `weapon_modular` overrides the legacy first-pickup and duplicate-weapon paths. Newly placed guns load their default cartridge into an empty magazine once, then place any remainder in the correctly named reserve. Used/drop guns keep their serialized **loaded cartridge index** and clip count; no default-ammo rewrite occurs.
- Duplicate modular guns transfer world-spawn default cartridges or typed dropped-magazine rounds into the matching player's reserve. The recipient's magazine is **never silently refilled or relabeled**. Transfers respect reserve capacity; partial pickups retain their remaining rounds on the world entity. Actual accepted counts, not the `GiveAmmo` return slot, determine whether rounds were transferred.
- `BindDefinition(definition, false)` rebinds restored weapons without recreating `default_ammo`, cartridge indices, or burst state. First-world-spawn binding still initializes its authored default ammo.
- `IsUseable` and `CanDeploy` look up the actual selected and loaded cartridge reserve slots and, when an `ammo_cycle` action is configured, every compatible cartridge. An empty weapon with JHP selected and JHP reserve may still deploy/reload even when default FMJ reserve is zero.
- Human grunts now check `primary.ammoCost` *before firing* and charge that cost on successful NPC shots. Both legacy `Shoot` and `Shotgun` animation event adapters use the shared cost; failed/insufficient-ammo shots do not fire.
- NPC weapon drops now send their actual cartridge and bounded remaining magazine rounds as keys to `weapon_modular`, rather than creating a fresh default-ammo gun. Standalone `ammo_modular` pickups likewise require a positive *actual* ammo grant.

## Boundaries

- J.10.3 deliberately preserves **homogeneous magazines**; mixed-cartridge magazine serialization is a later feature.
- The configured default cartridge is still a legacy `pszAmmo1` metadata field for compatibility. Modular inventory transfer does not treat it as the loaded magazine's cartridge.
- For NPCs, only the existing human-grunt adapter is generalized as to ammo cost; broader NPC/turret AI remains scheduled for J.10.5.
- This milestone does not repair the J.10.4 client physics-pose/registry-lifetime issues, death-ragdoll integration, PhysX SDK build gaps, or unfinished projectile visual networking.
- Player weapon drop reserve-ammo selection still follows the legacy `DropPlayerItem` policy; this milestone preserves loaded magazine identity and duplicate transfer, but a future full modular drop-box policy should allow separately packing all selected/compatible reserves.
- The legacy weapon respawn factory constructs an entity from the shared classname; assigning a distinct `weapon_id` to a newly respawned modular entity is not repaired in J.10.3.

## Validation

- `game_shared/tests/modular_j10_3_tests.cpp`: initial magazine fill, partial/full reserve acceptance, selected-compatible reserve eligibility, NPC action cost.
- `game_shared/tests/modular_j10_3_source_tests.py`: verifies the integration points use typed transfers, restore-mode rebind, loaded and selected ammo slots and actual grunt action costs.
- Compile changed game `.cpp` files as server and client (where applicable) with the project Linux build defines and PhysX disabled. Prior ballistic/physics regressions should stay passing.
- In-game acceptance still required: JHP/FMJ duplicate/drop while ammo full/partially full, restore an owned and dropped weapon with zero default ammo, deploy/reload a zero-FMJ/positive-JHP weapon, save/reload after NPC drops, and grunt attacks with `ammoCost` 3 and 1/2/3+ rounds. Test single-player and networked sessions.
