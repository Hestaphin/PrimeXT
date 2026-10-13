# Physics Asset Editor — J.10.6.10.7 (Steps 5–8)

This milestone extends the built-in Help from J.10.6.10.6; the 110-field reference remains available. **No `.phys` schema, collision/PhysX runtime, Xash3D engine, or generator algorithm is modified by this milestone.**

## Text size and scrolling

The editor, Auto Generate, Physics Settings, and the Help window now default to **Large (125%)**. The lower part of each dialog has a **Text size** selector with Normal 100%, Large 125%, and Extra large 150%. The choice is shared across Physics Asset Editor dialogs for this application session. Text and control geometry scale together from immutable original coordinates, so changing 100 → 150 → 100 does not compound rounding errors. The scale is not yet saved across application restarts.

On Linux and other Qt builds, editor controls are displayed on a scrollable surface; the viewport is constrained by the available screen size. Use the scrollbars to reach lower parameters on small displays. On MSVC Win32 builds, the editor adds window scrollbars and wheel support to reach enlarged controls. A font update does not change a saved `.phys` file. Verify actual visual behavior using a native build; portable mocks do not test operating-system font metrics or display DPI.

## Contextual parameter help

A **Parameter guidance** selector appears under Bodies and Joints in the main editor, and under the main panels in Auto Generate and Physics Settings. The selected entry shows a short explanation. **Full explanation** opens the exact entry in the original detailed Help catalog, including units, examples, and safety notes.

Clicking into or keyboard-focusing many important controls automatically selects the matching help entry: shapes, body mass, offsets, damping, hitgroups, joint limits, chain depth/branching, role mass, tuning values, CCD and self-collision. A few composite controls can also be selected manually in the guidance dropdown. Help and text scaling never Apply/Save/Generate a candidate.

## Beginner walkthrough

Click **Beginner walkthrough** in the main editor or generator; or choose **Start here → First ragdoll walkthrough** in Help. Use **Previous** and **Next step** in the Help window to move through eight stages:

1. Load the MDL and open Physics Asset Editor.
2. Select an appropriate anatomy preset and body detail.
3. Generate Preview and review every mapped body, joint and warning.
4. Inspect joint guides and collision geometry.
5. Simulate Preview if PhysX support was compiled in.
6. Fix fit/limits/mass and generate a fresh candidate.
7. Apply, then explicitly Save the `.phys` file.
8. Validate drops, physical contact and damage behavior in the real game.

This walkthrough is instructional only: it does not click buttons on the user's behalf, cannot guarantee ragdoll stability, and does not supersede in-game PhysX testing.

## Known constraints

- The J.10.6.10.5 chain-accessory, generated-joint preview selection, and high-risk warning-acknowledgment issues remain deferred to **J.10.6.10.8**.
- Contextual help bindings cover the primary editable parameters; all entries remain accessible from the selector and full Help dialog.
- Portable tests do not replace Qt/Win32 visual and accessibility validation, native build/compile, PhysX solver acceptance, or multiplayer gameplay testing.
- On very small/large DPI monitors, scrolling may be required to reach lower or right-side fields. The interface is not a redesigned responsive grid.
