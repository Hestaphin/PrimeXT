# Physics Asset Editor - integrated help reference (J.10.6.10.6)

This document mirrors the catalog compiled into pxmv. Open **Help / ?** in the main Physics Asset Editor, Ragdoll Template Generator, or Physics Settings. The Help window is available without a network connection.


## Start here

### What is a physics body?

**Brief:** An invisible solid that represents part of a character.

**Units/values:** concept

The visible MDL is for rendering; its skeleton is for positioning and animation. A .phys body describes a simpler solid (capsule, sphere or box) linked to a skeleton bone. Each body gets mass, collision size and a pose. Bodies are not themselves a physics engine.

**Example:** A forearm bone may use a narrow capsule; its elbow joint connects it to an upper-arm body.

### What is .phys?

**Brief:** A saved recipe for collision bodies and their joints.

**Units/values:** asset file

The .phys asset is text data describing bodies, joints, collision geometry, filtering and solver settings. Saving .phys does not simulate anything. When loaded for preview or gameplay, its definitions are used to create PhysX objects. This editor uses .phys version 5; old files may not include newer properties.

**Example:** Edit barney.phys alongside barney.mdl; Save writes the asset to disk.

### What is PhysX?

**Brief:** PhysX performs movement, collision and constraint calculations.

**Units/values:** physics engine

PhysX takes the bodies and joints from the .phys asset and advances them in time under gravity, collisions, forces and joint limits. It is separate from the file and the editor. A badly aligned joint can behave badly even when PhysX works correctly.

**Example:** An off-center bullet impulse makes a PhysX arm rotate and pull on its shoulder.

### Bone vs body vs joint

**Brief:** Bones move visible geometry; physics bodies collide; joints connect bodies.

**Units/values:** concept

A Studio skeleton bone is not automatically a solid. Give selected bones collision bodies and connect adjacent ones with constraints. The simulation updates the physics bodies, and their poses are transferred back to the skeleton for drawing. Do not put one body on every attachment or helper bone unless intentional.

**Example:** Head bone -> sphere body; neck joint -> connects head body to torso.

### First ragdoll walkthrough

**Brief:** Generate, review, simulate, adjust, apply and save.

**Units/values:** steps

(1) Load a Studio MDL. (2) Open Auto Generate. (3) Choose an anatomy preset and detail. (4) Generate Preview; inspect mappings and warnings. (5) Simulate Preview if PhysX is enabled. (6) Tune shapes and joints; inspect the axes. (7) Apply to Asset, then Save the .phys. (8) Test the result in the real game. Preview Apply and Save are separate actions.

**Example:** For Barney, start with Human Standard; try Heavy and Loose only after checking the baseline.

### Simulation preview vs game

**Brief:** They share constructors, not the entire environment.

**Units/values:** concept

The editor creates a dedicated PhysX scene, gravity and a flat ground plane. A normal preview runs fixed 0.01-second steps. The game creates bodies through shared PhysX helpers but uses map geometry, game gravity, weapon impulses and network behavior. An editor drop test cannot establish multiplayer or game-map correctness.

**Example:** A stable drop onto the preview plane may still clip through complex map geometry.

### Saving and safety

**Brief:** Apply changes before Save; keep backups before major generation.

**Units/values:** steps

Editing numeric fields does not mean changes are saved. Apply Body or Apply Joint commits fields to the working asset. Generate Preview is temporary; Apply to Asset copies it into the editor and Undo Gen can undo one generation. Save writes .phys, Reload discards unsaved edits. An unverified generation must not be treated as production-ready.

**Example:** Keep a copy of the original .phys and compare in-game behavior after saving.


## Bodies

### Body list / Add / Remove / Apply

**Brief:** Choose, create, delete or commit an authored physics body.

**Units/values:** editor actions

Select an item to edit its properties. Apply validates and commits values to the working asset; Save persists them. Removing a body may affect attached joints. The editor stops simulation when applying edits, because live PhysX actors need rebuilding.

**Example:** Add a forearm body, set its bone and shape, press Apply, then Save.

### Body name

**Brief:** A unique name used by joints and collision pair rules.

**Units/values:** text

Use short stable names. Renaming an existing body updates joint references and explicit collision pairs in the editor. Two different parts should not share the same name.

**Example:** left_forearm

### Bone

**Brief:** Binds the body to a pre-existing skeleton bone.

**Units/values:** Studio bone

Select the Studio bone whose reference pose places this collision solid correctly. A bone is NOT added to the model. Wrong bone mapping makes the physics mesh follow the wrong body part. Review attachments and helper bones separately.

**Example:** Bip01 L Forearm

### Shape

**Brief:** Choose the invisible collision primitive.

**Units/values:** capsule / box / sphere

Capsules are useful for limbs; spheres for heads and rounded pivots; boxes for broad solid parts. These are simple PhysX shapes, not the exact rendered skin. Prefer a few well-fitted shapes over dozens of tiny ones.

**Example:** Choose Capsule for a forearm, Sphere for a head.

