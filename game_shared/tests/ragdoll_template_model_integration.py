#!/usr/bin/env python3
"""Actual Barney bytes -> production bounds, F1 preview and editor save/reload.

Only GUI/engine services are adapted; bone bounds and main authoring/persistence
methods are actual production code. This is not a native GUI/PhysX/game run.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
from ragdoll_f0_integration import function

ROOT=Path(__file__).resolve().parents[2]
TESTS=ROOT/'game_shared/tests'
FIXTURES=TESTS/'fixtures/j10_6_10_1'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model',type=Path,required=True)
    parser.add_argument('--asset',type=Path,default=FIXTURES/'barney_reference.phys')
    parser.add_argument('--ubsan',action='store_true')
    args=parser.parse_args()
    expected=json.loads((FIXTURES/'barney_source.json').read_text())
    sources={args.model.resolve():expected['model_sha256'],args.asset.resolve():expected['asset_sha256']}
    for path,digest in sources.items():
        if hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
            print('FAIL input does not match the audited supplied file:',path.name);return 2
    editor=(ROOT/'utils/pxmv/PhysicsAssetEditor.cpp').read_text()
    dialog=(ROOT/'utils/pxmv/RagdollTemplateDialog.cpp').read_text()
    studio=(ROOT/'utils/pxmv/studio_render.cpp').read_text()
    utils=(ROOT/'utils/pxmv/studio_utils.cpp').read_text()
    prefix=(FIXTURES/'template_dialog.cpp.in').read_text().split('@PRODUCTION@')[0]
    for signature in [
        'void PhysicsAssetEditor::RefreshBodyList','void PhysicsAssetEditor::RefreshJointList',
        'bool PhysicsAssetEditor::ApplyBodyFields','bool PhysicsAssetEditor::ApplyJointFields',
        'bool StudioModel::GetRagdollFitVertices','const char *mxWidget::getLabel']:
        prefix=prefix.replace(function(prefix,signature),'')
    methods=[function(editor,s) for s in [
        'static void SetFloat','static bool ReadStrictFloat','static bool ReadStrictInt',
        'static physicsasset::Vec3 ToAssetVec','PhysicsAssetEditor::PhysicsAssetEditor',
        'void PhysicsAssetEditor::BuildUI','void PhysicsAssetEditor::ShowPage','void PhysicsAssetEditor::AutoGenerate',
        'bool PhysicsAssetEditor::CollectTemplateBones','void PhysicsAssetEditor::ClearGenerationPreview',
        'void PhysicsAssetEditor::UndoGeneration','void PhysicsAssetEditor::StopSimulation',
        'void PhysicsAssetEditor::MarkDirty','void PhysicsAssetEditor::SetSimulationMode','void PhysicsAssetEditor::StartSimulation',
        'bool PhysicsAssetEditor::RebuildPreview','int PhysicsAssetEditor::FindBone',
        'void PhysicsAssetEditor::OnModelLoaded','void PhysicsAssetEditor::PopulateBones',
        'void PhysicsAssetEditor::RefreshBodyList','void PhysicsAssetEditor::RefreshJointList',
        'void PhysicsAssetEditor::RefreshBodySelectors','void PhysicsAssetEditor::ShowBody',
        'void PhysicsAssetEditor::ShowJoint','bool PhysicsAssetEditor::ApplyBodyFields',
        'bool PhysicsAssetEditor::ApplyJointFields','bool PhysicsAssetEditor::Validate',
        'bool PhysicsAssetEditor::SaveAsset','void PhysicsAssetEditor::ReloadAsset']]
    methods += [function(studio,'StudioModel::StudioModel')]
    methods += [function(utils,s) for s in ['void StudioModel::ClearToolBoneOverrides',
                'void StudioModel::ComputeSkinMatrix','bool StudioModel::GetBoneVertexBounds','bool StudioModel::GetRagdollFitVertices']]
    methods += [(ROOT/'utils/pxmv/PhysicsSettingsDialog.cpp').read_text()]
    methods += [function(dialog,'namespace\n')]
    methods += [function(dialog,s) for s in [
        'RagdollTemplateDialog::RagdollTemplateDialog(', 'void RagdollTemplateDialog::BuildUI(', 'void RagdollTemplateDialog::ShowPage(',
        'void RagdollTemplateDialog::SyncSimulationMode(', 'void RagdollTemplateDialog::Open(', 'void RagdollTemplateDialog::Cancel(',
        'void RagdollTemplateDialog::Status(', 'void RagdollTemplateDialog::ShowTemplate(',
        'void RagdollTemplateDialog::ShowRole(', 'bool RagdollTemplateDialog::ReadFields(',
        'void RagdollTemplateDialog::EditPhysicsSettings(', 'void RagdollTemplateDialog::ImportTemplate(', 'void RagdollTemplateDialog::ExportTemplate(',
        'std::string RagdollTemplateDialog::PreviewKey(', 'bool RagdollTemplateDialog::GeneratePreview(',
        'bool RagdollTemplateDialog::ApplyPreview(', 'int RagdollTemplateDialog::handleEvent(']]
    main=(FIXTURES/'template_model_roundtrip.cpp.in').read_text()
    source=prefix+'\n'+main.replace('@PRODUCTION@','\n\n'.join(methods))
    with tempfile.TemporaryDirectory(prefix='f1_actual_model_') as tmp:
        tmp=Path(tmp);unit=tmp/'model.cpp';unit.write_text(source);binary=tmp/'model'
        includes=['utils/pxmv','external/mxtk/include','external/mxtk/include/mx','client/render','game_shared','engine','common','public','game_shared/tests']
        flags=['-std=c++17','-O0','-g','-fno-exceptions','-ffunction-sections','-fdata-sections','-D_LINUX','-DLINUX',
               '-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp','-D_snprintf=snprintf','-D_vsnprintf=vsnprintf',
               '-Wno-write-strings','-Wno-invalid offsetof'.replace(' ','-')]
        if args.ubsan:flags+=['-fsanitize=undefined','-fno-sanitize-recover=all']
        libraries=['vector.cpp','matrix.cpp','mathlib.cpp','stringlib.cpp','bone_setup.cpp','ikcontext.cpp','procbones.cpp','jigglebones.cpp','physics_asset_format.cpp','ragdoll_template.cpp']
        command=[os.environ.get('CXX','c++'),*flags,*['-I'+str(ROOT/p) for p in includes],str(unit),
                 *[str(ROOT/'game_shared'/p) for p in libraries],'-Wl,--gc-sections','-o',str(binary)]
        build=subprocess.run(command,cwd=ROOT,text=True,capture_output=True)
        if build.returncode:print((build.stdout+build.stderr)[-10000:]);return build.returncode
        model=tmp/'barney.mdl';asset=tmp/'barney.phys'
        shutil.copyfile(args.model,model);shutil.copyfile(TESTS/'fixtures/j10_6_10_3/editor_authored_v5.phys',asset)
        run=subprocess.run([str(binary),str(model)],cwd=tmp,text=True,capture_output=True)
        print(run.stdout+run.stderr)
        for path,digest in sources.items():
            if hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
                print('FAIL original input changed:',path.name);return 1
        return run.returncode

if __name__=='__main__':raise SystemExit(main())
