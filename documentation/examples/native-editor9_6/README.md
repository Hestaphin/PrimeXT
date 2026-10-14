# J.10.6.9.6 native editor fixtures

Open this directory as the game folder in pxwe. It is a separate editor fixture pack; it does not need a running game.

- Model / Animations: Load model. Sequence 0 is a still colored flag; sequence 1 moves it horizontally. Select a skin, orbit, pause and restart.
- Add a conditional entry; duplicate `view_default` (the copy suggests random). Edit an ammo or mode condition and try Undo / Redo.
- Select a sound property and Play sound. The WAV files are short generated tones.
- Cartridges: select each allowed cartridge to inspect computed muzzle damage. Edit coefficient or Ammo assets / sounds. Blank clears an override.
- Sprites: preview default HUD crops, conditional HUD entries, ammo-specific sprites and cartridge icons. Frames change color; the transparent quadrant shows the checkerboard.
- Ballistics: compare both cartridges, uncheck one, and switch trajectory / energy / damage.
- Save, reopen and validate. These fixtures use format 3. Do not replace a real mod's manifests with them.

Studio and sprite bytes originate from `tools/weapon_editor/tests/native_preview_fixtures.h`. All geometry, palettes and tones are generated for this release and distributed under GPL-3.0-or-later. They are preview fixtures, not finished weapon art. The model reader defaults to the first animation blend and controller rest values; full engine lighting, shader/IK/procedural behavior is outside this preview.