### Offset XYZ

**Brief:** Position the body relative to its chosen bone.

**Units/values:** model units

Offsets are local to the selected Studio bone. Change one component at a time while inspecting the shape in the viewport. Do not attempt to fix incorrect joint orientation by moving a body far away from its bone.

**Example:** Move Z slightly to center a chest capsule over the chest.

### Angles XYZ

**Brief:** Rotate the body's local collision shape.

**Units/values:** degrees

Euler pitch/yaw/roll angles orient the primitive relative to the chosen bone. They do not change the skeleton bind pose or an articulated joint's frame. Inspect all three viewport axes after modifying a capsule.

**Example:** Rotate a capsule to align its long direction with a thigh.

### Dimensions XYZ

**Brief:** Size the selected shape; meanings depend on shape.

**Units/values:** model units

Box: half-extents XYZ (total width is twice each value). Sphere: first value is radius. Capsule: first value is radius and second is cylinder half-height; remaining field is unused. Radii and box half-extents must be positive; capsule half-height may be zero. Oversized primitives overlap other limbs and create jitter.

**Example:** A box half-extent of 4 gives a total length of 8 along that axis.

### Mass

**Brief:** Controls body inertia and response to forces.

**Units/values:** positive mass value

Every dynamic physics body needs positive mass. The sum and distribution matter more than an isolated number. Extreme adjacent mass ratios can make joints unstable. Do not use mass to compensate for a badly sized collision shape.

**Example:** Use more mass for torso than fingertips; review the neighbor-mass warning.

### Linear damp

**Brief:** Resists sliding/translation of this body.

**Units/values:** per second

Higher linear damping slows translational motion faster. It does not fix penetrations or broken joints. Values must be finite and non-negative. Physics Settings can supply inherited defaults; an explicitly authored body override takes priority.

**Example:** Slightly increase if the entire ragdoll drifts for too long.

### Angular damp

**Brief:** Resists the spinning of an individual body.

**Units/values:** per second

Higher angular damping reduces angular velocity more quickly. Very high values make a ragdoll sluggish; too little can prolong rotation. If a head spins wildly, inspect joint frames, body sizes and mass distribution before just increasing damping.

**Example:** Increase moderately only after confirming head/neck joint limits.

### Hitgroup

**Brief:** Gameplay zone reported by a hit on this body.

**Units/values:** integer 0..255

0 is generic. Standard Half-Life hitgroups: 1 head; 2 chest; 3 stomach; 4 left arm; 5 right arm; 6 left leg; 7 right leg. Values 8..255 are available to mods. This is not a collision-filter setting.

**Example:** Set a head physics body to 1 and a left forearm to 4.

### Make Part

**Brief:** Marks a body for interactive articulated-prop authoring.

**Units/values:** action

This action helps configure the selected body as an interactive part. Ordinary ragdolls do not need a named interactive part or motor. This is distinct from the Make Hinge action for joints.

**Example:** Use only for a controllable prop door or machine limb.

### Part name

**Brief:** Stable identity for a controllable articulated body.

**Units/values:** text

Required when Drive mode is not None. Keep it consistent with your gameplay controls and entity setup; it is metadata, not an automatic skeleton bone name.

**Example:** robot_arm

### Drive mode

**Brief:** How an interactive part is controlled.

**Units/values:** None / Animated / Driven / Free

None is the normal ragdoll behavior. Animated follows authored animation; Driven uses a motor toward a target; Free runs under physics. The preview may not reproduce full gameplay control logic. For Driven mode, strength must be positive.

**Example:** Use None for a corpse; consider Driven for an actively controlled machine arm.

### Drive strength

**Brief:** Controls strength of a driven part's motor.

**Units/values:** nonnegative

Higher values make a driven body try harder to track its target. It is not a mass value, and it is meaningful only for interactive driving modes. Positive strength is required for Driven mode.

**Example:** Start low and increase while testing the joint under load.

### Drive damping

**Brief:** Damps oscillation in an interactive drive.

**Units/values:** nonnegative

It opposes rapid motor motion. Increasing it can reduce overshoot, but too much makes target motion slow. This does not replace body angular damping or fix wrong joint axes.

**Example:** If a driven arm overshoots, increase drive damping in small steps.

### Drive max force

**Brief:** Caps interactive motor effort; zero uses server default.

**Units/values:** motor torque limit

Prevents a driven part from applying unlimited torque. On this asset type the stored driveMaxForce field is used as a motor torque limit; the interface's historical label says force. A zero value selects the server tuning default.

**Example:** Limit the motor on a small hinge to avoid violently pushing other objects.


## Joints

### Joint list / Add / Remove / Apply

**Brief:** Create and edit connections between physics bodies.

**Units/values:** editor actions

A joint defines how two bodies stay connected and how they may rotate. Select a joint before changing limits or inspecting joint guides. Apply commits values to the working asset; Save writes the file.

**Example:** Connect left_forearm (child) to left_upperarm (parent).

### Joint name

**Brief:** Unique readable label for a joint.

