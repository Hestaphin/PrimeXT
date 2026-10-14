# PrimeXT Weapon Editor — Beginner Guide and Property Reference

**J.11.6 — Version 4 workflow and editor repair**
This manual assumes you are new to game modding. You do not have to understand C++ or physics to start. The examples are **illustrative game-balancing examples**, not promises about real-world ammunition performance. In the editor, select a property to see longer, contextual help; use **Tools → Copy selected property help** to share it, or **Tools → Open Beginner Guide** to view this document. Search this Markdown document for a property name (Ctrl+F in most viewers).

> **Important:** Save/Export changes on disk; your running game may need to restart/reload. A property value accepted by the editor does not guarantee that a particular weapon model, sound or game build is installed. Validate with **F5** before saving and test inside the game afterwards.

## 1. Your first working custom weapon, step by step

1. Open the game/mod folder containing `scripts/weapons/` and `scripts/cartridges/` (**File → Open game folder**). Work on a backup, not your only copy of the mod.
2. Create a **Cartridge**. The cartridge describes what travels out of the muzzle, its travel method, and how its damage is computed.
3. Set **Attack Logic = hitscan** for an instantly traced, straight shot, or **ballistic** for a projectile simulated over time. Hitscan does not mean automatic firing.
4. Set a compatible **Damage Logic**: hitscan uses `legacy`; ballistic uses `configurable_base` or `energy_based`. Fields belonging to the other modes are disabled on purpose.
5. Set cartridge **chamber** and, if ballistic, sensible projectile mass, diameter, drag, and reference velocity. A chamber is a compatibility label shared with the gun, not a physical measurement the engine derives by itself.
6. Create a **Weapon**, choose its `display_name`, `runtime_id`, clip/ammo capacity, and model paths.
7. On the **Cartridges** tab, **Allow** the cartridge, choose its muzzle velocity, then **Set Default**. A compatible weapon can allow more than one cartridge.
8. Set the weapon's **Default Fire Rate (RPM)**, **Default Spread** and **Default Fire Sound**. They apply whenever a cartridge has no override. Preview a WAV in the cartridge override editor when possible.
9. Set **Primary Behavior = fire_loaded_cartridge** if the primary button should fire; use **Secondary Behavior = ammo_cycle** to switch allowed ammunition. Behavior describes the button action; it does not choose hitscan versus ballistic.
10. In the Cartridges tab, optionally override **Fire Rate, Spread, and Fire Sound** per allowed cartridge. Blank means *inherit*. A typed **0** for Spread means deliberately use zero spread, not inherit.
11. In **Model / Animations**, load the `v_*.mdl` view model and inspect its sequences. The first-person preview is approximate. Preview Scale only changes the editor view, not the gun's real size.
12. Press **F5 Validate**. Fix errors, press **Ctrl+S Save**, restart/reload the game if necessary, and test actual firing, impact sounds, multiplayer prediction and reloading. Editor previews do not replace gameplay tests.

## 2. Three separate decisions: behavior, attack logic and damage logic

| Question | Setting | Stored on | Example |
|---|---|---|---|
| What happens when the player presses primary or secondary attack? | Primary / Secondary Behavior | Weapon | Fire loaded ammo, or cycle the cartridge |
| How does a fired projectile reach the target? | Attack Logic | Cartridge | Hitscan or ballistic |
| How is impact damage calculated? | Damage Logic | Cartridge | Legacy, configurable-base, energy-based |

**Hitscan** fires an invisible straight trace immediately. There is no modeled travel arc; it follows the classic bullet-tracing path. **Ballistic** creates a projectile with position and velocity, simulated over time. Gravity and drag can change its flight and energy. This is independent of a weapon's semi-automatic, automatic, burst or single mode. A shotgun can fire multiple projectiles in one attack event; the attack's projectiles-per-event and cartridge projectile count interact.

### Legacy Hitscan (`attack_logic = hitscan`, `damage_logic = legacy`)

Only this combination enables `default_damage` and `legacy_bullet_type`. **Default Damage** is the configured HP damage **per impacting projectile before target-specific rules**. If you set 60, its base damage is 60, not a lookup from the MP5 skill table. Headshots, armor and other target damage handling can alter the HP lost. Legacy bullet type **1 pistol, 2 MP5/SMG, 3 .357** provides legacy impact classification; it does not magically set caliber, projectile dimensions or the chosen HP damage. This path does not use flight kinetic energy to reduce that 60 with distance. Its penetration and effects follow the supported legacy trace behavior, not a fabricated ballistic energy model.

