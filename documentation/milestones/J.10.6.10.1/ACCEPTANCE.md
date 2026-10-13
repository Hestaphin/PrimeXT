# F1 native acceptance still to run

The portable tests establish core and adapted-method behavior. Complete the
following on the supported native toolchain before calling the F1 pxmv exit
gate observed. F2–F5 remain independent later milestones.

Use the real build runner described in [completion notes](COMPLETION.md) to
record configuration/link evidence. It does not replace the manual native
acceptance below. The current environment's build result is BLOCKED, with no
native configuration attempted.

1. Build PrimeXT server/client and pxmv with the real PhysX SDK; also build the
   supported Qt/Windows editor variant and a PhysX-disabled pxmv where supported.
   Check real toolkit text/choice widgets, owned dialog sizing, DPI and closing.
2. Load the supplied 24-bone Barney. Open Auto Generate and verify three presets,
   Simple/Standard/Detailed counts, mappings and collider overlay. Duplicate,
   rename roles, add/remove optional roles, use an override, export and reimport.
3. Test missing/ambiguous aliases, incompatible ancestry and invalid numeric
   fields. Decline Apply. Cancel by button and title-bar close during static and
   running previews. Pending main-panel text, asset and dirty state must survive.
4. Apply a reviewed candidate; change a body and D6 angular mode using the normal
   panels, Save, Reload and compare values. Use Undo Gen before later edits;
   verify it refuses to discard later applied edits. Change/reload models while
   a candidate is open; old previews and undo must not cross the boundary.
5. Test Free Rotation in both real preview and runtime: angles are free, all
   relative translations constrained. Record F0 diagnostics. Test mixed angular
   modes and existing hinge semantics. Inspect the concrete `.phys` without any
   authoring template installed in the game.
6. Exercise Replace/Merge on authored collision/interactive assets. Confirm
   original collision sections and protected hinge/prop fields survive, reused
   joints keep settings, and conflicting/disconnected graphs fail clearly.
7. Run existing collision proxy/import, props, direct ragdoll bullet queries,
   network/save/load and modular ballistic acceptance alongside these assets.

F1 candidates still use generic initial fitting/limits and unchanged contact,
solver and pose construction. Orientation drops, settling, symptom repair and
performance thresholds belong to F2–F5 and must be measured in native runs.
