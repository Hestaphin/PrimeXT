#!/usr/bin/env python3
"""F1 source-wiring checks only; no GUI, SDK, scene or solver execution."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def read(name): return (ROOT/name).read_text()
core=read('game_shared/ragdoll_template.cpp')
editor=read('utils/pxmv/PhysicsAssetEditor.cpp')
dialog=read('utils/pxmv/RagdollTemplateDialog.cpp')
runtime=read('server/physx/physics_asset.cpp')
motion=read('server/physx/physics_asset_motion.h')
cmake=read('utils/pxmv/CMakeLists.txt')
assert 'ragdoll_template.cpp' in cmake and 'RagdollTemplateDialog.cpp' in cmake
assert 'm_templateDialog->Open()' in editor and 'void PhysicsAssetEditor::UndoGeneration()' in editor
assert 'ActivePreviewAsset()' in editor and 'm_generation = ragtemplate::Transaction{}' in editor
assert 'if(!m_generationPreview && m_selectedJoint' in editor
assert 'if(!m_generationPreview && m_selectedBody' in editor
construction=read('server/physx/physics_asset_construction.h')
for source in [editor,runtime]: assert 'physicsasset::CreatePrimitiveBody(' in source and 'physicsasset::CreateAssetJoint(' in source
assert 'ConfigureD6AngularMotion(joint,def)' in construction
for axis in ['eX','eY','eZ']: assert 'setMotion(PxD6Axis::'+axis+',PxD6Motion::eLOCKED)' in construction
assert 'eX' not in motion and 'eY' not in motion and 'eZ' not in motion
assert 'm_generation.Apply(m_editor.m_asset' in dialog and 'm_previewKey!=PreviewKey()' in dialog
assert 'mxEvent::Close' in dialog and 'm_editor.ClearGenerationPreview();m_editor.m_generation.Cancel()' in dialog
assert 'event.event = mxEvent::Close;' in read('external/mxtk/src/win32/mx.cpp')
assert 'event.event = mxEvent::Close;' in read('external/mxtk/src/qt/mxWindow_p.h')
assert 'static thread_local QByteArray label' in read('external/mxtk/src/qt/mxWidget.cpp')
assert 'if(key=="parent")' in core and 'explicit bone' in core and 'ambiguous' in core
assert 'current.collisions.size()' in core and 'JointKind::Hinge' in core
assert 'barney' not in core.lower() # supplied model is a test fixture, not a production special case
print('PASS F1 parser/editor/runtime/toolkit wiring; translation locks; model-generic production code (source checks only)')
