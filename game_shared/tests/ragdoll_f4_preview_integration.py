#!/usr/bin/env python3
"""Execute production preview and CPU skinning; optionally reproduce F3 defects."""
from pathlib import Path
import argparse,os,subprocess,tempfile
from ragdoll_f0_integration import function
ROOT=Path(__file__).resolve().parents[2];TESTS=ROOT/'game_shared/tests'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ubsan',action='store_true');p.add_argument('--baseline-root',type=Path);a=p.parse_args()
 studio=(ROOT/'utils/pxmv/studio_render.cpp').read_text();utils=(ROOT/'utils/pxmv/studio_utils.cpp').read_text()
 methods=[function(studio,'StudioModel::StudioModel'),*[function(utils,s) for s in ['void StudioModel::SetToolBoneOverride','void StudioModel::ClearToolBoneOverrides','void StudioModel::CaptureToolAnimationPose','void StudioModel::ReleaseToolAnimationPose','void StudioModel::SetToolPhysicalRootBone','bool StudioModel::GetRagdollFitVertices']]]
 if a.baseline_root:
  old=(a.baseline_root/'utils/pxmv/studio_render.cpp').read_text();start=old.index('\t// Physics Asset Editor preview can temporarily');end=old.index('\n\tfor( i = 0; i < m_pstudiohdr->numbones; i++ )',start)
  methods+=['void StudioModel::ApplyToolBoneOverrides(bool mirrored) {int i;const bool bMirror=mirrored;const auto *pbones=(const mstudiobone_t*)((const byte*)m_pstudiohdr+m_pstudiohdr->boneindex);'+old[start:end]+'}']
  methods+=[function((a.baseline_root/'utils/pxmv/studio_utils.cpp').read_text(),'void StudioModel::ComputeSkinMatrix')]
 else:methods+=[function(studio,'void StudioModel::ApplyToolBoneOverrides'),function(utils,'void StudioModel::ComputeSkinMatrix')]
 source=(TESTS/'fixtures/j10_6_10_4/preview_pose.cpp.in').read_text().replace('@PRODUCTION@','\n\n'.join(methods)).replace('@BASELINE@','true' if a.baseline_root else 'false')
 with tempfile.TemporaryDirectory(prefix='f4_preview_') as tmp:
  tmp=Path(tmp);unit=tmp/'preview.cpp';unit.write_text(source);binary=tmp/'preview'
  flags=['-std=c++17','-O0','-g','-fno-exceptions','-ffunction-sections','-fdata-sections','-D_LINUX','-DLINUX','-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp','-D_snprintf=snprintf','-D_vsnprintf=vsnprintf','-Wno-write-strings','-Wno-invalid offsetof'.replace(' ','-')]
  if a.ubsan:flags+=['-fsanitize=undefined','-fno-sanitize-recover=all']
  includes=['utils/pxmv','external/mxtk/include','external/mxtk/include/mx','client/render','game_shared','engine','common','public']
  libraries=['vector.cpp','matrix.cpp','mathlib.cpp','stringlib.cpp','bone_setup.cpp','ikcontext.cpp','procbones.cpp','jigglebones.cpp']
  command=[os.environ.get('CXX','c++'),*flags,*['-I'+str(ROOT/p) for p in includes],str(unit),*[str(ROOT/'game_shared'/p) for p in libraries],'-Wl,--gc-sections','-o',str(binary)]
  r=subprocess.run(command,cwd=ROOT,text=True,capture_output=True)
  if r.returncode:print((r.stdout+r.stderr)[-12000:]);return r.returncode
  r=subprocess.run([str(binary)],text=True,capture_output=True);print(r.stdout+r.stderr);return r.returncode
if __name__=='__main__':raise SystemExit(main())
