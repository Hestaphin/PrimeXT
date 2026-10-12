# J.10.6.3 — death promotion and acceptance

Apply the incremental patch to J.10.6.2, or the cumulative patch to the original
PrimeXT/Xash ZIP sources. Choose one route and rebuild. Earlier cumulative
patches must not be stacked.

Ordinary non-gib monster deaths now promote existing partial physics to full
physics. The automatic-death cvar and gib policy remain in force. Existing
actors/joints and saved entity/root relationships are retained. Repeated full
activation reasserts entity hull suspension because the legacy death transition
can set MOVETYPE_TOSS. Animation anchors that reject a bullet impulse retain it
in the bounded, short-lived pending queue; promotion consumes that queue once.
Dynamic bodies that accepted the hit do not queue it again.

The outstanding shape-selection finding is also repaired: collect eligible
entries, deduplicate exclusive shape identities, find the global minimum, then
reject another distinct shape within 0.075 units of that minimum. Reverse exit
queries use only the selected shape. Enumeration order and selected exit failure
cannot substitute another shape. Roots exceeding the query bound fail closed.

Run available acceptance with Python 3 and C/C++17 compilers:

```sh
python3 PrimeXT-master/game_shared/tests/verify_j10_6_3.py --ubsan
```

`--ubsan` applies undefined-behavior checks to the new death/query and manager
production-method fixtures. The driver includes the earlier pose, inventory,
BSP and source checks, every supplied portable C++ test, the native validator
linked from its declared sources, three valid sample packs, an invalid pack,
the 16 headless editor/discovery tests, and supported Linux syntax checks.
The editor tests can also use a separately built validator:

```sh
PRIMEXT_WEAPON_VALIDATOR=/absolute/path/primext-weapon-validate \
  python3 -m unittest discover -s PrimeXT-master/tools/weapon_editor/tests -v
```

The method fixtures compile unchanged production methods with explicit entity,
presentation, actor and geometry-query adapters. The new box ray adapter is
analytic and does not run PhysX. The 256-shot stress and reentrant damage callback
tests run the real manager/flight/impact methods with deterministic engine
services. They do not measure live frame cost, network behavior or solver stability.

Full acceptance still requires the intended Linux and Windows target builds,
PhysX 4.1.2#6 from the pinned vcpkg manifest, actual overlap/contact scenes,
native saves and the planned live maps. This environment lacks CMake/Ninja/vcpkg,
PhysX/fmt/graphics prerequisites and Xash's libbacktrace submodule. Only the native
weapon validator was linked here. See the bundled results report for actual
passes and outstanding checks. Portable passes do not certify those unavailable
routes. Player/special-NPC death policy and hidden touching BSP material layers
remain outside this repair.
