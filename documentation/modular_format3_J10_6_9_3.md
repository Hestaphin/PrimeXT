# Shared modular format 3 — J.10.6.9.3

This release implements step 4 of the agreed plan: the shared data format,
validation, canonical serialization and client/server fingerprints. It also
implements step 3's sky repairs. Its base is J.10.6.9.2.

Conditional asset selection and coefficient-based impact damage in the game
are step 5. The new editor controls, textured playback, curves and category
operations are steps 6–9. In this release, the game still uses its existing
flat model/sound fields and damage code. Format-3 data can be validated and
saved without losing the new metadata; it should be used as authoring data
until step 5 activates it in gameplay.

## Compatibility

| Definition | Accepted versions | Behavior of earlier definitions |
| --- | --- | --- |
| Cartridge | 1, 2, 3 | Version 1 keeps its historical implicit damage; version 2 keeps `damage`. |
| Weapon | 1, 2, 3 | Existing fields/actions remain valid. The canonical writer retains the earlier version-2 normalization for versions 1–2. |

Adding this support does not silently migrate old files. Earlier definitions
keep their canonical bytes and fingerprints. The shipped legacy example
fingerprint remains `1605249880`. Each client and server must be rebuilt with
the same patch; new fields participate in the existing fingerprint handshake.

All files use `key = value`, with `#`/`;` comments. Duplicate keys are errors;
`accept` remains the sole repeatable key. Existing manifests and file names
continue to use stable IDs. Categories do not relocate files or change IDs.

## Cartridge additions

Version 3 requires `damage_coefficient` instead of `damage`. It retains the
SI projectile properties and required `projectile_count` from older versions.

```ini
version = 3
id = fmj
chamber = 9x19
projectile = fmj
mass_grams = 8
diameter_mm = 9
drag_coefficient = 0.3
reference_velocity_mps = 360
projectile_count = 1
damage_coefficient = 2
category = 9x19/fmj
sprite = sprites/640hud7.spr
```

The coefficient scales energy density in joules per square millimetre:

`energy_J = 0.5 * (mass_grams / 1000) * velocity_mps^2`

`area_mm2 = pi * diameter_mm^2 / 4`

`damage = damage_coefficient * energy_J / area_mm2`

The loader verifies finite positive coefficients, at most `1000000`, and
derived muzzle damage in `(0, 1000]` at the reference speed and every weapon
`accept` speed. `damage` and `damage_coefficient` cannot coexist. This formula
currently validates authoring data; current-energy impact integration is step 5.

`category` and `sprite` are optional version-3 fields. Sprite paths are
mod-relative `.spr` or sprite-description `.txt` paths. The loader validates
path shape and resource type, not whether a file is installed.

## Conditional weapon assets

Keep the existing required flat model fields as fallbacks. Each named entry
has one `role`, one `path`, and an optional `when` (defaults to `default`):

```ini
version = 3
# ...existing required weapon properties and accept lines...
category = rifles/sg550/camo
asset.standard.role = view_model
asset.standard.path = models/v_sg550.mdl
asset.standard.when = default
asset.automatic.role = fire_sound
asset.automatic.path = weapons/sg550_auto.wav
asset.automatic.when = mode:auto
asset.choice_a.role = fire_sound
asset.choice_a.path = weapons/sg550_a.wav
asset.choice_a.when = random
asset.fmj_view.role = view_model
asset.fmj_view.path = models/v_sg550_fmj.mdl
asset.fmj_view.when = ammo:fmj
```

| Asset role | Resource |
| --- | --- |
| `view_model`, `player_model`, `world_model` | `.mdl` |
| `fire_sound`, `reload_sound`, `pickup_sound`, `primary_sound`, `secondary_sound` | `.wav` |
| `sprite`, `hud_sprite` | `.spr` or `.txt` |
| `kill_sprite` | `.spr` |

| Condition | Valid value |
| --- | --- |
| Default | `default`, or omitted `when` |
| Random candidate | `random` |
| Firing mode | `mode:auto`, `mode:semi`, `mode:burst`, `mode:single` |
| Allowed ammunition | `ammo:<cartridge-ID>`; the cartridge must appear in this weapon's `accept` list |

There can be at most one explicit default per role and 128 entries per weapon.
IDs are unique without regard to ASCII case; entry IDs use letters, numbers,
underscores or hyphens, at most 63 characters. Unknown roles, properties or
conditions are errors. Action-stable random selection, condition precedence
and replacement of flat fallbacks will be implemented together in step 5.

## Ammunition overrides

Use a cartridge ID from the weapon's `accept` list:

```ini
ammo.fmj.fire_sound = weapons/fmj_fire.wav
ammo.fmj.reload_sound = weapons/fmj_reload.wav
ammo.fmj.view_model = models/v_sg550_fmj.mdl
ammo.fmj.hud_sprite = sprites/weapon_sg550.txt
```

Supported properties are `view_model`, `player_model`, `world_model`,
`fire_sound`, `reload_sound`, `sprite`, `hud_sprite`, `kill_sprite`.
An omitted override preserves fallback data. Validation rejects unavailable
cartridge references and incorrect resource types. Gameplay fallback and
selection will be applied in step 5.

## Nested category metadata

Both version-3 weapon and cartridge files may specify `category`. A category
is a slash-separated path of stable identifier segments, up to 16 levels,
63 characters per segment and 255 characters total. Empty segments, traversal,
spaces and leading/trailing slashes are rejected. Omit the field for an
uncategorized item. Examples: `rifles/sg550/camo`, `556/fmj`.

This field is metadata. Right-click move/duplicate/delete and the editable
category tree are step 9; references and flat manifests stay intact here.

## Canonical serialization and verification

The shared writer emits variants sorted by entry ID and overrides sorted by
cartridge ID and property, with classic-locale numbers. All new semantic
fields are included in `Registry::Fingerprint()`. Native editor validation
uses this same registry and preserves dotted fields through save/reload and
undo/redo. Invalid exports remain atomic.

Run `python3 game_shared/tests/verify_j10_6_9_3.py --ubsan` from PrimeXT to
exercise the shared format, real editor filesystem operations, client/server
fingerprints, sky method adapters, complete syntax checks and inherited tests.
Standalone CMake builds may enable `PXWE_BUILD_TESTS=ON` and run `ctest` for
the native backend and format cases. Windows linking and live GPU acceptance
are separate from these checks.