### Configurable Base Ballistic (`attack_logic = ballistic`, `damage_logic = configurable_base`)

`base_damage` is damage at the start of flight. The projectile's effective damage is multiplied by a fraction of impact energy relative to starting kinetic energy. Under the present valid-hit formula, the fraction is clamped to a minimum of **5%** and maximum of **100%** for a real positive-energy impact. This is not permission to damage a target if no collision occurs.

| Example: `base_damage = 40` | Base impact damage, before target modifiers |
|---|---:|
| 100% energy retained | 40 |
| 75% retained | 30 |
| 50% retained | 20 |
| 25% retained | 10 |
| 5% or less retained, valid impact | 2 |

The energy ratio can depend on air resistance, initial muzzle speed, penetration and the actual velocity when impact is detected. The cartridge's `reference_velocity_mps` is useful for preview and defaults; the weapon's allowed-cartridge velocity is used for the shot.

### Energy-Based Ballistic (`attack_logic = ballistic`, `damage_logic = energy_based`)

Kinetic energy is movement energy: `energy_joules = 0.5 × mass_kg × velocity_mps²`. Divide energy by the circular cross-sectional area of the projectile in square millimetres, then multiply by the authored `damage_coefficient`. Formula: `damage = damage_coefficient × energy_joules / (π × (diameter_mm / 2)²)`. A **joule** is a unit of energy, **grams** are thousandths of a kilogram, and a **coefficient** is a game-tuning multiplier. If projectile speed doubles, its kinetic energy becomes roughly **four times as large** at the same mass; that does not predict real-world medical injury. Tune the coefficient for your game and validate all values; extreme values may be rejected by runtime rules.

**When to choose each:** Legacy for familiar instant gunfire; Configurable Base when you want to author starting damage and let the bullet lose effectiveness during flight; Energy-Based when you want mass, velocity, diameter, and coefficient to determine base impact damage together.

## 3. Fire Rate is RPM, not seconds

Version 4 weapon definitions use `default_fire_rate` and optional `ammo.<cartridge_id>.fire_rate` overrides. Their unit is **firing events per minute** (RPM). Internally, `seconds_between_events = 60 / RPM`.

| RPM | Time between events |
|---:|---:|
| 300 | 0.2 s |
| 600 | 0.1 s |
| 900 | about 0.0667 s |
| 1,200 | 0.05 s |

**Example:** With default 600 RPM, `ammo.training.fire_rate = 300` and `ammo.tracer.fire_rate = 900`, one gun fires at three different rates depending on loaded ammo. If no cartridge override is authored, the weapon default wins. One event can produce multiple projectiles; don't confuse fire events with pellet count. The older seconds-based `fire_interval` belongs to older file formats, not Version 4 authoring.

## 4. Spread and recoil are different

**Default Spread** describes random shot-direction variation. It is a unitless engine spread setting, **not a number of degrees**. Zero requests the minimum variation. **Recoil Pitch/Yaw** adjust the viewpoint/aim reaction around each action and are separate. A cartridge can override spread, but a blank override means inherit. A custom `0` must survive save/reload and override a nonzero default.

## 5. Sounds: default, override, and impact

**Default Fire Sound** is stored on the weapon as `default_fire_sound`. A cartridge-specific `ammo.<id>.fire_sound` overrides it; an empty override inherits. Enter paths relative to `sound/`, for example `weapons/example.wav` (not `sound/weapons/example.wav`). Use WAV preview where available. Reload and pickup sounds are separately authored. Bullet **impact** audio (metal, dirt, etc.) is different from the gun's **firing** audio; Version 4 ballistic impact handling must still work even though Legacy Bullet Type is hitscan-only.

## 6. Ammo compatibility and muzzle velocity

**Cartridge Chamber** is the cartridge's logical chamber ID. Weapons should accept compatible chamber identifiers. **Reference Velocity (m/s)** gives a cartridge-speed reference for preview and initial compatibility. In the weapon's **Cartridges** tab, muzzle velocity is the actual per-weapon/cartridge launch-speed value. A faster gun can therefore shoot the same ammo at a different velocity. For ballistic ammunition, this changes kinetic energy and projectile flight. Hitscan ammo has no travel-time arc, and Legacy damage does not derive from kinetic energy.

## 7. Cartridge physical properties and projectile counts

