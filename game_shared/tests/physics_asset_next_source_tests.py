#!/usr/bin/env python3
"""Source-wiring regressions only. Does not exercise PhysX or the GUI."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
ragdoll = (root / 'server/physx/physics_asset.cpp').read_text()
prop = (root / 'server/physx/physx_impl.cpp').read_text()
editor = (root / 'utils/pxmv/PhysicsAssetEditor.cpp').read_text()
hinge = (root / 'server/physx/physics_hinge_frame.h').read_text()
assert 'RigidUniformScale(' in ragdoll and 'RigidUniformScale(' in prop
construction=(root/'server/physx/physics_asset_construction.h').read_text()
assert 'm_rigidScale' in ragdoll and 'body.shape.radius*scale' in construction
assert 'BuildReferencePoses()' in ragdoll
assert 'physicsasset::CreateAssetJoint(' in ragdoll
assert 'physicsasset::CreateAssetJoint(' in prop and 'ChildHingeFrame(def,scale)' in construction
assert 'joint.hingeAnchor.x * uniformScale' in hinge
assert 'physicsasset::CreateAssetJoint(' in editor and 'CreateRevoluteJoint(' in construction
assert 'std::vector<PxJoint *> joints' in editor
assert 'if(m_selectedJoint >= 0 && !ApplyJointFields())' in editor
assert 'if(m_selectedBody >= 0 && requested != m_selectedBody && !ApplyBodyFields())' in editor
assert 'ReadStrictFloat(m_mass, mass)' in editor
assert 'ReadStrictInt(m_hitGroup, hitGroup)' in editor
assert 'ScaleAuthoredAsset(resized, factor)' in editor
assert 'IDC_PHYS_SCALE_ASSET' in editor
dialog = (root / 'utils/pxmv/RagdollTemplateDialog.cpp').read_text()
assert 'Apply this reviewed candidate to the editor?' in dialog
assert 'GeneratePreview()' in dialog and 'm_generation.Apply(m_editor.m_asset' in dialog
assert 'Collision and interactive sections are preserved.' in dialog
print('Scale + editor + hinge source-wiring checks PASS')