**Units/values:** text

A stable descriptive joint name helps diagnose warnings and identify it in the viewport. It does not define its physical position; parent/child and anchors do.

**Example:** left_elbow

### Parent body

**Brief:** Body on the upstream side of the joint.

**Units/values:** body name

Choose the intended chain parent (upper arm, torso, upper leg). Parent and child refer to physics body names, not necessarily adjacent skeleton nodes. Incorrect selection can bind distant body parts.

**Example:** Elbow parent = left_upperarm.

### Child body

**Brief:** Body downstream of the joint.

**Units/values:** body name

Choose a distinct body that should articulate against the parent. Joint anchor and frame coordinates are defined relative to the child Studio bone for the newer frame types.

**Example:** Elbow child = left_forearm.

### Swing Y / Swing Z

**Brief:** Constrain the two sideways angular directions of a D6 joint.

**Units/values:** degrees

These are D6 swing angular ranges. Smaller values restrict bending, larger values permit more. Values must be positive when edited. They apply only if the corresponding angular motion is Limited. Wrong frame orientation makes otherwise reasonable limits bend in the wrong anatomical direction.

**Example:** Start narrow for a neck; inspect the colored swing guides.

### Twist low / Twist high

**Brief:** Lower and upper bounds of D6 rotation about twist axis.

**Units/values:** degrees

Twist low must be strictly less than twist high. The authored D6 frame determines which physical direction is twist. If the frame is wrong, negative/positive limits can appear reversed. The values do nothing if Twist motion is Free or Locked.

**Example:** Use -20 and 20 as a cautious starting range, then inspect.

### Joint type

**Brief:** Use a multi-axis ragdoll joint or one-axis hinge.

**Units/values:** D6 / Hinge

D6 supports limited, locked or free rotation around multiple angular axes; translation is locked in this ragdoll implementation. Hinge/revolute joints are for doors, mechanisms and other one-axis articulation. Changing types changes which limits matter.

**Example:** Shoulder: D6. Door: Hinge.

### Make Hinge

**Brief:** Configure selected connection as an interactive hinge.

**Units/values:** action

Use this to author mechanisms with one main rotation axis. Validate its child-local anchor, nonzero axis, angular travel and optional damage/break behavior. Hinge-specific release/break damage is not supported for D6.

**Example:** Use for a swinging gate or prop lever.

### Hinge min / max

**Brief:** Angular travel allowed to a hinge.

**Units/values:** degrees -180..180

The editor requires -180 <= min < max <= 180. These are meaningful for Hinge, not D6. If the hinge rotates backward, check the child-local hinge axis and its sign before inverting limits.

**Example:** A door may use min 0, max 90 degrees.

### Hinge anchor XYZ

**Brief:** Hinge pivot position in the child bone's local space.

**Units/values:** child-bone model units

This is NOT a world-space location. Place the pivot at the physical hinge pin. Bad anchors can cause separation or jerking. Selecting the joint and enabling Joint Guides helps compare parent and child locations.

**Example:** A robot elbow pivot should lie where upper and lower arm meet.

### Hinge axis XYZ

**Brief:** Direction of permitted hinge rotation in child-bone space.

**Units/values:** unit direction (normalized)

The vector must have a nonzero length; the runtime normalizes it. A hinge rotates around its local X axis after the frame is oriented to this vector. Wrong axis makes a door twist or an elbow fold sideways.

**Example:** Try (1,0,0) when the chosen bone X axis follows the intended hinge pin.

### Release damage

**Brief:** Accumulated damage needed to release an interactive hinge motor.

**Units/values:** gameplay damage

Zero disables the threshold. This is gameplay behavior associated with a hinge and cannot be used with a D6 ragdoll joint. It is distinct from PhysX constraint forces.

**Example:** Set only for a motorized machine part meant to be freed by damage.

### Break damage

**Brief:** Accumulated damage required to detach an interactive hinge.

**Units/values:** gameplay damage

Zero disables damage-triggered detachment. Hinge only; positive values are not accepted on D6 joints. Do not use on ordinary skeleton elbows unless detachability is intended.

**Example:** A breakable prop hinge can be released after enough damage.

### Break impulse

**Brief:** Single-impact threshold for breaking a joint.

**Units/values:** impulse magnitude

Zero disables single-impact break logic. This is not a body mass setting. Tune in a physical test; too small makes limbs or props detach from ordinary impacts.

**Example:** Use a nonzero threshold only on intentionally breakable connections.

### T / Y / Z motions

**Brief:** Set D6 twist, swing-Y and swing-Z freedom.

**Units/values:** Locked / Limited / Free

Locked prevents relative rotation on the selected axis, Limited enforces the authored angle range, and Free has no authored angular limit. The field labels T/Y/Z refer to twist and the two swings, not translation; D6 translation stays locked. Hinge uses its own limit.

**Example:** Use Limited for anatomically constrained elbows and shoulders.

### D6 anchor XYZ

**Brief:** Joint-frame pivot in the child reference bone.