- **Projectile / profile:** gameplay projectile-type ID used to select compatible impact/penetration profile behavior; not itself a damage formula.
- **Mass (grams):** weight of ONE projectile. `8 grams = 0.008 kilograms`. For a ballistic projectile, kinetic energy grows with mass for fixed speed.
- **Diameter (mm):** width of ONE bullet; affects circular area and may affect drag/damage calculations. Do not use centimeters or inches here.
- **Drag Coefficient:** controls air-resistance loss in simulated flight. `0` disables air drag in this model. Higher drag normally slows the projectile faster.
- **Projectile Count:** number of projectiles released by a cartridge. This interacts with the weapon action's `primary_projectiles`/`secondary_projectiles`, within the engine's per-shot total limit. A shot producing five pellets is not automatically five hits.
- **Default Damage, Base Damage, Damage Coefficient:** mutually exclusive authored values depending on Attack/Damage Logic. Disabled choices remain visible for explanation.

## 8. Weapon inventory and HUD properties

- **Weapon ID (`id`):** internal asset key and `.weapon` basename, used by the manifest. Read-only in the property table; use **F2 / Rename ID** to change it safely.
- **Display Name (`display_name`):** human-readable name shown to players. Changing this does **not** rename a file or definition ID.
- **Runtime ID (`runtime_id`):** engine-assigned numeric weapon slot (modular range 32–62). This is **not** the weapon script ID. Renaming a weapon leaves its runtime ID intact. Editing this number could affect multiplayer and saves.
- **Ammo Name (`ammo_name`):** ammunition inventory pool identity. Do not assume it always matches `default_cartridge`; the two fields can have distinct purposes.
- **Default Cartridge:** cartridge that is initially selected/loaded, and must be in the weapon's accepted list.
- **Clip Size:** how many ammunition rounds fit in the magazine; **Default Ammo** is pickup/starting quantity, **Max Carry** is reserve capacity.
- **Slot / Position:** where the weapon appears in the player's weapon HUD list.
- **Weight:** automatic selection priority; **Flags** are bit options governing empty pickup/reload/switch behavior.
- **NPC Usable:** allows modular-NPC use; AI schedules still control how often some NPCs choose to shoot.

## 9. Primary and Secondary behavior

**Primary Behavior / Secondary Behavior:** choose `fire_loaded_cartridge`, `ammo_cycle` or `none` for Version 4 where supported. **Mode:** `semi` once per press, `auto` repeats while held, `burst` fires a series, `single` is for restricted single-shot configurations. **Projectiles** controls the number of shots per event and interacts with cartridge projectile_count. **Ammo Cost** describes rounds spent per attack. **Burst Count** controls burst size. **Recoil Pitch/Yaw** describe the player's view movement; negative/positive directions are engine-specific. **Animation** is a numeric Studio model sequence index. **Sound/Event** select applicable optional assets. On Version 4 firing actions, use the cartridge → default RPM/spread/sound resolution, not stale per-action seconds-based overrides. Separate non-fire behaviors may have different timing needs.

## 10. Model / Animations viewer

**View Model (`v_*.mdl`)** appears in your first-person hands. **Player Model (`p_*.mdl`)** is held by other characters. **World Model (`w_*.mdl`)** represents pickups/dropped weapons. They can use unrelated filenames; renaming the weapon ID never silently renames the models. Select sequences, bodygroups and skins in the model preview; indices must exist in the chosen `.mdl`.

**Orbit / Arcball:** drag left to rotate around the model. **Pan:** drag right. **Zoom:** mouse wheel. **Front/Back/Left/Right/Top/Bottom:** orthographic presets. **Fit:** frame model. **Reset:** restore default camera. **FOV:** camera field of view, expressed as an angle in degrees. **First Person:** an approximation for view-model inspection, not a promise of identical engine bob/hand offsets. **Preview Scale:** multiplies *on-screen display* only; it never edits `.mdl`, collision proxies, hitboxes, physics or model attachments. **Playback Speed:** changes preview timing only. Use **Open in pxmv** for richer dedicated model inspection.

## 11. Conditional assets and other resources

Weapon asset variants (`asset.<variant>.*`) may provide alternate model/sound/sprite roles and a condition such as `ammo:<cartridge_id>`. Ammo-specific overrides (`ammo.<cartridge_id>.<field>`) are scoped to that weapon. Paths for models/sprites are game-root-relative; sound paths omit the `sound/` folder. Resource files must exist for useful previews. `sprite` controls HUD imagery, which differs from the player's model or the game's internal inventory ID. Never assume choosing a preview camera changes the game asset.

## 12. Renaming categories, weapons, and cartridges

