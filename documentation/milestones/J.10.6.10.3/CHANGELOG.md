# F3 changelog

Adopted one v5 runtime schema and removed v1–v4 compatibility branches. Added simulation profiles, inherited overrides, deterministic pair rows, duplicate-key and pair validation, finite/range checks and transactional parse output. Existing collision, bodygroup, hinge/part and damage metadata remain available in newly authored v5 assets.

Centralized body/joint setup and contact decisions. Corrected preview tolerance scale to the runtime scale; made contact/rest margins explicit; restored CCD after kinematic transitions; retained world/inter-entity/trigger/conveyor behavior and manual ragdoll bullet-query ownership. Added immutable group identity and indexed shape tokens with cleanup ordered after joint/actor release.

Extended template persistence/generation with physics defaults, semantic pair overrides, contact-clearance checks and preservation of protected effective tuning. Merge retains authored policy/settings and resolves new generated overrides explicitly. Reused authored pairs remain unchanged. Graph distances are computed once for generation.

Added transactional asset/template tuning dialogs, pair editing, inheritance preservation, fresh reopened snapshots, stale Apply protection and body/role rename/delete repair. Existing generation preview, axis review, Apply/Cancel/Undo and main field editing remain intact.

Updated active test fixtures for v5, retained the historical original Barney v4 reference, and added production-helper/filter/editor tests plus a safe matching-MDL bind extractor and a real-SDK drop capture target. No F4 pose repair or Xash source change is included.
