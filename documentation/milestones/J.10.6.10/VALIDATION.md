# J.10.6.10 F0 validation

The available automated checks pass. F0's live exit gate is still pending:
the matching Barney model, reference physics asset, recordings, native PhysX
SDK/toolchain and game installation were not available. This milestone provides
diagnostics and a source-backed defect register; it does not certify a repaired
ragdoll or an in-game FPS baseline.

## Executed checks

| Check | Result | What it establishes |
| --- | --- | --- |
| Original ZIP hashes and J.10.6.9.9 cumulative reconstruction | Pass | The supplied latest baseline was reconstructed once from the original archives; executable ZIP modes were preserved. |
| Baseline `verify_j10_6.py` | 41/41 pass | Existing portable weapon, collision, ragdoll and source guards. |
| Baseline additional portable physics executables | 25/25 pass after two corrected invocations | Parser, scale, traces, collision ownership, sidecar, hinges, pose cache and restore helpers. Initial and corrected logs are retained. |
| Final `verify_j10_6_10.py --ubsan` | 8/8 pass | Real matrix/math code, read-only diagnostics, finite values, joint-anchor transforms, broken/world joints, scale/pose links, bounded hierarchy walks, peaks, capture/export and unchanged physics scope. |
| `verify_j10_6_10.py --ubsan --inherited` | 9/9 available groups pass | The complete inherited portable/source/method/syntax chain; its nested J.10.6.3 suite reports 42/42. This run preceded the final frame/backlog columns and angular-peak index refinement; the focused suite was refreshed afterward. |
| Refreshed editor acceptance `verify_j10_6_8.py --ubsan --skip-inherited` | 4/4 groups pass, including 17/17 method cases | Actual dialog/editor/idle methods through explicit adapters; 101 controls fit. Three complete pxmv translation units pass syntax with GL declarations adapted and PhysX disabled. |
| Changed full client/server translation units | 5/5 syntax checks pass | `gl_cvars.cpp`, `gl_studio_init.cpp`, `gl_studio_draw.cpp`, `game.cpp`, `physx_impl.cpp`; server PhysX is disabled for this check. |

Group counts are nested and overlap; they must not be summed into a fabricated
total. UBSan means undefined-behavior instrumentation was enabled for the
portable C++ executables and method fixtures. It does not imply solver, GUI or
GPU coverage.

## Diagnostic cases

- True world joint-anchor separation is tested with rotated local frames,
  including an anchor offset where actor-center distance is misleading.
- Broken joints skip invalid actor reads; world anchors, missing bodies/joints,
  free/locked/limited angles, kinematic counts and non-finite velocities are
  checked through a getter-only PhysX facade. That facade is not the SDK.
- Real Studio transforms are tested at scales 0.5, 1 and 2.5 with moving driven
  bones, unsimulated ancestors/descendants, an injected 100-unit child spike,
  a separate driven-target discrepancy and invalid/cyclic hierarchy input.
- Window tests retain transient speed, anchor, invalid-input, frame/backlog,
  position and angular peaks. An angular-only peak updates its associated bone.
- Actual runtime recorder methods produce no diagnostic getters/logging while
  disabled and do not alter actor state. pxmv methods are compiled both with
  and without the preview feature macro.
- Actual export methods preserve original asset bytes and an existing log,
  select unused log names with exclusive creation, enforce 4096 recorded rows,
  expose dropped-row counts, retain disabled captures and report create failure.

## Environment and blocked checks

The environment provides GCC 13 and Python 3. It has no CMake/Ninja, MSVC/Windows
SDK, PhysX SDK, Qt/OpenGL development stack or installed game. Full Windows x86/
x64 Debug/Release builds, Linux native target links, actual GUI controls and
rendering, PhysX settling, death handoff and FPS comparison remain pending in
[ACCEPTANCE.md](ACCEPTANCE.md).

Three inherited Xash complete-source syntax checks remain blocked because the
supplied engine dependency `library_suffix.h` is absent. Their log explicitly
marks BLOCKED; the inherited suite's available-group pass is not a full engine
build. Xash production files are unchanged in this milestone. A direct pxmv
syntax attempt also lacked `GL/gl.h`; the successful adapted declaration checks
are labeled separately.

Initial baseline failures from dropped executable ZIP modes, a missing TMPDIR
and a missing sample-asset command argument were corrected without changing
physics production code. They are harness/environment problems, not evidence
that Barney's reported symptoms passed or failed here.

Delivered evidence contains the baseline logs, final focused and inherited
logs, editor/syntax logs, environment information and clean patch-route checks.
Compilation/test durations in those files are not live FPS measurements.
