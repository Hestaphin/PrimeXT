# pxwe — PrimeXT Weapon Editor (J.11.6)

J.10.6.9.9 repairs an empty native Windows editor: the tab-control HWND is now
limited to the tab strip so it cannot cover the sibling property/preview panels.
The editor also explicitly selects the first tab on creation and defaults to
Properties if the Windows control temporarily reports no selection. J.10.6.9.8's
Windows SDK `small` macro fix and earlier weapon/asset features are retained.

The default editor is a **native C++17 Windows GUI**, compiled as **pxwe.exe**.
It uses Windows controls and GDI, with the game's modular weapon parser and
ballistic integrator linked into the executable. End users need the executable
and their game files. They do not need Python, source, a compiler, CMake, PhysX,
Qt, mxToolKit, or a separate validator executable.

The earlier Python/Tk interface remains as a legacy developer tool. It is no
longer the default launcher or the distributed editor.

## Build on Windows

Install Visual Studio's Desktop development with C++ workload, including a
Windows SDK, and CMake 3.19+. From the PrimeXT source directory:

```bat
cmake -S tools/weapon_editor -B build-pxwe -A x64
cmake --build build-pxwe --config Release --target pxwe
cmake --install build-pxwe --config Release --prefix "C:/Games/Xash3D/primext"
```

This standalone build bypasses the root project's vcpkg toolchain. It does not
need the engine or game DLLs. For 32-bit Windows, use `-A Win32` and a separate
build directory. With Visual Studio, the built file is
`build-pxwe/Release/pxwe.exe`. MSVC Release builds use the static C/C++ runtime
(`/MT`). Only `pxwe.exe` is installed by this target.

`build_pxwe.bat "C:\Games\Xash3D\primext"` is a developer shortcut for those
commands. Ship the resulting executable in the game download.

The full PrimeXT CMake build adds `pxwe` when `BUILD_UTILS=ON` and
`BUILD_PXWE=ON` (default), and installs it into `primext/devkit` alongside
pxmv. Existing caches can use `cmake --build <build-dir> --config Release --target pxwe`.

## Install and launch without source

Put the executable in `primext/devkit/pxwe.exe` and double-click it. It finds
`primext` from its own location, independently of the shortcut's working
directory. Direct installation as `primext/pxwe.exe` also works. `bin` and
`tools` installations use their parent folder. An explicit argument wins:

```bat
pxwe.exe "C:\Games\Xash3D\another-mod"
```

**File → Open game folder** selects another mod. An existing empty directory
starts a new project. The title marks committed unsaved changes with `*`.
Apply, selection changes, Save and closing commit the current property value.

## Workflow

The left tree groups Weapons and Cartridges into their format-3 categories.
Right-click a root, category or asset (or use the context-menu key) to create
assets and nested categories, move or duplicate assets, and delete assets.
Category menus also offer rename (**F2**), subtree move/duplicate, **Remove
category (keep assets)** and **Delete category and its assets** as separate
confirmed actions. Removing a category moves all its subtree's assets to its
parent; deleting its assets rejects cartridge references still used by weapons.
Move uses a destination category for assets and a destination **parent** for
folders. Blank means the root. Rename uses the complete new category path.
Cycles and occupied subtree destinations are rejected; moves do not merge.

Categories use case-sensitive ASCII ID segments separated by `/`, up to 16
levels and 255 characters. IDs, resource paths and definition filenames remain
stable. Duplicated folder assets use `prefix_<original ID>`; duplicated weapons
receive unused runtime IDs and retain their original cartridge references.
Category copies include empty subfolders and leave the originals in place.
Each category operation is one undo action; cancelled or rejected operations
leave the operation's data and history untouched. A pending property edit is
committed before a context action, following the existing editing workflow.

New assets inherit the selected category when their type matches its root.
Editing the `category` property or moving a legacy cartridge upgrades it to
format 3 and converts damage in one transaction, preserving damage at its
reference velocity. The shared parser checks damage at all weapon velocities.
Existing invalid definitions should be repaired before category moves/copies.
Asset categories imply folders and their parents. Explicitly created folders,
including empty ones, persist in `scripts/editor_categories.txt` (version 1,
repeatable `weapon = path` / `cartridge = path`, up to 256 explicit folders).
The game ignores that editor file and continues to use flat manifests and the
asset `category` fields. The sidecar shares export staging, external-edit
protection, undo and redo. Legacy packs without folders gain no sidecar.