**Category Rename:** reorganizes the editor's folder path, including nested categories. Categories do not determine internal weapon/cartridge IDs. Use category F2 / context menu, and examine any moved assets before saving.

**Asset Rename ID:** select a weapon or cartridge, press **F2**, or use **Asset → Rename asset ID**. Enter a new 1–63 character ASCII ID using letters, numbers, `-` or `_`. The editor checks collisions including Windows case-insensitive names, and performs an all-or-nothing transaction. A single Undo (Ctrl+Z) restores the former ID and references. Redo (Ctrl+Y) reapplies. Nothing changes on disk until Save/Export (Ctrl+S).

When **renaming a cartridge**, the editor updates its manifest and `.cartridge` filename, accepted cartridge lists on weapons, `default_cartridge`, `ammo_name` **only if that exact field equals the renamed ID**, `ammo.<id>.*` override keys, and typed conditional `asset.<variant>.when = ammo:<id>` values. It does **not** rewrite arbitrary strings that happen to mention the old text. It preserves default/custom override values such as **zero spread**.

When **renaming a weapon**, the editor updates its manifest and `.weapon` definition filename while preserving `runtime_id`, referenced model/sound paths and other unrelated assets. Do not assume this updates an external `weapon_<id>` entity or an engine plugin.

The save system stages and validates owned files, checks whether any of them changed externally since opening, installs the new manifest and definitions, and attempts rollback on I/O errors. If another program changed a file, reopen rather than overwriting their work. The **old definition file disappears only after successful Save**.

**External references WARNING:** the editor cannot safely find and rewrite map files, scripts not managed by this project, third-party tools or old save games. Back those up and update them manually. Changing an ID is therefore a bigger operation than changing Display Name.

## 13. Undo, validation, and troubleshooting

- **Ctrl+Z / Ctrl+Y:** undo/redo edits and rename transactions. Export/save may still require restarting the game to refresh loaded definitions.
- **F5 Validate:** checks definitions with the game's C++ parser and relevant runtime preflight rules. Fix missing fields, incompatible attack/damage combinations and invalid numbers before export.
- **Cannot rename:** check the new ID's spelling, case-insensitive duplicate, unlisted destination file or definition validation error. You should not need to delete files to work around a collision.
- **Weapon doesn't fire:** verify loaded/allowed cartridge, Primary Behavior, ammo count, and game log errors. If server ballistics are disabled, a ballistic cartridge should still fire using instant hitscan with muzzle damage (J.11.5.1+), but no ballistic flight/penetration.
- **Gun sound silent:** check `.wav` path under `sound/`, custom-vs-default precedence, then try the WAV preview.
- **Spread looks unchanged:** check whether the cartridge has its own override; blank inherits, `0` does not.
- **Model is blank:** load a valid Studio `.mdl`, Fit the camera, check animation/skin/bodygroup, and run Windows GUI/DPI checks if the tab page is empty.
- **Wrong damage:** confirm the loaded cartridge's Attack Logic and Damage Logic, single projectile versus shot count, armor/hitgroup, and whether the test actually hit the target.
- **Unexpected rename effects:** undo prior to saving; after export, restore a backup or use the editor to rename back. Examine external map references separately.

## 14. Searchable field index

For each field below, select it in the editor to read the full **What / When / Related / Example / Mistake / See Also** explanation. Where a key belongs to an older format it may be shown but unavailable when authoring Version 4.

