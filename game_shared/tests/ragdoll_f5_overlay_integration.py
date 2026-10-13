#!/usr/bin/env python3
"""Compile actual selected-joint overlay with project math and GL service adapter."""
from pathlib import Path
import argparse,os,subprocess,tempfile
from ragdoll_f0_integration import function
from ragdoll_f5_dialog_integration import MOCK
ROOT=Path(__file__).resolve().parents[2];TESTS=ROOT/'game_shared/tests'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ubsan',action='store_true');a=p.parse_args()
 editor=(ROOT/'utils/pxmv/PhysicsAssetEditor.cpp').read_text();studio=(ROOT/'utils/pxmv/studio_render.cpp').read_text();utils=(ROOT/'utils/pxmv/studio_utils.cpp').read_text()
 signatures=['static Vector ToVector','static matrix3x4 BodyLocalMatrix','static void DrawUnitCircle','static void DrawBox','static void DrawSphere','static void DrawCapsule','static bool BuildReferenceBoneMatrices','static raggeometry::Pose ConstraintPose','PhysicsAssetEditor::PhysicsAssetEditor','void PhysicsAssetEditor::BuildUI','int PhysicsAssetEditor::FindBone(','void PhysicsAssetEditor::DrawOverlay','void PhysicsAssetEditor::RecordPreviewDiagnostics']
 methods=[function(editor,s) for s in signatures]+[function(studio,'StudioModel::StudioModel'),function(utils,'const matrix3x4 &StudioModel::GetToolBoneTransform'),function(studio,'const matrix3x4 &StudioModel::GetToolModelTransform')]
 base=(TESTS/'fixtures/j10_6_8/editor_adapter.cpp.in').read_text();base=base[:base.index('int main(int argc,char **argv)')]
 base=base.replace('#include "PhysicsAssetEditor.h"','#include "PhysicsAssetEditor.h"\n#include "PhysicsAssetUiGuide.h"\n#include "PhysicsAssetHelpDialog.h"')
 source=base.replace('@PRODUCTION@','\n\n'.join(methods))+MOCK+(TESTS/'fixtures/j10_6_10_5/overlay_cases.cpp.in').read_text()
 with tempfile.TemporaryDirectory(prefix='f5_overlay_') as tmp:
  tmp=Path(tmp);unit=tmp/'overlay.cpp';unit.write_text(source);binary=tmp/'overlay'
  flags=['-std=c++17','-O0','-g','-ffunction-sections','-fdata-sections','-D_LINUX','-DLINUX','-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp','-D_snprintf=snprintf','-D_vsnprintf=vsnprintf','-Wno-write-strings','-Wno-invalid offsetof'.replace(' ','-')]
  if a.ubsan:flags+=['-fsanitize=undefined','-fno-sanitize-recover=all']
  includes=['utils/pxmv','external/mxtk/include','client/render','common','engine','game_shared','public'];libs=['vector.cpp','matrix.cpp','mathlib.cpp','stringlib.cpp','bone_setup.cpp','ikcontext.cpp','procbones.cpp','jigglebones.cpp','physics_asset_format.cpp']
  r=subprocess.run([os.environ.get('CXX','c++'),*flags,*['-I'+str(ROOT/x) for x in includes],str(unit),*[str(ROOT/'game_shared'/x) for x in libs],'-Wl,--gc-sections','-o',str(binary)],capture_output=True,text=True)
  if r.returncode:print((r.stdout+r.stderr)[-12000:]);return r.returncode
  r=subprocess.run([str(binary)],capture_output=True,text=True);print(r.stdout+r.stderr);return r.returncode
if __name__=='__main__':raise SystemExit(main())