1. **Asset → New cartridge** creates an ammunition definition with an ASCII ID.
2. **Asset → New weapon** creates a weapon. **Properties** contains identity, physics, inventory, attack behavior and NPC support.
   Model, sound, event and animation fields are on **Model / Animations**;
   sprite fields are on **Sprites**. The property editor works on all three pages. Select a row for detailed help; double-click a
   value (or press Enter while the property list has focus) to focus its value
   editor. Edit below, then Apply or Enter. The help pane scrolls and its text
   can be copied. Double-click a section heading or press Enter to collapse or
   expand it; this changes visibility only and preserves hidden values.
   Clearing a value removes that property; validation checks whether omission
   is allowed. IDs are read-only. Duplicate creates a new ID and allocates a
   fresh runtime weapon ID. Creation rejects existing IDs and unlisted disk
   definitions, including Windows case aliases, before adding a draft. A
   rejected ID can be corrected in the same creation flow.
   New weapons receive `models/v_<ID>.mdl`, `models/p_<ID>.mdl`, and
   `models/w_<ID>.mdl`. These are editable path suggestions; the editor does
   not generate model files or remap their animations. Duplicate preserves
   authored model paths.
   **Browse...** is enabled for models, sounds, sprites and event scripts.
   It writes game-relative paths; sounds are relative to `sound/`. Choose
   loose files inside the open game folder. Unsupported file types, paths
   outside that folder, and names outside the game's ASCII path grammar are
   rejected. Cancel leaves the pending value unchanged. Existing format-3
   conditional paths and ammo override paths use their role's resource type.
   Browse selections use normal Apply/history/save behavior.
3. **Cartridges** lists all ammunition. Select a row and **Allow / Edit velocity**,
   **Disallow**, or **Set default**. Muzzle velocity is per weapon/cartridge.
   Set default also updates `ammo_name`. Switch defaults before disallowing
   the current one. Remove its overrides/ammo conditions before disallowing it.
   The muzzle-damage column is read-only and calculated per projectile.
   **Edit coefficient...** changes the selected cartridge's coefficient; its
   **damage_coefficient** property is also editable on the cartridge's Properties
   page. Converting formats 1/2 to 3 removes legacy damage in the same undo
   transaction, with a suggested coefficient that preserves reference-speed
   damage. Other weapon velocities then follow energy/area scaling. Validation
   checks all using weapons before accepting the conversion.
   **Ammo assets / sounds...** edits firing/reload sounds and model/sprite/HUD/kill
   overrides for allowed ammo. Blank clears that override and restores fallback.
   Firing uses loaded ammo; reload uses selected ammo. A cartridge icon edit also
   converts legacy damage atomically when format 3 is required.
4. **Ballistics** compares every allowed cartridge at its weapon-specific muzzle
   velocity (a selected cartridge uses its reference velocity). Check the ammo
   you want to display; colors match the legend. Select trajectory, kinetic
   energy or damage per projectile. Curves use the game integrator, gravity and
   drag at default air density for a two-second horizontal launch. No collision,
   penetration, aggregate pellet damage or game difficulty multiplier is plotted.
5. **Model / Animations** edits models, sounds, events and animation indices.
   **Load model** renders indexed textures and animated triangle geometry inside
   pxwe. Select a sequence in the list to start it, **Play / Pause** to pause,
   **Restart** to return to its first frame, and drag the preview to orbit.
   Looping sequences repeat; other sequences stop at their final frame. Skin
   and body selectors change texture family/body groups. **Play sound** previews
   a selected sound property or sound entry using Windows WAV playback.
   Conditional-entry **Add**, **Edit**, **Duplicate** and **Remove** use atomic,
   validated, undoable changes. Roles can use default, random, mode or allowed
   ammo conditions. Duplicating an explicit default suggests random so it does
   not create a second default. Cancel changes nothing. **Preview entry** loads
   that exact model/sound path; it does not simulate the runtime random selector.
   Intentional conditional/override edits upgrade the weapon to format 3.
   **Open in pxmv** remains an optional external viewer for loose files.
   **Sprites** offers the same conditional controls for sprite/HUD/kill roles
   and previews indexed .spr transparency/frames or the selected .txt HUD crop.
   Its selector also includes ammo overrides and a cartridge's icon.