| Property group | Common fields | Related topic |
|---|---|---|
| Cartridge identity | `version`, `id`, `category`, `chamber`, `projectile` | Sections 2, 6, 12 |
| Cartridge motion | `mass_grams`, `diameter_mm`, `drag_coefficient`, `reference_velocity_mps`, `projectile_count` | Sections 6–7 |
| Cartridge damage | `attack_logic`, `damage_logic`, `default_damage`, `legacy_bullet_type`, `base_damage`, `damage_coefficient`, older `damage` | Section 2 |
| Weapon identity | `version`, `id`, `display_name`, `runtime_id`, `category` | Sections 8, 12 |
| Inventory and HUD | `ammo_name`, `default_cartridge`, `clip_size`, `default_ammo`, `max_carry`, `slot`, `position`, `weight`, `flags` | Section 8 |
| Weapon defaults | `default_fire_rate`, `default_spread`, `default_fire_sound`, `reload_seconds`, `range`, `muzzle_flash`, `fire_underwater`, `auto_reload`, `npc_usable` | Sections 3–5, 9 |
| Ammo overrides | `ammo.<id>.fire_rate`, `ammo.<id>.spread`, `ammo.<id>.fire_sound`, `ammo.<id>.view_model`, `ammo.<id>.player_model`, `ammo.<id>.world_model`, `ammo.<id>.reload_sound` | Sections 3–6, 11 |
| Primary attack | `primary_behavior`, `primary_mode`, `primary_projectiles`, `primary_ammo_cost`, `primary_burst_count`, `primary_recoil_pitch`, `primary_recoil_yaw`, `primary_animation`, `primary_sound`, `primary_event` | Section 9 |
| Secondary attack | `secondary_behavior`, `secondary_mode`, `secondary_projectiles`, `secondary_ammo_cost`, `secondary_burst_count`, `secondary_recoil_pitch`, `secondary_recoil_yaw`, `secondary_animation`, `secondary_sound`, `secondary_event` | Section 9 |
| Models and animation | `view_model`, `player_model`, `world_model`, `fire_animation`, `draw_animation`, `reload_animation`, `idle_animation`, `idle_seconds`, `animation_extension` | Section 10 |
| Sound and HUD assets | `reload_sound`, `pickup_sound`, `fire_event`, `sprite`, `asset.<variant>.role`, `asset.<variant>.path`, `asset.<variant>.when` | Sections 5, 11 |
| Earlier formats | `fire_interval`, `spread`, `bullet_type`, `fire_sound`, `primary_interval`, `secondary_interval`, `primary_spread`, `secondary_spread` | Sections 2–5; do not introduce into V4 files |

---

*Documentation coverage and source tests are not proof of a Windows GUI or full PhysX build. Compile/run the editor on supported Windows targets and play-test the gameplay changes before claiming end-to-end verification.*


## J.11.6: validation, sounds, saving, and NPC shooting

**New assets:** The normal **New Weapon** and **New Cartridge** commands now create
Version 4 definitions. To make an old-format example, choose the explicit
**New legacy-format weapon (V2)** or **New legacy-format cartridge (V2)** menu
command. Duplicating a file preserves its existing version. Create a cartridge
first, then create your weapon; if there is no compatible Version 4 cartridge
when you create the weapon, it remains a draft until you choose one.

**Script-valid vs. engine-ready:** Validation now calls a project **script-valid**
when the definitions parse, reference valid assets, and pass best-case capacity
checks. It cannot assume that all original Half-Life ammo slots and weapon IDs
are free! The target game's engine may have registered many of them already.
The editor therefore says **Engine admission: UNKNOWN** unless real registration
counts and names from the game are supplied to its validation API. At startup,
the game itself checks the true counts. Never treat script-valid alone as proof
that all weapons will load; check the game's log during a test run.

**Missing resources:** The editor looks for referenced resources as loose files
and in local pak-numbered PAK archives. Resources missing from both receive a
warning rather than causing a save failure. This is intentional: the game may
load a model, sprite, event or sound from a mounted base game folder or a custom
virtual filesystem that the standalone editor cannot inspect. A warning means
**unverified**, not definitely missing. Test inside the running game.

**Conditional firing sounds:** For a Version 4 weapon, the sound priority is
(1) loaded-cartridge direct `fire_sound` override, (2) matching conditional
`primary_sound`/`secondary_sound` or `fire_sound` asset (ammo, mode, random,
default tiers), (3) `default_fire_sound`. Random variants use the existing pinned
shot seed and action, so the selection is deterministic for that shot. The
editor's per-ammo inherited display cannot predict a random sound variant.

**NPC Fire Rate:** Version 4 ammunition RPM now caps how quickly an NPC can
produce successive shots from the same weapon. NPCs do not automatically fire
at that rate: monster schedules and firing animation events still determine when
a shot is requested. A request made during cooldown produces no shot, consumes
no ammunition and plays no sound. This is an upper bound, not new AI scheduling.

**Recovery after an interrupted Save:** pxwe writes an on-disk stage journal
before replacing definition files. If it detects an unfinished transaction,
it stops rather than loading or saving possibly mixed files. The Windows editor
asks whether to restore the original files from that transaction; choosing No
leaves them untouched for manual inspection. Recovery requires the journal; an
older incomplete stage without it must be inspected manually. Back up your mod
before accepting recovery. Abrupt power loss during filesystem writes can still
defeat software-only recovery on some filesystems; this is not a full durable
database transaction.

**Cartridge Chamber:** The chamber string describes compatible ammunition to
a human editor. The current runtime does **not** reject mixed-chamber names in
a weapon's accepted list. Authors are responsible for designing realistic
compatibility, unless a future feature adds enforced chamber checks.