**Units/values:** child-bone model units

The parent frame is derived from this child-local bind-pose pivot. Small placement errors can pull bodies apart when simulation begins. Inspect joint axes and anchor gap before saving.

**Example:** Set the shoulder anchor at the actual shoulder center.

### D6 pitch / yaw / roll

**Brief:** Orientation of the D6 constraint frame.

**Units/values:** degrees

Euler angles are relative to the child reference bone and determine what PhysX treats as twist and swing. This is independent from body shape Angles XYZ. Inspect the colored guide arcs before changing limits; frame alignment matters more than large numeric ranges.

**Example:** Rotate the frame so elbow flexion corresponds to the intended axis.

### Joint guides

**Brief:** Draw axes and angular-limit guides for the selected joint.

**Units/values:** view toggle

RGB shows the parent joint frame; cyan marks the child X axis; yellow indicates pivot separation. Twist and swing arcs are only visual aids, not a full coupled solver-cone prediction. They do not automatically validate stable physics.

**Example:** Select a joint and switch Joint guides On.


## Generator

### Template preset

**Brief:** Select a standard or customized anatomy recipe.

**Units/values:** preset

Built-ins include Human Standard, Free Rotation, Custom, Human Heavy, Human Loose, Quadruped and Creature. Built-ins are read-only; Duplicate to a custom copy before editing parameters. Anatomical inference is still approximate.

**Example:** Start with Human Standard for Barney or Quadruped for four-legged skeletons.

### Import / Save As / Duplicate / Reset

**Brief:** Manage editable .ragtemplate recipes.

**Units/values:** actions

Import loads a template file, Save As writes a copy, Duplicate creates an editable custom template, and Reset returns to the preset. Importing does not alter the currently applied .phys until a candidate is generated and applied.

**Example:** Duplicate Creature, edit roles, then Save As to preserve your recipe.

### Template ID / name

**Brief:** Stable machine ID and friendly preset name.

**Units/values:** text

Use an ID without ambiguous naming collisions; the friendly name appears in selection. Changing the template name does not rename your MDL or body bone names.

**Example:** id = creature_tail; name = Creature with Tail.

### Anatomy

**Brief:** Declares the general skeleton family.

**Units/values:** text

This helps document intended template anatomy and grouping. It does not magically infer unknown bones without suitable aliases or explicit mappings.

**Example:** human, quadruped, creature or custom.

### Description

**Brief:** Human-readable purpose of the template.

**Units/values:** text

Document expected skeleton naming, intended shape detail and any unusual constraint conventions. This text is saved with the template, not as a PhysX runtime force.

**Example:** A loose-jointed quadruped with a four-link tail.

### Mass budget

**Brief:** Target combined mass for generated ragdoll bodies.

**Units/values:** positive total mass

The generator distributes the budget among selected roles using mass weights and regions. Large differences between connected bodies can make joints unstable. More mass is not necessarily more believable motion.

**Example:** Start near the chosen preset budget; review mass-ratio warnings.

### Linear / angular damp

**Brief:** Simulation defaults inherited by generated bodies.

**Units/values:** per second

Same physical meaning as body linear and angular damping. Edit these at template level for consistent behavior; override individual bodies only when needed. Does not correct bad joint frames.

**Example:** Human Loose uses less angular damping to allow longer rotations.

### Role ID

**Brief:** Logical part to generate and map to a Studio bone.

**Units/values:** text

The role is a template concept, not a skeleton bone itself. Each role uses aliases or a selected override to find a suitable bone. Duplicate roles or accidental mapping can create inappropriate physics bodies.

**Example:** left_forearm, pelvis, tail_root.

### Parent role

**Brief:** Role whose generated body is linked to this role.

**Units/values:** role ID

Use a stable upstream role for body connectivity. It is a role ID, not a bone index. Wrong parentage can cause long or crisscrossed constraints.

**Example:** left_forearm parent role = left_upperarm.

### Aliases (;)

**Brief:** Candidate skeleton bone names for automatic mapping.

**Units/values:** semicolon-separated names

The generator tries these names when matching the model. Be specific where possible; broad aliases may choose helper or weapon attachment bones. If inference is uncertain, use a Bone override and inspect the report.

**Example:** Bip01 L Forearm;left_forearm;forearm_l

### Bone override / picker

**Brief:** Force a role to a particular existing model bone.

**Units/values:** Studio bone

Use for skeletons with nonstandard names or ambiguous aliases. The picker lists actual loaded model bones. It does not create new bones. Recheck after switching models.

**Example:** Map head role to Bone_Head_01.

### Mass weight

**Brief:** Share of the template mass budget assigned to this role.

**Units/values:** positive relative value

Weights help distribute mass; they are not absolute kilograms. A high torso weight compared with tiny fingers is reasonable, but extreme adjacent mass ratios can create instability. Review warnings after generating.

**Example:** Give pelvis or torso more weight than tail tip.

### Role hitgroup

**Brief:** Damage zone automatically assigned to this role's body.

**Units/values:** integer 0..255

