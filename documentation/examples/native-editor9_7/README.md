# J.10.6.9.7 native editor sample

Open this folder with the rebuilt `pxwe.exe`, or copy its contents into a
separate test mod. It extends the 9.6 generated preview fixtures with nested
categories and persistent empty folders.

| Root | Category | Assets |
| --- | --- | --- |
| Weapons | `pistols/service` | `preview_pistol` (runtime ID 32) |
| Weapons | `pistols/backup` | `preview_backup` (runtime ID 33) |
| Weapons | `pistols/service/reserve` | Empty, explicitly saved |
| Cartridges | `9x19/fmj` | `preview_fmj` |
| Cartridges | `9x19/jhp` | `preview_jhp` |
| Cartridges | `9x19/unassigned` | Empty, explicitly saved |

Right-click categories to create, rename, move, duplicate or remove them.
Try duplicating `pistols` to `trial_pistols` with the ID prefix `trial`, then
undo/redo. IDs become `trial_preview_pistol` and `trial_preview_backup`;
models and cartridge references retain their authored paths and IDs. Save
and reopen to verify the empty folders. Removing a category keeps its assets
at the parent. The separate subtree-delete action asks for confirmation;
deleting `9x19` is rejected while either weapon still references its ammo.

The two Studio models, four indexed sprites, HUD crop and four WAV files are
generated diagnostic assets. Model / Animations displays colored textures,
translation sequences and skin changes. Sprites displays animation,
transparency and a HUD crop. Both weapons retain the 9.6 conditional entries
and cartridge overrides; Ballistics compares both allowed ammunition speeds.
These tiny resources demonstrate editor behavior and are not production art.

`scripts/editor_categories.txt` contains only editor folders. Gameplay uses
the flat manifests and asset `category` fields. A test game must use matching
patched DLLs and definitions on client/server; definition edits change
fingerprints. Install this sample in a separate folder to preserve a real mod.

Studio and sprite bytes originate from
`tools/weapon_editor/tests/native_preview_fixtures.h`. Geometry, palettes
and tones are generated diagnostic resources distributed under GPL-3.0-or-later.
The preview uses the first animation blend and controller rest values; full
engine lighting, shader/IK/procedural behavior remains outside this preview.
