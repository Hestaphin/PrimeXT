# J.10.6.9.2 — HUD selection and entity tools

This release implements steps 1 and 2 of the agreed repair plan, based on
J.10.6.9.1 reconstructed from the supplied original PrimeXT and Xash3D FWGS
source ZIPs. Sky rendering and the additional weapon/editor features remain
later steps.

## Changes

- Normal HUD confirmation and single-item fast switching send `use <weapon-id>`.
  Classic weapon names still work. Names are bounded and restricted to a single
  identifier token; the existing HEV suit and weapon availability rules remain.
- `ent_create` defers its missing-factory diagnostic until the legacy, extension
  and restore-list factories have all failed. Ordinary engine/map creation keeps
  its existing error reporting.
- `ent_getvars_cb` has a quiet default client command. A user alias takes
  precedence, and command-buffer privilege/filter behavior is unchanged.
- Creation success and the final `ent_last_*` callback are published after
  spawning, floor placement, the initial think and post-spawn keys. Rejected,
  removed and recycled edicts stop initialization. A recycled replacement is
  never freed by the failed creation attempt.
- Placement passes through the game's origin keyvalue handler, including after
  dropping to the floor, and final bounds are linked before feedback is sent.
  PrimeXT's origin parser copies the requested value before updating its cached
  absolute transform.

## Placement evidence

The engine originally wrote only `edict->v.origin`; PrimeXT also maintains
`CBaseEntity::m_vecOrigin`. `FallThink` can invalidate and recalculate the
absolute transform, restoring the stale local position. A second failure occurs
when `EntvarsKeyvalue` passes `pev->origin` by reference into `SetAbsOrigin`:
recalculating a dirty transform can overwrite that same value before the setter
uses it. Production-method regressions reproduce both mechanisms on J.10.6.9.1
and pass with this release.

This establishes a source-level placement defect. It does not certify the
reported Windows map's visibility or walking pickup; those require the native
acceptance below.

## Automated checks

From the PrimeXT source directory, with the matching Xash source in the sibling
`xash3d-fwgs-master` directory:

```sh
python3 game_shared/tests/verify_j10_6_9_2.py --ubsan
```

The targeted suite has 33 cases covering HUD selection, allocation fallbacks,
spawn rejection and entity lifetime, final feedback, callback aliases and
filtering, game transforms, floor placement and weapon materialization.
It extracts complete production functions and uses explicit adapters for engine
services and factory callbacks. Placement also links the real PrimeXT math code.
The suite passes 33/33 with undefined-behavior sanitization; the same cases
against J.10.6.9.1 pass 10/33. The failures are regression cases, not a count of
independent bugs.

Complete Linux syntax checks cover the changed PrimeXT client and server files.
The supplied Xash ZIP lacks its platform `build.h` and `library_suffix.h`
dependency headers. Method checks use the supplied PrimeXT `build.h` explicitly;
complete Xash translation-unit checks are separately reported as blocked when
`library_suffix.h` is absent. This does not replace a configured engine build.
Windows x86 linking, live rendering and collision/pickup acceptance remain
unverified here.

## Native Windows x86 acceptance

Rebuild/install the PrimeXT client and server DLLs and the matching Xash engine.
Use the same architecture as the working J.10.6.9.1 installation. Keep the
existing sample `test_pistol` definitions installed and restart the game after
changing them. Enter commands separately and wait for the map to load:

```text
map env_cubemap
sv_cheats 1
sv_enttools_enable 1
give item_suit
ent_create weapon_modular weapon_id test_pistol
ent_list weapon_modular
```

1. Successful fallback creation must produce no misleading `No spawn function`
   message and no `Unknown command: ent_getvars_cb`. Use the actual entity number
   from the final `Created` line with `ent_info <number>`; record its origin,
   model and bounds. Walk onto the weapon before trying `ent_fire <number> touch`
   as a separate control. Repeat with `ent_create weapon_9mmhandgun`.
2. Select the acquired pistol with slot 2 and confirmation under
   `hud_fastswitch 0`. With `hud_fastswitch 1`, test direct switching when it is
   the only active weapon in its bucket. Acquire a classic pistol as well and
   check slot cycling, wheel cycling, `use test_pistol` and secondary ammo cycling.
3. Define `alias ent_getvars_cb "echo callback_ok"`, create another entity and
   verify that the callback runs with final `ent_last_*` values. Repeat with an
   explicit `targetname`. The default command must not replace the alias.
4. `ent_create j10692_missing_class` must still report a genuine factory failure.
   A modular weapon with an unknown `weapon_id` must report its rejection and
   must not publish `Created` or call the success callback.

Record the engine/game artifact versions and the results of walking pickup and
manual touch separately. This patch changes no sky or editor feature code.
