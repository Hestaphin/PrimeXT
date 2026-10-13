# J.10.6.10.1 completion

This finishes the remaining executable F1 checks in the supplied environment.
It keeps the F1 milestone and scope. Anatomical fitting and symptom repair
remain later phases.

## Changes and observed evidence

- Float and integer panel readers copy the toolkit label before parsing. Qt's
  returned text is borrowed until the next label read; retaining parse pointers
  while calling the getter again could invalidate them.
- The matching binary Barney model now exercises production vertex bounds,
  candidate isolation/apply, ordinary body and mixed angular-mode edits, invalid
  field rejection, Save, Reload, Cancel, Undo and model-path invalidation.
  All 23 checks pass under UBSan. Bounds agree with the numeric audit fixture
  within 0.0002 units. Both supplied files retain their original hashes.
- The complete F1/inherited run with the model passes 13/13 available groups.
  Historical F1 evidence is retained separately. Three inherited Xash syntax
  groups remain blocked by the original missing `library_suffix.h` dependency.
- A real native build runner is included. The current run records BLOCKED
  because CMake is unavailable, and performs no configuration or linking.

The actual-model harness supplies bind transforms and GUI services. It does
not run the complete native loader, Qt/Win32 controls, GL rendering, PhysX
solver, Windows file replacement, live game deaths, or FPS measurement.
No native binaries or physically certified replacement Barney asset are
included.

## Reproduce the available checks

From patched PrimeXT root, with Python 3 and a GCC-compatible C++17 compiler:

```sh
python3 game_shared/tests/verify_j10_6_10_1.py --ubsan --inherited --barney-model /path/to/barney.mdl --json f1_actual_model.json
```

The optional model must match the SHA in `BARNEY_AUDIT.md`. To check both
supplied inputs explicitly:

```sh
python3 game_shared/tests/ragdoll_template_model_integration.py --ubsan --model /path/to/barney.mdl --asset /path/to/barney.phys
```

The runner writes only temporary copies. The asset argument accepts any
filename whose bytes match the supplied reference.

## Run the native build on the supported machine

Install the project's CMake/toolchain dependencies and matching PhysX, toolkit
and graphics development packages. Select the architecture/triplet matching
your game build. Point `VCPKG_ROOT` at the installed vcpkg distribution, or
supply its exact toolchain path. Use a build directory outside the PrimeXT
source root.

For an already configured compiler environment:

```sh
python3 game_shared/tests/verify_j10_6_10_1_native.py --build-dir ../build-f1-native --json ../f1-native.json
```

For Visual Studio 2022 and a 32-bit game build, for example:

```sh
python3 game_shared/tests/verify_j10_6_10_1_native.py --generator "Visual Studio 17 2022" --arch Win32 --triplet x86-windows --toolchain "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" --build-dir ../build-f1-win32 --json ../f1-native-win32.json
```

For the PhysX-disabled variant, repeat in another directory with
`--physx OFF`. Use `--prefix` for installed dependency prefixes if needed.
The runner records exact configure/link commands and separate logs; PASS
means the real `server`, `client` and `pxmv` targets linked. Missing CMake
reports BLOCKED (exit 2); configuration/link failures report FAIL and retain
the native logs.

After a successful build, carry out [ACCEPTANCE.md](ACCEPTANCE.md) in real pxmv
and the game. Native controls, solver behavior, stability, rendering parity
and performance remain unobserved until that run is performed.

## Apply the completion

The bundle includes a small `PrimeXT_J.10.6.10.1_completion.patch` for the exact
previous F1 delivery and a combined-directory counterpart. Other regenerated
routes include all completion changes. Select one route that matches your
starting source; do not stack the completion patch on the refreshed F1 patch.
