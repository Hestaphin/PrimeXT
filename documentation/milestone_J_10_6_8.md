# J.10.6.8 — Physics Asset Editor window and preview

This release retains J.10.6.7 and repairs the separate `pxmv` editor. J.10.6.7
did not modify the Physics Asset Editor or mxToolKit window implementation.

## Findings and repairs

| Trigger | J.10.6.6 behavior | J.10.6.8 behavior |
| --- | --- | --- |
| Open Physics Asset Editor on Windows | mxWindow overwrites Dialog style with WS_CHILD when a parent exists. The 730 × 705 editor is embedded in a viewer initially sized 640 × 540, clipping controls and overlapping the GL viewport. | Dialog/ModalDialog retain their window style. The main viewer is the owner, while ordinary GL/control panels remain children. Client dimensions account for title and borders. |
| Open the editor on Qt | Dialog style is ignored and a main-viewer-owned window is reparented into its central widget. | Explicit Qt::Dialog flag; ownership retained without embedding. |
| Edit body Offset/Angles/Dimensions | Row names, axis labels and inputs overlap; some joint captions are too long for their label slots. | Separate row-label and numeric columns; axis/dimension headers above fields; shorter joint captions. All 98 authored control rectangles fit without intersection. |
| Inspect collision/body shapes within a solid model | DrawOverlay enables depth testing, hiding internal shapes. It also inherits blend and alpha-test state. | Authoring wireframes draw through the mesh, disable blending/alpha test and depth writes, and restore caller state. The overlay remains visible only while the editor is open. |
| View rotating or source-offset models | Root collision vertices omit the model transform; bone-bound collision already uses its bone transform. | Root collision follows the model transform; bone-bound collision continues through its current bone pose exactly once. |
| A body references an unavailable bone | Joint lines can use an uninitialized body matrix. | Only bodies with a valid displayed transform contribute joint endpoints. |
| Pause animation, then simulate or open/close editor | Physics advances, but the viewport does not repaint while paused. | Running simulation and overlay visibility changes repaint without advancing paused animation. Editor actions also request a repaint. |

## Verification

`python3 game_shared/tests/verify_j10_6_8.py --ubsan`

The editor runner compiles unmodified production window constructors, BuildUI,
overlay drawing, model transform accessors and the viewport Idle case. It uses
real mx/PhysicsAssetEditor/StudioModel declarations, Studio and asset types,
and project math. Platform widgets, clocks, simulation services and GL state
are explicit test adapters.

On the exact attached J.10.6.6 baseline, 11 of 17 cases fail. The repaired
source passes all 17, including controls for ordinary child panels, existing
bone-bound transforms, hidden/empty overlays and unpaused animation. Three
complete edited pxmv translation units pass Linux syntax checks with PhysX
disabled and GL declarations adapted from the project's real client API.
Inherited J.10.6.7 acceptance is included in the verifier.

These checks do not establish native Windows/Qt painting, font/DPI behavior,
PhysX-enabled linking or a live GPU preview. The user's exact editor screenshot
was not supplied. The listed source defects are demonstrated; any different
visual symptom still needs a screenshot and native reproduction.

## Build and acceptance

For an existing J.10.6.7 installation, apply the editor-only incremental patch,
rebuild mxToolKit and `pxmv`, and replace `pxmv.exe` plus any separately deployed
mxToolKit library used by that build. Static mxToolKit builds must relink pxmv.

For J.10.6.6, the corresponding incremental patch also includes J.10.6.7:
rebuild/deploy the matching client, server, engine/ref_gl and pxmv. The first
New Game crash repair is in `ref_gl.dll`.

1. Start pxmv at the usual viewer size, load a model and open Physics Asset Editor.
2. Confirm the editor is a separate movable owned window; inspect bottom Save,
   Simulate, Scale and status controls. Resize the viewer and move either window.
3. Select bodies/shapes and confirm labels, values and highlighting are readable.
4. View a solid/textured model and inspect internal physics wireframes. Check
   rotating/source-offset and bone-bound collision if those assets are available.
5. Pause animation, start simulation, then stop/reset. Open/close the editor while
   paused; verify the overlay appears/disappears and animation remains paused.
6. Repeat on the actual Windows display scale and with the PhysX-enabled build.

The editor changes do not diagnose the remaining in-game black sky/water report.
J.10.6.7's runtime diagnostic and its stated uncertainty remain applicable.