6. **F5** validates with the game's C++ `modular::Registry`, in process.
   **Ctrl+S** exports scripts and manifests. **Ctrl+Z / Ctrl+Y** undo/redo.

Only files listed in the two manifests are opened. Structural errors, missing
assets and malformed manifests reject Open without changing the current project.
Semantically invalid values can be opened and repaired. IDs avoid Windows
case-insensitive filename collisions.

Export validates before writing, stages files inside the mod directory, backs up
owned files, then installs data before manifests. It removes only formerly owned
assets deleted in the editor. Ordinary I/O failures restore original files;
incomplete recovery retains the stage and reports its location. Changes made
outside pxwe since Open/Save, or existing unlisted files with a new asset's name,
block export. Multi-file export is not power-loss atomic.

## Limits and verification

Run `python3 game_shared/tests/verify_j10_6_9_7.py --ubsan` for category backend
and actual controller methods (with explicit OS adapters), then the inherited
resource, usability, runtime, sky and physics acceptance chain. Adapters do not
certify native Windows controls, message ordering, SDK links or live gameplay.

- The game needs the matching modular weapon runtime. pxwe does not install game
  DLLs or reload a running game. Restart/load the game after editing.
- Runtime IDs are 32..62, with 63 reserved for the suit. The 32 ammo slots include
  base-game ammunition. The editor checks its pack; the target game's occupied
  slots require final runtime admission.
- Built-in behaviors are hitscan, ballistic, ammo_cycle and none. Native extension
  DLL authoring is outside this editor.
- Definition manifests remain loose files. Model, sprite and sound previews read
  loose resources first, then pak<number>.pak in descending numeric order.
  Companion *T.mdl textures and IDSQ sequence-group files are supported. Browsing
  selects loose files; archived paths can be entered manually.
- The software model preview supports Studio v10 indexed textures, masked/additive
  materials, RLE bone animation (first blend), neutral controller rest values,
  weighted vertices and half-float UVs. It does not emulate engine lighting,
  shader materials, animation blend/IK/procedural-bone controls or events.
  The preview reader bounds allocations, offsets, dimensions and work. Large
  assets exceeding those limits report an error; pxmv can inspect such files.
- Sprite playback preserves authored origins and group intervals. Angled sprite
  groups are displayed as a frame sequence. HUD lists select a 640-resolution
  weapon/ammo entry when available; the preview is a crop, not a HUD simulation.
- Windows compilation, font/DPI behavior and visual acceptance need a native
  Windows build/run. Automated backend checks do not certify the GUI.

```sh
python3 game_shared/tests/verify_j10_6_9_5.py --ubsan
```

Python is only the developer test driver; pxwe does not invoke it. The C++ tests
exercise real manifests/files, history, editing, compatibility, game validation,
export, external changes, rollback, textured triangle output, RLE/controller/weighted
poses, companion files, PAK precedence, sprite palette/alpha/group timing,
malformed-resource bounds, coefficient/override transactions and comparison math. Existing
example packs round-trip with matching fingerprints. The retained step-6 and new step-7/8 controller tests
execute real Application methods with explicit Windows control/dialog adapters;
they do not test Windows rasterization, fonts, native message ordering or links.

Alternatively build native tests with CMake:

```sh
cmake -S tools/weapon_editor -B build-pxwe -DPXWE_BUILD_TESTS=ON
cmake --build build-pxwe --config Release
ctest --test-dir build-pxwe -C Release --output-on-failure
```

Non-Windows builds produce a headless `pxwe --validate <game-folder>` interface.
The Windows GUI is separate. The older `primext-weapon-validate` target and
Python files remain for inherited developer checks.

A redistributable `documentation/examples/native-editor9_6` pack supplies a
colored animated flag model, two cartridge definitions, conditional/override
examples, sprites and short generated WAV tones. Open that directory with pxwe
for a self-contained preview exercise. These are editor fixtures, not replacement
weapon art.

