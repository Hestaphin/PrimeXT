#!/usr/bin/env python3
"""Source-wiring regressions only. Does not exercise PhysX or the GUI."""
from pathlib import Path
root = Path(__file__).resolve().parents[2]
ragdoll = (root / 'server/physx/physics_asset.cpp').read_text()
prop = (root / 'server/physx/physx_impl.cpp').read_text()
editor = (root / 'utils/pxmv/PhysicsAssetEditor.cpp').read_text()
hinge = (root / 'server/physx/physics_hinge_frame.h').read_text()
assert 'RigidUniformScale(' in ragdoll and 'RigidUniformScale(' in prop
assert 'm_rigidScale' in ragdoll and 'def.shape.radius * scale' in ragdoll
assert 'BuildReferencePoses()' in ragdoll
assert 'ChildHingeFrame(def, scale)' in ragdoll
assert 'ChildHingeFrame(def, uniformScale)' in prop
assert 'joint.hingeAnchor.x * uniformScale' in hinge
assert 'PxRevoluteJoint *hinge = physicsasset::CreateRevoluteJoint' in editor
assert 'std::vector<PxJoint *> joints' in editor
assert 'if(m_selectedJoint >= 0 && !ApplyJointFields())' in editor
assert 'if(m_selectedBody >= 0 && requested != m_selectedBody && !ApplyBodyFields())' in editor
assert 'ReadStrictFloat(m_mass, mass)' in editor
assert 'ReadStrictInt(m_hitGroup, hitGroup)' in editor
assert 'ScaleAuthoredAsset(resized, factor)' in editor
assert 'IDC_PHYS_SCALE_ASSET' in editor
assert 'Replace the current authored bodies and joints?' in editor
print('Scale + editor + hinge source-wiring checks PASS')
