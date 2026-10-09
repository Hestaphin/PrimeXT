from pathlib import Path
r=Path(__file__).resolve().parents[2]
impl=(r/'server/physx/physx_impl.cpp').read_text()
asset=(r/'server/physx/physics_asset.cpp').read_text()
client=(r/'client/render/gl_studio_draw.cpp').read_text()
anim=(r/'server/animating.cpp').read_text()
assert 'ScaleBonePivot' in impl and 'StudioBoneWorldTransform' in impl
assert 'CaptureRagdollState' in impl and 'RestoreRagdollState' in impl
assert 'SaveArticulatedBodies' in asset and 'RestoreArticulatedBodies' in asset
assert 'CaptureRagdollState(this,m_ragdollSaved' in anim
assert 'RestoreRagdollState(' in anim and 'this,m_ragdollSaved,m_ragdollBodiesSaved' in anim
assert client.count('m_bRagdollPose || m_pModelInstance->m_bRagdollExitBlend')>=2
print('I.9.5 code wiring PASS')
