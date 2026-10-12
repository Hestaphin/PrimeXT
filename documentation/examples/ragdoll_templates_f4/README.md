# Actual Barney F4 v5 review rigs

Human Standard and Free Rotation were generated through production Studio bind/filtered-vertex extraction from the matching supplied 129,104-byte MDL. It has 24 bones and rigid vertex weights. The historical supplied 23-body/22-joint v4 .phys is not converted or modified.

- Model SHA256: d326ef89e83489db85e1cc1d3c554f1b7cbc9c43c4bc7a940d76b0d75e1a393f.
- Reference .phys SHA256: 8040edc7a4b5de91279730f692a64df41167a4ee42cdcb18777b58f848777490.
- Each rig: v5, 15 bodies, 14 joints. Human: 105 pair rows, 73 allows. Four inferred flexion axes remain flagged for review.

Read the matching *_review.txt and barney_generation.json. These assets are **not native solver/game certified**. In updated pxmv load the matching model, inspect fitting/frames/pairs and run the milestone acceptance checklist. For game use a reviewed v5 sidecar must have the matching model's basename, such as models/barney.phys. Deploy updated client/GLSL with compatible server code.

The three authoring presets are the F3 Human Standard, Free Rotation and Custom definitions. Generation remains generic; no Barney branch or hard-coded bone ID is added to production code. The MDL and historical v4 input are not redistributed in this patch bundle.