Same hitgroup meaning as the main editor. Standard zones 0 generic, 1 head, 2 chest, 3 stomach, 4/5 arms, 6/7 legs. You can edit the saved result afterward.

**Example:** Head role -> 1; right thigh -> 7.

### Required role

**Brief:** Require this role to resolve to an actual bone.

**Units/values:** checkbox

A required unresolved role can prevent candidate generation. Use optional for anatomy that is absent on some models. Do not mark an essential torso role optional just to hide wrong mappings.

**Example:** Jaw or extra wings may be optional; pelvis is often required.

### Twist / Y / Z motions

**Brief:** Default angular freedom for joints generated from this role.

**Units/values:** Locked / Limited / Free

Locked stops relative rotation, Limited uses specified angular ranges, and Free leaves it unrestricted. D6 translations are locked. Orient the joint frame before relying on ranges to enforce anatomical motion.

**Example:** Limit elbow and knee travel rather than leaving all axes Free.

### Swing Y / Swing Z

**Brief:** Angular limits assigned to this role's generated joint.

**Units/values:** degrees

Smaller values restrict joint bending; larger values loosen it. Only meaningful for Limited motions. These are not rotation axis directions, so inspect generated frames.

**Example:** Keep a creature neck more restricted than a loose tail link.

### Twist low / Twist high

**Brief:** Twist interval for a generated D6 joint.

**Units/values:** degrees

Low must precede High. Positive and negative directions depend on the inferred or explicit joint frame. A wrong axis can make values look reversed in simulation.

**Example:** Begin with -20/20; tune after a drop test.

### Shape

**Brief:** Preferred collision primitive for the generated role.

**Units/values:** box / sphere / capsule

Generator estimates dimensions using model geometry and bone spans. A shape choice is only a starting point; review clipping and limb overlap. Capsule is a common choice for long limbs.

**Example:** Capsule for a shin; box for bulky armor.

### Minimum detail

**Brief:** Minimum generation detail at which this role appears.

**Units/values:** Simple / Standard / Detailed

A role with high detail requirement may be skipped in Simple generation. Choose Simple to reduce body counts and solver load. Detailed adds additional authored parts when the skeleton has them.

**Example:** Make a tiny tail-tip segment Detailed-only.

### Chain edges

**Brief:** How many descendant bone connections to expand automatically.

**Units/values:** integer 0..16

Zero disables expansion. A positive depth follows child skeleton bones below the mapped role and creates extra bodies and joints. This uses existing bones; it never adds skeleton bones. The J.10.6.10.5 algorithm may include unintended attachment or helper bones; inspect every extra mapping and stop before Apply if one is suspicious.

**Example:** Depth 4 can add four tail segments after the starting tail role.

### Expand branches

**Brief:** Allow automatic chain expansion to follow branching children.

**Units/values:** checkbox

Disabled is safer for a straight tail: unexpected branching is rejected. Enabled permits multiple child paths and may add extra bodies. It does not identify accessories reliably by itself; visually verify branch mappings.

**Example:** Enable only when an intentionally branched tentacle or wing rig needs it.

### Frame mode

**Brief:** Method used to orient a generated joint frame.

**Units/values:** Bone / Segment / Flexion / Explicit

Bone uses bone axes, Segment uses its segment direction, Flexion infers a positive anatomical bending axis, and Explicit uses your supplied anchor/angles. Inferred axes are review-only guidance, not guaranteed correct PhysX stability.

**Example:** Use Explicit after confirming a misaligned elbow frame in the guide.

### Flexion sign

**Brief:** Flips the inferred positive flexion direction.

**Units/values:** +1 / -1

Only meaningful when using inferred flexion frames. Adjust when a knee or elbow bends backward in preview. This is not an overall rotation or stiffness setting.

**Example:** Change from +1 to -1 if anatomical flexion is reversed.

### Mass region

**Brief:** Group roles for region-based mass distribution.

**Units/values:** text

A named group helps distribute mass to torso, limbs and specialized anatomy. The region label is metadata for generation, not a PhysX scene region. Keep names consistent in your template.

**Example:** torso, legs, tail.

### Exclude (;) sections

**Brief:** Skip selected model sections during geometry extraction.

**Units/values:** semicolon-separated strings

Use to exclude decorative mesh sections when fitting collision shapes. It does not automatically guarantee descendant bones are omitted from chain expansion. The exclusion list is distinct from bone mapping and manual role choice.

**Example:** Omit weapon accessory mesh sections from limb fitting.

### Axyz / PYR

**Brief:** Explicit frame anchor and orientation for a generated role.

**Units/values:** model units / degrees

Axyz is child-bone-local anchor XYZ; PYR is pitch/yaw/roll in degrees. Used for Explicit frame mode. Wrong local-space assumptions often cause anchor gaps or backwards motion.

**Example:** Place explicit elbow pivot at the joint, then use guides to inspect.

### Body detail

**Brief:** Overall number and complexity of generated bodies.

**Units/values:** Simple / Standard / Detailed

