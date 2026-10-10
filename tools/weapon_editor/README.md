# pxwe — PrimeXT Weapon Editor (J.10.6.9)

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

1. **Asset → New cartridge** creates an ammunition definition with an ASCII ID.
2. **Asset → New weapon** creates a weapon. **Properties** exposes every supported
   field, including primary/secondary attacks, magazine/reserve values, assets,
   animations and NPC support. Select a row, edit below, then Apply or Enter.
   Clearing a value removes that property; validation checks whether omission
   is allowed. IDs are read-only. Duplicate creates a new ID and allocates a
   fresh runtime weapon ID.
3. **Cartridges** lists all ammunition. Select a row and **Allow / Edit velocity**,
   **Disallow**, or **Set default**. Muzzle velocity is per weapon/cartridge.
   Set default also updates `ammo_name`. Switch defaults before disallowing
   the current one.
4. **Ballistics** plots a horizontal launch for two seconds using the real game
   integrator, gravity and drag at default air density. It reports distance,
   drop and energy. There is no collision or penetration preview.
5. **Model / Animations** reads loose Studio v10 models, lists sequence indices,
   names, FPS and frame counts, and shows bounds. Drag to orbit the bounds.
   **Open in pxmv** uses an installed sibling or `devkit/pxmv.exe` for textured
   mesh/animation; the embedded preview shows bounds and metadata.
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

- The game needs the matching modular weapon runtime. pxwe does not install game
  DLLs or reload a running game. Restart/load the game after editing.
- Runtime IDs are 32..62, with 63 reserved for the suit. The 32 ammo slots include
  base-game ammunition. The editor checks its pack; the target game's occupied
  slots require final runtime admission.
- Built-in behaviors are hitscan, ballistic, ammo_cycle and none. Native extension
  DLL authoring is outside this editor.
- Extract models/definitions from PAK archives before editing.
- Windows compilation, font/DPI behavior and visual acceptance need a native
  Windows build/run. Automated backend checks do not certify the GUI.

```sh
python3 game_shared/tests/verify_j10_6_9.py --ubsan
```

Python is only the developer test driver; pxwe does not invoke it. The C++ tests
exercise real manifests/files, history, editing, compatibility, game validation,
export, external changes, rollback, flight and bounded Studio metadata. Existing
example packs round-trip with matching fingerprints.

Alternatively build native tests with CMake:

```sh
cmake -S tools/weapon_editor -B build-pxwe -DPXWE_BUILD_TESTS=ON
cmake --build build-pxwe --config Release
ctest --test-dir build-pxwe -C Release --output-on-failure
```

Non-Windows builds produce a headless `pxwe --validate <game-folder>` interface.
The Windows GUI is separate. The older `primext-weapon-validate` target and
Python files remain for inherited developer checks.
