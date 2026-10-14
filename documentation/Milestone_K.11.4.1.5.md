# K.11.4.1.5 — rendering budgets

Step 5 of the six-step FPS improvement plan, based on K.11.4.1.4. Client-only
rendering changes; the Step 4 physics rates, scheduling and gameplay stay in place.

## Presets and controls

`r_performance_showmenu` opens the existing in-game performance window.
`r_physics_showmenu` remains an alias. The window retains the local physics
selector and adds separate Low, Medium and Legacy rendering controls. Rendering
controls also work online without sending a command to change server physics.
Physics and rendering choices are independent, so select both for a combined
Low/Medium setup. Defaults are Medium rendering and the existing 60 Hz physics.

| Setting | Medium (default) | Low | Legacy |
| --- | --- | --- | --- |
| `r_monitor_hz` | 30 | 15 | 0: every displayed frame |
| `r_monitor_size` | 512 | 256 | 1024 |
| `r_reflection_scale` | 0.5 | 0.25 | 1.0 |
| `r_reflection_cache` | 1 | 1 | 0 |
| `r_shadow_budget` | 16 | 8 | -1: unlimited |
| `r_shadow_subview_budget` | 8 | 0 | -1: unlimited |
| `r_shadow_cache` | 1 | 1 | 0 |
| `r_shadow_particles` | 0 | 0 | 1 |
| `r_grass_shadows` | 1 | 0 | 1 |

The new cvars are archived. Positive monitor rates clamp to 15–30 Hz; zero or
negative values select display-driven refresh. Monitor size is the longest
texture dimension, clamped to 128–1024. Reflection scale clamps to 0.25–1.0.
Shadow budgets count actual scene passes: a spotlight costs one, an omni light
costs six cube faces, and a sun costs four cascades. Budgets clamp to 0–512;
negative values mean unlimited. Nonfinite settings resolve to bounded defaults.

The main-view and secondary-view quotas are separate and reset once per full
display frame. All nested monitor/reflection/portal views share the secondary
quota. Medium therefore admits at most 24 fresh shadow passes per display frame;
Low admits at most eight. Cached groups require no new pass. Zero quota permits
no fresh groups, while valid main-view cached groups can remain visible. Use
the existing `r_shadows 0` to disable shadows completely.

The stock Options → Video menu belongs to the mainui submodule omitted from the
supplied Xash ZIP. This release uses PrimeXT's ImGui window and the existing
Controls binding. It does not modify that separate stock menu module.

## Monitor refresh and texture ownership

Monitor images refresh on simulated client-time buckets, independent of display
call count. Missed buckets trigger one current refresh rather than a catch-up
burst. Stable monitor-entity phases spread warmed refreshes across frames.
Newly visible cameras render immediately. Camera/model identity, source/FOV,
viewport, HDR format, view flags and lifecycle participate in cache freshness.
Ordinary camera movement is sampled at the selected video cadence.

Each retained image pins its existing subview texture. The general allocator
cannot lend that texture to another view while the cached feed references it.
Refresh writes back into the owned slot; the framebuffer-copy fallback also
keeps this ownership. Both paths mark the image current for surface validation.

Up to 32 retained images share the existing 64-texture pool, leaving half for
current-view rendering. Excess cache candidates retain the ordinary render path.
Unseen entries release ownership after two complete unseen frames. Missing or
stale cameras clear the surface and release the cache immediately. Render/driver
allocation failures do not publish a valid image or poison a retry.

Monitor aspect ratio is preserved under the size cap. Same-camera screen clones
now require matching source texture dimensions, preventing mismatched-aspect
screens from inheriting another screen's image.

## Reflections, portals and sky

Planar reflections and puddles use the selected resolution scale; their camera,
clip plane, FOV and projection remain view-correct. Medium's half-size dimensions
use roughly one quarter of the original pixels; Low's quarter-size dimensions
use roughly one sixteenth. Geometry/pass CPU cost does not scale proportionally
with pixel count, so this is not an FPS-gain claim.