Higher detail enables roles requiring that level. It increases simulation complexity and opportunities for overlap. Start Simple when diagnosing instability, then increase only if more articulated parts are needed.

**Example:** Use Standard for a humanoid; Simple as a performance fallback.

### Replace / Merge

**Brief:** Choose whether to replace ragdoll bodies or preserve existing edits.

**Units/values:** generation mode

Replace rebuilds the ragdoll but preserves prop and collision geometry; Merge keeps existing authored bodies/joints where possible. Inspect generated report counts for added, removed and reused structures before Apply.

**Example:** Use Merge when retaining carefully authored accessory physics parts.

### Radius / length

**Brief:** Fallback body radius relative to its bone segment length.

**Units/values:** fraction 0.01..0.45

Controls fitted thickness when vertex samples are scarce. Actual geometry samples may dominate shape fitting. Too wide colliders overlap adjacent parts; too narrow miss physical contacts.

**Example:** Reduce for a very thin tail; inspect the generated collision overlay.

### End trim

**Brief:** Shortens fitted segments near their endpoints.

**Units/values:** fraction 0..0.3

Helps reduce neighboring collision overlap near joints. Very high trim leaves physical gaps, while too little can cause penetration or oscillation. Adjust after previewing body lengths.

**Example:** Slight trim can help adjacent knee capsules avoid overlap.

### Mass-ratio warn

**Brief:** Warning threshold for mass differences across connected bodies.

**Units/values:** ratio 1..1000

The generator reports adjacent body ratios above this threshold. Raising it hides warnings but does not improve simulation stability. Prefer redistributing mass or merging small links.

**Example:** If torso is 20 times heavier than a tiny neck body, investigate the joint.

### Generate Preview

**Brief:** Build a candidate without modifying the saved .phys.

**Units/values:** action

Review mappings, inferred-axis warnings, expanded-chain warnings, overlaps and mass ratios. A candidate may be structurally valid yet physically unstable. Preview is not equivalent to Save or an in-game drop test.

**Example:** Inspect every newly added body for accessories and duplicate roles.

### Simulate Preview

**Brief:** Drop the candidate into the editor PhysX scene.

**Units/values:** action

Requires a pxmv binary built with PhysX support. Uses a simplified ground plane, not the actual map. Watch for spinning heads, exploding constraints, floor penetration and unexpected body collisions. Stop or reset before authoring major changes.

**Example:** Check whether knees bend in the intended direction under gravity.

### Apply / Cancel / review

**Brief:** Explicitly accept or discard the generated candidate.

**Units/values:** actions

Apply copies the reviewed candidate into the editable asset (not disk); Save then writes .phys. Cancel leaves the original authored asset unchanged. The review checkbox acknowledges inferred axes and chain mapping; it is not proof of a physically stable result.

**Example:** If a tail expands into weapon_scope, Cancel and correct your role setup.


## Tuning

### Scope / target

**Brief:** Choose whether settings affect whole asset or a specific object.

**Units/values:** profile / body / joint

Profile values are defaults. Body and joint overrides replace selected profile parameters. In the generator dialog the target is a role, not an existing runtime bone. Understand the effective inheritance before comparing two assets.

**Example:** Set profile damping first; override only a problematic head body.

### Use profile settings

**Brief:** Clear individual overrides and inherit profile defaults.

**Units/values:** checkbox

When enabled on a selected body or joint, its tunings are reset to inherited values. This is not a numeric zero. Editing the asset profile later then affects the inherited parameter.

**Example:** Use inheritance to keep all limb solver iteration counts consistent.

### Linear damping

**Brief:** Resists straight-line velocity.

**Units/values:** per second

Affects translation of dynamic bodies. Higher damping decays motion more rapidly; extremely high values can make objects move unnaturally. Supported profile range 0..100.

**Example:** Raise slightly if a dropped body slides too long.

### Angular damping

**Brief:** Resists spin and rotation.

**Units/values:** per second

Acts on each body, not on joint angle limits. Use after correcting pivot locations and limits. Supported profile range 0..100.

**Example:** Do not mask a freely spinning head by setting damping to 100.

### Position solver passes

**Brief:** Position-constraint solver iteration count.

**Units/values:** integer 1..255

Increasing it may improve joint and contact stability under load but costs CPU time. More passes cannot fix an impossible chain or profoundly overlapping shapes. Defaults to 16 in current format.

**Example:** Try a modest increase if properly built joints stretch.

### Velocity solver passes

**Brief:** Velocity-constraint solver iteration count.

**Units/values:** integer 0..255

Helps resolve velocity effects in contacts and joints. Higher counts can improve certain impacts at a performance cost. Defaults to 4. Diagnose bad geometry first.

**Example:** Compare 4 and 8 only after a repeatable impact test.

### Max linear velocity

**Brief:** Clamps the maximum translational speed.

**Units/values:** model units/s

Prevents extreme runaway speeds. A very low cap makes objects appear to move through syrup; a very high cap may increase tunneling risk. Profile must be positive.

