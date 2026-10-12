# F1 validation

Run from PrimeXT root with Python 3 and a GCC-compatible C++17 compiler:

```sh
python3 game_shared/tests/verify_j10_6_10_1.py --ubsan --inherited --json f1.json
python3 game_shared/tests/verify_j10_6_10_1.py --ubsan --inherited --barney-model /path/to/barney.mdl --json f1_actual_model.json
```

The verifier compiles the actual shared template/asset sources with exceptions
disabled and strict warnings. It exercises parsing, numeric validation, alias
ambiguity, ValveBiped and generic mappings, actual Barney topology/bounds,
determinism, detail levels, resolved Free Rotation, preservation, stale-preview
guards, cancel/undo and atomic POSIX file replacement/refusal.

The dialog integration extracts actual production editor/dialog methods. GUI,
file picker, message box and engine services are explicit adapters; bone bounds
come from the supplied numeric model fixture. Toolkit tests execute actual Qt
label storage and Qt/Win32 close notification snippets with service adapters.
The motion test executes the real angular setter with an explicit SDK facade
and confirms zero translation writes. Windows persistence is syntax checked
against a declaration-only header; no Windows OS behavior is emulated.

With `--barney-model`, an additional check hashes the actual supplied MDL and
matching `.phys`, copies both into temporary storage, and exercises production
`GetBoneVertexBounds`, editor bone collection, generation, body/joint panel
editing, Save, Reload and Undo. Actual bounds agree with the audited numeric
fixture within 0.0002 units of transform roundoff. The fixture supplies bind
transforms and GUI services; it does not invoke the complete native model
loader, draw pixels, or run PhysX. All **23 checks** pass under UBSan and the
original input hashes remain unchanged. Without the model this check is
explicitly skipped, rather than counted as a pass.

The example factory regenerates three built-in files and two resolved Barney
candidates, compares their exact published bytes, parses them with the actual
asset parser, and verifies a concrete body edit can be serialized/reloaded.

Inherited checks cover existing editor method cases and full changed pxmv units
using GL declarations with PhysX off; F0 recorder/measurement tests remain
enabled. Only F0's unchanged-physics hashes are explicitly skipped because F1
intentionally changes generation and angular-motion fields. The historical
F0 baseline JSON is retained without rewriting its original scope.

The completion run passes **13/13** groups with the actual model and complete
inherited chain. The earlier F1 delivery passed **12/12** groups with the
inherited chain and in a focused refresh after role-edit refinements. Core tests
passed **90 checks**; the final actual dialog/editor adapter passed **25 checks**.
Existing editor method cases passed **17/17**, editor method/pxmv syntax groups
**5/5**, F0 groups with the inherited chain **8/8**, and inherited portable/source groups
**44/44**. Three Xash full syntax groups remain explicitly blocked by the
supplied source's missing `library_suffix.h` dependency.

Logs are recorded in the patch bundle's `evidence/` and `PROVENANCE.json`. Adapter passes are not native GUI, real SDK, solver, OS commit
or live-game evidence. Existing Xash syntax groups may remain dependency-blocked
exactly as in F0; the inherited log identifies those blocks.

Native build/link, actual pxmv review/edit/save/reload, calibrated Barney drop
tests, client rendering parity, live deaths and FPS remain pending. No native
binaries are included. See [native acceptance](ACCEPTANCE.md).

The native build runner configures the real CMake project and links `server`,
`client` and `pxmv`, with PhysX enabled by default and no SDK facade. Its run
here records **BLOCKED** because CMake is unavailable; configuration and linking
were not attempted. A successful build still leaves GUI/solver/game/FPS checks
unrun. See [completion notes](COMPLETION.md) for concrete native commands.

The first ad hoc dialog extractor omitted return types and a recorder stub;
these adapter defects were corrected. An initial source assertion required a
shorter literal than the actual guarded condition; it was corrected to check
both real guards. Final compiled-method and syntax checks pass.
