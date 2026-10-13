# J.10.6.9.4 — step 5 runtime

Base: corrected J.10.6.9.3, commit `92ee63a953217f5513f55dce143a92b0005ddbef`.
This release activates the format-3 data added in steps 3–4. Editor UI work in
steps 6–9 is still pending. The inherited sky changes are retained.

## Damage

For a format-3 cartridge, each projectile inflicts:

`damage = damage_coefficient * current_kinetic_energy_J / (pi * diameter_mm^2 / 4)`.

Current energy is calculated from the collision-time projectile velocity and
mass, including drag and losses during penetration. There is no five-percent
floor for format 3. The same cartridge therefore has different muzzle damage
in guns with different `accept` velocities. A hitscan action, or a ballistic
action with `sv_ballistics_enabled 0`, uses that gun's muzzle energy. Fractional
damage is retained instead of being rounded to zero and invoking legacy damage.
Formats 1–2 retain their previous hitscan behavior and ballistic damage floor.
Format-3 hitscan actions multiply the action's projectile count by the cartridge
projectile count, with the same validated limit of 256 as ballistic actions.

## Asset selection

For each model, sound or sprite role, precedence is:

1. A nonempty override for the action's cartridge.
2. A matching `when = ammo:<ID>` variant.
3. A matching `when = mode:<mode>` variant.
4. A deterministic choice among `when = random` variants.
5. The role's `when = default` variant.
6. The existing flat property.

The loader continues to reject more than one explicit default per role.
Multiple matches at a non-random tier use the first variant in byte-sorted ID
order. At each tier, `primary_sound`/`secondary_sound` take priority over the
generic `fire_sound`; a more specific generic condition still precedes an
action-specific default. `hud_sprite` similarly precedes `sprite` at each tier.

Firing uses the loaded magazine's cartridge, never the next reload selection.
Reloading uses the selected cartridge; topping up with a different cartridge is
rejected. Cycling ammo with rounds still loaded retains the loaded cartridge's
assets. An empty gun can show the selected cartridge. Deploy/reload/ammo-change
conditions use the primary mode; a primary or secondary shot uses that action's
mode. Each shot in a burst is an action.

The accepted action pins its seed and action type. Frames, prediction replay,
video reload and save/restore do not resample its random choices. Failed actions
do not change the pin or play their firing/reload feedback. Deploying after a
switch starts a new selection. Models are applied before action animations;
authored models must support the weapon's specified sequence numbers.

HUD sprites support `.txt` role/rectangle lists or a standalone `.spr` first
frame. A custom list falls back to the baseline `sprites/<weapon-ID>.txt` for
omitted roles. A standalone selection icon retains the baseline crosshair.
The cartridge's `sprite` controls the ammo icon. All conditional/override assets
and sprites referenced by HUD lists are precached through the engine VFS.

Kill sprites are pinned to the projectile, including delayed impacts after the
owner switches weapons. The existing DeathMsg gains an optional three-byte
reference to the matching definition, rather than transmitting an asset path.
Unknown/unverified references fall back to the normal death notice icon. The
server owns firing/reload audio, as in the previous runtime; custom event scripts
should leave that audio to the runtime to avoid overlapping hard-coded sounds.

## Network and save compatibility

Content fingerprints and the old format-1/2 canonical bytes are unchanged.
A separate runtime fingerprint includes the J.10.6.9.4 semantics when any
format-3 definitions are loaded. A J.10.6.9.3 peer therefore cannot incorrectly
verify a format-3 J.10.6.9.4 game. Deploy both rebuilt game DLLs together.

The weapon delta now sends `iuser4` for selected ammo and six-bit weapon IDs.
The full seed and action tag use exact integer fragments within the existing
`fuser1`–`fuser3` float quantizers. No engine/public struct layout is enlarged.

In-flight save records advance to version 3. Their version-2 byte stride is
unchanged; coefficients and shot attribution are separate save fields.
Version-2 in-flight shots restore with their original damage behavior.
Version-3 shots retain their coefficients and icons; invalid coefficients,
unregistered icons and incompatible record versions are rejected. Data-pack
fingerprints still guard save restoration.

## Checks and remaining acceptance

Run from `PrimeXT-master`:

```sh
python3 game_shared/tests/verify_j10_6_9_4.py --ubsan
```

The supplied suite checks real shared loaders, both configurations of the real
weapon context, NPC actions, real HUD loading, manager impact/save/restore
methods, existing 256-projectile/reentrant behavior, network field integration,
and 15 complete client/server translation units. Engine services in method
tests are adapted. It also runs inherited J.10.6.9.3 and earlier regressions.

These checks do not certify full Windows/client/server/PhysX links or live
network/save/HUD/map rendering. The original Xash source still lacks
`3rdparty/library_suffix/library_suffix.h`, blocking the three inherited engine
translation-unit probes. Live map changes, reflections and cubemaps retain the
separate sky acceptance described in J.10.6.9.3.