**Example:** Lower only when violent impacts cause unrealistic velocities.

### Max angular velocity

**Brief:** Clamps the maximum spin speed.

**Units/values:** radians/s

The units are radians per second, not degrees. A cap can reduce runaway spinning but does not repair a misoriented head joint. Positive profile value required.

**Example:** Test spinning limbs with a temporary cap, then inspect their joints.

### Max depenetration velocity

**Brief:** Caps speed used to resolve overlapping collision shapes.

**Units/values:** model units/s

Higher values separate interpenetrating bodies faster, possibly creating abrupt pops. Lower values can leave overlaps unresolved. The best fix is fitting shapes and pivots correctly.

**Example:** If limbs pop on activation, inspect geometry before adjusting.

### Sleep energy threshold

**Brief:** Controls when PhysX considers an almost-stationary body asleep.

**Units/values:** PhysX sleep threshold

Higher values may allow earlier sleeping and save CPU; too high can freeze still-moving ragdolls. It is a solver threshold, not a timer in seconds. Must be non-negative.

**Example:** Lower if a still-moving arm freezes abruptly.

### Angular resistance

**Brief:** Adds resistance to D6 angular motion when enabled.

**Units/values:** joint motor parameter

Uses PhysX D6 drives for angular resistance; it is distinct from body angular damping. Zero disables this authored resistance. Hinge resistance is handled by its interactive motor, so per-hinge D6 resistance is not supported.

**Example:** Start low on loose neck joints and test under gravity.

### Maximum torque

**Brief:** Upper cap for authored angular drive torque.

**Units/values:** torque limit

Constrains joint resistance/motor effort. If resistance is positive, the global maximum torque must be positive. Extreme torque can make joints feel rigid or unstable; this is not body mass.

**Example:** Raise only if a correctly oriented D6 drive is too weak.

### Joint projection

**Brief:** Allow PhysX to correct excessive joint-frame separation.

**Units/values:** Off / On

Projection can reduce visible dislocation after solver difficulties, but is not a substitute for correct local anchors and shape placement. Only joint/profile scopes offer this setting. Compare with and without projection during diagnostics.

**Example:** Enable cautiously after aligning a troublesome shoulder joint.

### Projection distance

**Brief:** Position error tolerance before projection corrects a joint.

**Units/values:** model units

Smaller tolerances intervene sooner; overly aggressive projection can produce unnatural corrections. Must be positive. Projection must be enabled to matter.

**Example:** Use a small tolerance for tightly connected ragdoll segments.

### Projection angle

**Brief:** Angular error tolerance for joint projection.

**Units/values:** degrees

This differs from ordinary joint travel limits; it controls when projection corrects excessive angular violation. Valid profile range is above zero and below 180 degrees.

**Example:** Reduce cautiously if a shoulder joint drifts far beyond its limit.

### Contact margin

**Brief:** Contact offset used to begin collision contact generation.

**Units/values:** model units

Shapes may generate contact before their visible surfaces touch. Increasing margins can reduce tunneling but also cause early apparent collisions. Must be positive and greater than rest margin.

**Example:** If oversized margins make limbs hover, inspect the actual shape sizes first.

### Rest margin

**Brief:** Rest offset defining desired collision separation.

**Units/values:** model units

Must be non-negative and strictly smaller than contact margin. Tune carefully since incorrect offsets can create jitter or unnatural gaps. These are PhysX contact geometry settings, not joint anchor offsets.

**Example:** Keep near the preset default unless you have a reproducible contact problem.

### Continuous collision detection

**Brief:** Check fast-moving bodies against tunneling.

**Units/values:** checkbox

CCD can help when objects move quickly enough to cross thin barriers between simulation steps. It costs computation and cannot resolve every geometric edge case. The default profile enables CCD.

**Example:** Use for fast-thrown ragdoll parts that penetrate thin props.

### Self collision

**Brief:** Controls collision between bodies of the same asset.

**Units/values:** Off / Selective / Custom

Off disables configured self contacts. Selective enables generated/explicit pairs according to policy. Custom relies on explicit rules. Incorrect pairs may cause body jitter or allow a limb to pass through the chest; inspect pair selection in preview.

**Example:** Start with limited self-collision; only allow body pairs that need it.

### Collision pair A / B

**Brief:** Select two distinct bodies for an explicit contact rule.

**Units/values:** body choices

Pair rules refer to body names; invalid or identical selections are refused. Review adjacent bodies: enabling shoulder-to-upper-arm collision can cause constant overlapping contacts.

**Example:** Select left_hand and torso before choosing Allow or Deny.

### Allow / Deny / Default pair

**Brief:** Override, prohibit or remove a selected body-pair rule.

**Units/values:** actions

Allow enables specified self-contacts, Deny disables them, and Default removes the explicit override. Whether a pair collides also depends on the overall self-collision mode. Unlisted pairs are denied under Custom.

**Example:** Deny two neighboring capsules that intersect in the bind pose.

### Apply settings / Cancel

**Brief:** Commit or abandon staged profile, body and pair changes.

