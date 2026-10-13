#!/usr/bin/env python3
"""Execute actual F1 dialog/editor methods with explicit GUI/engine adapters."""
from pathlib import Path
from physics_asset_ui_test_adapter import adapt
import argparse
import os
import shutil
import subprocess
import tempfile
from ragdoll_f0_integration import function

ROOT = Path(__file__).resolve().parents[2]
TESTS = ROOT / 'game_shared/tests'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan',action='store_true')
    args=parser.parse_args()
    editor=(ROOT/'utils/pxmv/PhysicsAssetEditor.cpp').read_text()
    dialog=(ROOT/'utils/pxmv/RagdollTemplateDialog.cpp').read_text()
    studio=(ROOT/'utils/pxmv/studio_render.cpp').read_text()
    utils=(ROOT/'utils/pxmv/studio_utils.cpp').read_text()
    methods=[function(editor,s) for s in [
        'static void SetFloat','static bool ReadStrictFloat','static physicsasset::Vec3 ToAssetVec','PhysicsAssetEditor::PhysicsAssetEditor','void PhysicsAssetEditor::BuildUI','void PhysicsAssetEditor::ShowPage',
        'void PhysicsAssetEditor::AutoGenerate','bool PhysicsAssetEditor::CollectTemplateBones',
        'void PhysicsAssetEditor::ClearGenerationPreview','void PhysicsAssetEditor::UndoGeneration',
        'void PhysicsAssetEditor::StopSimulation','void PhysicsAssetEditor::MarkDirty',
        'void PhysicsAssetEditor::SetSimulationMode','void PhysicsAssetEditor::StartSimulation','bool PhysicsAssetEditor::RebuildPreview']]
    methods += [function(studio,'StudioModel::StudioModel'),function(utils,'void StudioModel::ClearToolBoneOverrides'),function(utils,'void StudioModel::CaptureToolAnimationPose'),function(utils,'void StudioModel::ReleaseToolAnimationPose')]
    methods += [(ROOT/'utils/pxmv/PhysicsSettingsDialog.cpp').read_text()]
    methods += [function(dialog,'namespace\n')]
    methods += [function(dialog,s) for s in [
        'RagdollTemplateDialog::RagdollTemplateDialog(', 'RagdollTemplateDialog::~RagdollTemplateDialog(', 'void RagdollTemplateDialog::BuildUI(', 'void RagdollTemplateDialog::ShowPage(',
        'void RagdollTemplateDialog::SyncSimulationMode(', 'void RagdollTemplateDialog::Open(', 'void RagdollTemplateDialog::Cancel(',
        'void RagdollTemplateDialog::Status(', 'void RagdollTemplateDialog::ShowTemplate(',
        'void RagdollTemplateDialog::ShowRole(', 'bool RagdollTemplateDialog::ReadFields(',
        'void RagdollTemplateDialog::EditPhysicsSettings(', 'void RagdollTemplateDialog::ImportTemplate(', 'void RagdollTemplateDialog::ExportTemplate(',
        'std::string RagdollTemplateDialog::PreviewKey(', 'bool RagdollTemplateDialog::GeneratePreview(',
        'bool RagdollTemplateDialog::ApplyPreview(', 'int RagdollTemplateDialog::handleEvent(']]
    production='\n\n'.join(methods)
    source=(TESTS/'fixtures/j10_6_10_1/template_dialog.cpp.in').read_text().replace('@PRODUCTION@',production)
    with tempfile.TemporaryDirectory(prefix='ragtemplate_dialog_') as tmp:
        tmp=Path(tmp);unit=tmp/'dialog.cpp';unit.write_text(adapt(source));binary=tmp/'dialog'
        includes=['utils/pxmv','external/mxtk/include','external/mxtk/include/mx','client/render','game_shared','engine','common','public','game_shared/tests']
        flags=['-std=c++17','-O0','-g','-fno-exceptions','-ffunction-sections','-fdata-sections','-D_LINUX','-DLINUX',
               '-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp','-D_snprintf=snprintf','-D_vsnprintf=vsnprintf',
               '-Wno-write-strings','-Wno-invalid offsetof'.replace(' ','-')]
        if args.ubsan: flags += ['-fsanitize=undefined','-fno-sanitize-recover=all']
        libraries=['vector.cpp','matrix.cpp','mathlib.cpp','stringlib.cpp','bone_setup.cpp','ikcontext.cpp','procbones.cpp','jigglebones.cpp','physics_asset_format.cpp','ragdoll_template.cpp']
        command=[os.environ.get('CXX','c++'),*flags,*['-I'+str(ROOT/p) for p in includes],str(unit),*[str(ROOT/'game_shared'/p) for p in libraries],'-Wl,--gc-sections','-o',str(binary)]
        compile=subprocess.run(command,text=True,capture_output=True,cwd=ROOT)
        if compile.returncode:
            print((compile.stdout+compile.stderr)[-14000:]);return compile.returncode
        fixture=tmp/'barney.phys';shutil.copyfile(TESTS/'fixtures/j10_6_10_3/editor_authored_v5.phys',fixture)
        run=subprocess.run([str(binary),str(fixture)],text=True,capture_output=True,cwd=tmp)
        print(run.stdout+run.stderr);return run.returncode

if __name__=='__main__': raise SystemExit(main())