## J.10.6.9.9 GUI smoke test (Windows required)

Open `sample_editor` from the patch bundle. The editor must show property rows
for `preview_pistol` on the Properties tab, and `preview_fmj` must show cartridge
properties when selected. Select every tab in turn: Cartridges has column
headers, Ballistics shows a graph area, Model / Animations shows controls and
a model viewport, and Sprites shows controls and a sprite viewport. Resizing
the window and switching back and forth must not produce an empty page.
This visual test must be done with the native Windows executable; portable
Linux backend tests cannot validate Windows common-control painting.


## J.11.4 — per-cartridge workflow and embedded model camera

On **Cartridges**, select an allowed cartridge and double-click its row (or use
**Edit cartridge settings...**). The table labels each **Effective Fire Rate
(RPM), Spread, Fire Sound** as *Inherited* or *Custom*; scroll horizontally to
see **Attack Logic** and **Damage Logic** (read-only cartridge attributes).
The dialog has individual **Inherit** buttons for RPM and spread, and an
**Inherit fire sound** button. Clearing just one override does not reset the
others. A spread of `0` is a deliberate custom value, not inheritance. The
live summary shows which value will take effect, including the weapon default
when the field is blank. **Play effective WAV** previews the loaded sound,
using the game's `sound/` folder; browsing and the full parser validate paths
on save. Reload/asset overrides and undo/redo continue to use the existing
project transaction machinery. Double-clicking a cartridge that is not yet
allowed still opens Allow/Edit velocity.

The **Model / Animations** preview offers orbit (left drag), pan (right drag),
wheel zoom, **Fit**, **Reset view**, adjustable **FOV** (30..110 degrees),
**Scale** (0.1..8, display-only), animation **Speed** (0.1..4), and camera
presets **Orbit**, **First Person**, **Front/Back/Left/Right/Top/Bottom**.
First Person is an *editor framing approximation*, not a live engine camera
with game-side bob, view offsets, hand positions or weapon FOV enforcement.
It is automatically suggested when loading a `v_*.mdl`. Orbit/First Person
render with bounded perspective; directional presets are orthographic. Camera
settings never write to `.weapon` or `.mdl`, physics bodies, attachments or
hitboxes. Rendering remains a built-in software preview and **Open in pxmv**
remains available.

**Platform validation:** portable C++ tests cover projections, camera commands,
reset/fit, finite limits, and unchanged model data. A real Win32 x64/x86 GUI
must still be checked for DPI scaling, child-control overlap and interaction.


### J.11.5 — Rename IDs and beginner guide

Select a weapon or cartridge and press **F2** or use **Asset → Rename asset ID...**.
For a category, F2 keeps the existing category-rename action. Cartridge renames
update editor-owned accepted/default ammunition, `ammo.<id>.*` override keys
and conditional `asset.*.when = ammo:<id>` references in one Undo/Redo step.
Weapon renames preserve the independent numeric `runtime_id` and model/sound
paths. Both use the existing validated staging/save rollback system; an owned
old definition file is replaced only after a successful Save/Export. **External
map scripts and saved games are NOT rewritten.** Keep backups.

Use **Tools → Open Beginner Guide...** to read
`PrimeXT_Weapon_Editor_Beginner_Guide.md` (installed to `devkit/`), or **Tools →
Copy selected property help** to copy the contextual, scrollable help panel.
The guide includes detailed tutorials, per-cartridge override inheritance,
attack/damage logic, RPM, model previews, renaming, and troubleshooting.

This milestone's portable test suite is run with
`cmake -S tools/weapon_editor -B build-pxwe -DPXWE_BUILD_TESTS=ON` then
`cmake --build build-pxwe` and `ctest --test-dir build-pxwe --output-on-failure`.
These tests do **not** replace a Windows GUI or PhysX in-game acceptance run.


For the J.11.6 cartridge-owned firing changes, editor admission warnings,
interrupted-save recovery and explicit legacy templates, see the installed
`PrimeXT_Weapon_Editor_Beginner_Guide.md` section "J.11.6".
