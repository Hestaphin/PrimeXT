#!/usr/bin/env python3
"""Execute production bind fitting and authoring with synthetic Studio inputs.
GUI/engine services are adapted. No native GUI, SDK, solver or actual Barney.
"""
from pathlib import Path
from physics_asset_ui_test_adapter import adapt
import argparse, os, subprocess, tempfile
from ragdoll_f0_integration import function
ROOT=Path(__file__).resolve().parents[2]
TESTS=ROOT/'game_shared/tests'
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--ubsan',action='store_true');args=parser.parse_args()
    editor=(ROOT/'utils/pxmv/PhysicsAssetEditor.cpp').read_text();dialog=(ROOT/'utils/pxmv/RagdollTemplateDialog.cpp').read_text()
    utils=(ROOT/'utils/pxmv/studio_utils.cpp').read_text();studio=(ROOT/'utils/pxmv/studio_render.cpp').read_text()
    prefix=(TESTS/'fixtures/j10_6_10_1/template_dialog.cpp.in').read_text().split('@PRODUCTION@')[0]
    for signature in ['void PhysicsAssetEditor::RefreshBodyList','void PhysicsAssetEditor::RefreshJointList','bool PhysicsAssetEditor::SaveAsset','bool PhysicsAssetEditor::ApplyBodyFields','bool PhysicsAssetEditor::ApplyJointFields','bool StudioModel::GetRagdollFitVertices']:
        prefix=prefix.replace(function(prefix,signature),'')
    methods=[function(editor,s) for s in [
        'static void SetFloat','static bool ReadStrictFloat','static bool ReadStrictInt','static physicsasset::Vec3 ToAssetVec',
        'PhysicsAssetEditor::PhysicsAssetEditor','void PhysicsAssetEditor::BuildUI','void PhysicsAssetEditor::ShowPage','void PhysicsAssetEditor::AutoGenerate',
        'bool PhysicsAssetEditor::CollectTemplateBones','void PhysicsAssetEditor::ClearGenerationPreview','void PhysicsAssetEditor::UndoGeneration',
        'void PhysicsAssetEditor::StopSimulation','void PhysicsAssetEditor::MarkDirty','void PhysicsAssetEditor::SetSimulationMode','void PhysicsAssetEditor::StartSimulation','bool PhysicsAssetEditor::RebuildPreview',
        'int PhysicsAssetEditor::FindBone','void PhysicsAssetEditor::OnModelLoaded','void PhysicsAssetEditor::PopulateBones',
        'void PhysicsAssetEditor::RefreshBodyList','void PhysicsAssetEditor::RefreshJointList','void PhysicsAssetEditor::RefreshBodySelectors',
        'void PhysicsAssetEditor::ShowBody','void PhysicsAssetEditor::ShowJoint','bool PhysicsAssetEditor::ApplyBodyFields',
        'bool PhysicsAssetEditor::ApplyJointFields','bool PhysicsAssetEditor::Validate','bool PhysicsAssetEditor::SaveAsset','void PhysicsAssetEditor::ReloadAsset']]
    methods+=[function(studio,'StudioModel::StudioModel'),function(utils,'void StudioModel::ClearToolBoneOverrides'),function(utils,'void StudioModel::CaptureToolAnimationPose'),function(utils,'void StudioModel::ReleaseToolAnimationPose'),function(utils,'bool StudioModel::GetRagdollFitVertices')]
    methods+=[(ROOT/'utils/pxmv/PhysicsSettingsDialog.cpp').read_text()]
    methods+=[function(dialog,'namespace\n')]
    methods+=[function(dialog,s) for s in ['RagdollTemplateDialog::RagdollTemplateDialog(', 'RagdollTemplateDialog::~RagdollTemplateDialog(', 'void RagdollTemplateDialog::BuildUI(', 'void RagdollTemplateDialog::ShowPage(',
        'void RagdollTemplateDialog::SyncSimulationMode(', 'void RagdollTemplateDialog::Open(', 'void RagdollTemplateDialog::Cancel(', 'void RagdollTemplateDialog::Status(',
        'void RagdollTemplateDialog::ShowTemplate(', 'void RagdollTemplateDialog::ShowRole(', 'bool RagdollTemplateDialog::ReadFields(',
        'void RagdollTemplateDialog::EditPhysicsSettings(', 'void RagdollTemplateDialog::ImportTemplate(', 'void RagdollTemplateDialog::ExportTemplate(', 'std::string RagdollTemplateDialog::PreviewKey(',
        'bool RagdollTemplateDialog::GeneratePreview(', 'bool RagdollTemplateDialog::ApplyPreview(', 'int RagdollTemplateDialog::handleEvent(']]
    source=prefix+'\n'+(TESTS/'fixtures/j10_6_10_2/fitting_editor.cpp.in').read_text().replace('@PRODUCTION@','\n\n'.join(methods))
    with tempfile.TemporaryDirectory(prefix='f2_editor_') as tmp:
        tmp=Path(tmp);unit=tmp/'editor.cpp';unit.write_text(adapt(source));binary=tmp/'editor'
        flags=['-std=c++17','-O0','-g','-fno-exceptions','-ffunction-sections','-fdata-sections','-D_LINUX','-DLINUX',
            '-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp','-D_snprintf=snprintf','-D_vsnprintf=vsnprintf','-Wno-write-strings','-Wno-invalid-offsetof']
        if args.ubsan:flags+=['-fsanitize=undefined','-fno-sanitize-recover=all']
        includes=['utils/pxmv','external/mxtk/include','external/mxtk/include/mx','client/render','game_shared','engine','common','public','game_shared/tests']
        libs=['vector.cpp','matrix.cpp','mathlib.cpp','stringlib.cpp','bone_setup.cpp','ikcontext.cpp','procbones.cpp','jigglebones.cpp','physics_asset_format.cpp','ragdoll_template.cpp']
        command=[os.environ.get('CXX','c++'),*flags,*['-I'+str(ROOT/p) for p in includes],str(unit),*[str(ROOT/'game_shared'/p) for p in libs],'-Wl,--gc-sections','-o',str(binary)]
        build=subprocess.run(command,cwd=ROOT,text=True,capture_output=True)
        if build.returncode:print((build.stdout+build.stderr)[-12000:]);return build.returncode
        run=subprocess.run([str(binary),str(tmp)],cwd=tmp,text=True,capture_output=True);print(run.stdout+run.stderr);return run.returncode
if __name__=='__main__':raise SystemExit(main())