**Units/values:** actions

Apply validates finite numbers, solver bounds, margins, and collision rules before committing to the calling editor. It does not automatically save the .phys file; use Save in the main editor. Cancel discards the staged dialog.

**Example:** Apply physics settings, test, then Save .phys.


## Tools

### Asset resize / Scale .phys

**Brief:** Rescale authored geometry for a re-exported MDL.

**Units/values:** positive scale factor

Use when the source model itself was re-exported at a different size. This changes authored geometry but does not change runtime entity scale. Nonpositive and invalid scales are rejected. Make a backup first.

**Example:** If an MDL was re-exported twice as large, scale its .phys to match.

### Import _cp

**Brief:** Import legacy collision proxy geometry.

**Units/values:** action

Loads a sibling _cp.mdl as authored collision geometry where valid. It is not the same as generating physics bodies for a ragdoll. The importer checks legacy Studio mesh data and can fail on unsupported or corrupt proxies.

**Example:** Import static prop collision from chair_cp.mdl.

### Clear Col

**Brief:** Remove authored collision geometry sections.

**Units/values:** action

Does not delete the ragdoll body/joint authoring tables. Use carefully: props may fall back to different collision behavior depending on runtime rules. Save only when the removal is intended.

**Example:** Clear obsolete imported _cp geometry after reviewing the replacement.

### Simulate / Reset

**Brief:** Start, stop or reset the local PhysX preview.

**Units/values:** actions

The preview has gravity, a flat floor and 0.01-second physics steps when compiled with PhysX. The game has different environment and forces. Reset/Stop does not rewrite the stored .phys.

**Example:** Run a drop test, observe joint stretching, then Reset to edit.

### Diagnostics / Save F0 log

**Brief:** Record and export physics preview diagnostics.

**Units/values:** actions

Diagnostics can help diagnose unstable bodies, contacts or solver behavior. Logs are supporting evidence, not proof that a joint is anatomically correct. Preserve the exact scenario and settings alongside a log.

**Example:** Save a log for a reproducible head-spin case.

### Undo Gen / Reload / Save

**Brief:** Undo the last generation, discard edits, or write the file.

**Units/values:** actions

Undo Gen reverses a supported generation transaction. Reload reads the file again and discards unsaved changes; Save writes the asset. A model replacement or unrelated manual edits can invalidate generation undo assumptions.

**Example:** Save a backup before Reload or a major regeneration.


## Troubleshooting

### Head spins uncontrollably

**Brief:** Inspect head frame, neck limits, shape fit and mass ratios first.

**Units/values:** diagnosis

Select the neck joint and show its frame guides. Check the child-local anchor, whether twist/swing axes match the skeleton, whether motions are unintentionally Free, and whether a very light head is connected to a heavy torso. Only afterward adjust damping, torque or angular speed limits.

**Example:** Run repeatable ground-drop tests before and after changing one value.

### Ragdoll jitters or explodes

**Brief:** Look for overlapping collision bodies and impossible constraints.

**Units/values:** diagnosis

Check pairs of adjacent bodies for intersection, unsupported self-collision pairs, different parent/child anchors, nonuniform scale, extreme mass ratios and high motor strength. Solver passes help only after the underlying geometry is corrected.

**Example:** Disable nearby self-collision pairs, validate anchors, then retest.

### Limbs bend backward

**Brief:** Correct joint frames, flexion sign and angular-motion limits.

**Units/values:** diagnosis

Moving the entire body or increasing mass usually will not fix a backwards knee. Inspect the D6 frame axes and whether the template inferred the flexion sign incorrectly. Try an explicit frame or opposite flexion sign, regenerate and preview.

**Example:** A knee opening forward may need flexion sign flipped.

### Missing or extra generated body

**Brief:** Audit bone aliases, chain depth and excluded sections.

**Units/values:** diagnosis

J.10.6.10.5 automatic chain expansion can convert a decorative attachment descendant into a physics body even when the bone has no useful vertices. Treat every chain result as a candidate to inspect. Reduce chain depth, tighten mappings, or Cancel and author the role explicitly. Full automatic accessory exclusion is deferred to the bug-fix milestone.

**Example:** If weapon_scope appears as a tail segment, do not Apply that candidate.

### Body penetrates the floor

**Brief:** Compare shape sizes, offsets, CCD and contact settings.

**Units/values:** diagnosis

Verify the body is centered on the correct bone and that the collider is large enough. Test at moderate velocities on the flat editor ground. CCD can help at high speeds but cannot fix a collider placed entirely off the mesh.

**Example:** Check a hand sphere offset before increasing collision margins.

### Preview works but game fails

**Brief:** Re-test the actual game physics environment and asset load.

**Units/values:** diagnosis

Confirm pxmv and server load the same saved .phys file, model scale is uniform and positive, and the native build includes PhysX. Test the map collision, death activation, impulses, replication and save/load. The preview's flat plane does not replace these tests.

**Example:** Use a known static floor map and compare the generated asset checksum.
