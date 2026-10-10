# J.10.6.9 — Native pxwe.exe weapon editor

J.10.6.9 adds a standalone C++17 Windows Weapon & Cartridge Editor named
**pxwe.exe**. The earlier Python/Tk GUI required Python and source-based
launch steps. It was not a finished distribution path for users who download
the game without its source. The native target links the game parser and
ballistic integrator into the editor, uses Win32 controls/GDI, and opens the
game folder from the executable's location.

## Scope

- Full native window with asset tree, scrollable property table, editable values,
  built-in enum suggestions, cartridge compatibility, ballistic plot and model
  metadata/bounds tabs. No engine process or Python subprocess is used.
- Create, duplicate, delete, undo and redo; duplicated weapons receive a fresh
  runtime ID. Every supported weapon/cartridge schema field is editable.
- In-process authoritative validation using the existing `modular::Registry`
  and runtime preflight. No game parser/ballistic algorithm changes.
- File-based export with manifest ownership, staging, backups and rollback.
  Detect external changes and pre-existing unlisted files before replacing data.
- Windows case-insensitive ID checks and bounded Studio v10 metadata loading.
- Real shared ballistic integration for a two-second horizontal launch with
  gravity/drag. No impact, penetration or live game preview.
- Optional installed pxmv launch for full textured model viewing. Embedded model
  preview lists animation metadata and displays orbitable bounds.
- Unicode Windows paths, embedded common-controls/DPI manifest, resize layout,
  monitor-aware initial sizing and font/layout changes on DPI notifications.
- Standalone CMake build requiring C++17, a Windows SDK and CMake 3.19+, with
  static MSVC runtime. Integrated `BUILD_PXWE` utility target/install.
- Default Windows launcher now runs the native application. Existing Python
  files and validator target remain for legacy developer checks.
- Non-Windows builds provide a portable validation CLI; they do not provide
  the Win32 GUI.

J.10.6.7 runtime and J.10.6.8 physics editor repairs remain included in the
cumulative routes. This release's incremental J.10.6.8 patch changes the
weapon authoring tool and its build/documentation only.

## Build and distribute

From the PrimeXT source directory, with Visual Studio C++ and a Windows SDK:

```bat
cmake -S tools/weapon_editor -B build-pxwe -A x64
cmake --build build-pxwe --config Release --target pxwe
cmake --install build-pxwe --config Release --prefix "C:/Games/Xash3D/primext"
```

Standalone configuration bypasses the root vcpkg integration. The executable
is `build-pxwe/Release/pxwe.exe` with the Visual Studio generator. Only the
executable is installed to `primext/devkit/pxwe.exe`. Ship that executable
with the game download. `tools/weapon_editor/build_pxwe.bat <game-folder>`
runs the equivalent build/install steps for a developer.

The full PrimeXT build includes the target by default when utilities are
enabled. Build `pxwe` and install it with the other devkit applications.

Double-click the installed executable to open its parent game folder.
`primext/pxwe.exe` also supports direct installation. An explicit mod path
overrides discovery, and File > Open game folder selects another project.
Source files, Python and a separate validator are not runtime inputs.

## Automated evidence

`python3 game_shared/tests/verify_j10_6_9.py --ubsan` passes **4/4 groups**:

1. Real C++ backend build with strict warnings and UBSan: **23/23 cases pass**.
   Includes editing, history, compatibility/defaults, duplicate runtime IDs,
   31-ID exhaustion, structural/semantic error handling, no-write invalid export,
   unrelated file preservation, external edits and a forced mid-export I/O failure
   with byte-identical rollback. Three existing example packs round-trip with
   unchanged fingerprints.
2. Real portable native pxwe CLI compiles with the same game sources.
3. A copied native binary validates an installed game-data folder from an unrelated
   working directory without source/Python files. Invalid cartridge data is rejected
   and no files change. This establishes the portable executable's source independence;
   Windows automatic discovery is separately exercised by the backend path tests.
4. Inherited J.10.6.8 acceptance: **5/5 groups**, including **17/17** editor method
   cases and inherited J.10.6.7/J.10.6.6 checks, pass.

The sandbox does not contain CMake, a Windows compiler/SDK or a Windows runtime.
**This bundle contains source patches, not a prebuilt pxwe.exe.** The Win32 GUI
and resource have not been compiled, linked or visually run here. Portable
tests do not establish native widget behavior, Windows DLL dependencies or
high-DPI painting. Build and visually accept the Windows target before shipping.

## Windows acceptance

1. Build the standalone Release target and install it into a game folder with a
   space/non-ASCII character in its path. Close source-based tools; launch pxwe
   from a shortcut with an unrelated Start In directory. Confirm it opens the
   installed mod. Repeat with an explicit second mod argument.
2. Create a cartridge and weapon, edit properties near the end of the scrollable
   list, and exercise enum suggestions, Apply/Enter and switching assets/tabs.
   Confirm pending values are included by F5, Save and close prompts.
3. Duplicate a weapon and verify unique runtime IDs. Assign different muzzle
   velocities for the same cartridge in two weapons; switch defaults and undo/redo.
4. Save, close and reopen. Confirm manifests, values, cartridge defaults and
   fingerprint. Set clip_size to 0, confirm F5 reports an error and Save leaves
   files unchanged, then repair it.
5. Confirm a used/default cartridge cannot be removed or disallowed. Delete an
   unused asset, save and confirm its owned file is removed while unrelated files
   remain. Cancel an unsaved close/open and confirm the project stays open.
6. Load a loose Studio v10 model, inspect sequence indices/FPS/frame counts,
   drag the bounds preview, and launch an installed pxmv for full model viewing.
7. Resize and move the window between monitors; repeat at 100%, 125%, 150% and
   200% scaling. Inspect value controls, compatibility buttons and modal dialogs.

The target game must already contain the matching modular weapon runtime.
The editor checks its pack but cannot know the mod's occupied base-game ammo
slots. The game performs final runtime admission. Restart/load after editing.
Extract packed model/definition files before editing. Multi-file export is not
power-loss atomic; incomplete rollback reports retained recovery files.