Active reflections render each displayed frame. The image cache is conservative:
it reuses an unchanged paused view only when exact view/scene/light/style/fog/
sky-resource inputs match. Studio/unknown geometry and remote screen/portal/movie
feeds keep it fresh. Engine particles and dynamic effects do not receive an
arbitrary reflection refresh cap. Moving the view invalidates immediately.

Portals retain their full-resolution, display-driven path; their distinct source
transforms are not cached as interchangeable images. Existing borrowed skybox
and static cubemap texture reuse remains. Three-dimensional sky scenes continue
to render for each view, preserving camera-dependent parallax and the previous
sky/water fixes. Main world, weapon view and character presentation stay per
display frame. The preset does not set `fps_max`; use the existing engine frame
limit separately when choosing a 60/30 FPS target.

## Shadow budgets and conservative reuse

With a finite quota, the sun is considered first, followed by nearest lights
with a stable order for equal distance. Culling/support checks run before quota
admission. An entire cube/cascade group must fit before any of its faces draws.
A rejected light uses the existing neutral depth texture, so it stays lit
without a stale, foreign or partly updated shadow. A finite quota intentionally
reduces which lights cast shadows; raise it or choose Legacy for full coverage.

Static shadow reuse applies to unchanged world/brush-only depth inputs. Exact
world/model/texture identity, brush state and transforms, shader/settings state,
light identity/projection and the sun view/split parameters control freshness.
Studio casters, engine particle shadows and screen/portal/mirror/movie worlds
are conservatively redrawn. Animated materials, water/conveyors and wind grass
retain their time dependency. Zero reuse in these scenes is expected.

Reusing a spot/cube shadow consumes its ordinary per-frame texture slot. Write
epochs plus slot/texture identity detect reordering or overwritten pool contents.
Later lights cannot overwrite an earlier cached image still used by the view.
Sun reuse requires valid dedicated FBOs; framebuffer-copy sun fallback remains
fresh. Texture matrices and their shader arrays are restored with cached images.
Secondary views always draw within their quota instead of borrowing main-view
shadow images with different culling or sun projection.

Medium/Low stop drawing engine particles into shadow maps; particles still draw
normally in the world. Low also disables grass shadow casting. These are explicit
quality choices; Legacy restores both. Map/world-context reset, video/common
texture initialization, backward time/frame count and world replacement clear
cached ownership. Resolution/HDR changes recycle the subview pool between frames,
avoiding unbounded accumulation of old-size resources.

## Profiling and validation

The compatible `PERF_K11411` output adds count rows `monitor_cache_hits`,
`reflection_cache_hits`, `shadow_cache_hits` and `shadow_budget_skips`. Existing
screen/mirror/portal/shadow pass counters now omit cache hits. Profiling remains
off by default. Compare actual pass counts and p95/p99 frame intervals at the
same resolution, camera, map and preset before comparing different resolutions.

```sh
python3 client/render/tests/verify_render_budget_k11415.py --retained
```

The runner uses production renderer/view/cache/allocator methods, real matrix/
texture types, recording GL/engine services and ASan/UBSan at O0/O2. It covers
24 float-clock refresh schedules, ownership pinning, framebuffer-copy reuse,
camera/FOV changes, resizing, allocation recovery, unseen/map/time reset, paused
reflection invalidation, portal freshness and screen-aspect clones. Shadow tests
cover complete-group quotas, cached/foreign texture ownership, pool reordering,
moving/unknown inputs, dynamic feeds, secondary limits and Legacy fallback.
Changed renderer objects compile against native Linux project headers. The
recording menu checks all preset writes and online physics isolation. Retained
physics, sleep/cache, rendering, transition and ballistics suites run too.

These checks do not run the live GPU/menu, link a full native game/PhysX/ImGui
build, certify Windows builds, or measure in-game FPS. LeakSanitizer is disabled
because process introspection is unavailable; ASan/UBSan remain active for the
stated adapters. Native scene comparisons are still required on the target
machine, especially for mirror/water/portal output and budgeted shadow coverage.
