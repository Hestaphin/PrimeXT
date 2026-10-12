#!/usr/bin/env python3
"""Execute actual GLSL palette expressions with real Studio quaternion helpers."""
from pathlib import Path
import argparse,os,subprocess,tempfile,re
from ragdoll_f0_integration import function
ROOT=Path(__file__).resolve().parents[2];TESTS=ROOT/'game_shared/tests'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ubsan',action='store_true');a=p.parse_args()
 skin=(ROOT/'game_dir/glsl/skinning.h').read_text();draw=(ROOT/'client/render/gl_studio_draw.cpp').read_text();decal=(ROOT/'client/render/gl_studiodecal_new.cpp').read_text()
 assert 'uniform vec3 u_BoneScale;' in skin
 assert skin.count('Mat4FromScaledOriginQuat( u_BoneQuaternion')==6
 assert draw.count('= physicsasset::StudioRenderBoneQuaternion(')==6
 for text in (draw,decal):assert 'case UT_BONESCALE:' in text and 'physicsasset::StudioRenderBoneScale(' in text
 assert '"u_BoneScale"' in (ROOT/'client/render/gl_shader.cpp').read_text()
 helpers=function((ROOT/'game_dir/glsl/matrix.h').read_text(),'mat4 Mat4FromOriginQuat')+'\n'+function(skin,'mat4 Mat4FromScaledOriginQuat')
 helpers=helpers.replace('const in ','const ')
 source=(TESTS/'fixtures/j10_6_10_4/shader_palette.cpp.in').read_text().replace('@PRODUCTION@',helpers)
 with tempfile.TemporaryDirectory(prefix='f4_shader_') as tmp:
  tmp=Path(tmp);unit=tmp/'palette.cpp';unit.write_text(source);binary=tmp/'palette'
  flags=['-std=c++17','-O0','-fno-exceptions','-D_LINUX','-DLINUX','-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp','-D_snprintf=snprintf','-D_vsnprintf=vsnprintf','-Wno-write-strings']
  if a.ubsan:flags+=['-fsanitize=undefined','-fno-sanitize-recover=all']
  command=[os.environ.get('CXX','c++'),*flags,*['-I'+str(ROOT/x) for x in ['game_shared','common','engine','public']],str(unit),*[str(ROOT/'game_shared'/p) for p in ['matrix.cpp','vector.cpp','mathlib.cpp','stringlib.cpp']],'-o',str(binary)]
  r=subprocess.run(command,cwd=ROOT,text=True,capture_output=True)
  if r.returncode:print((r.stdout+r.stderr)[-10000:]);return r.returncode
  r=subprocess.run([str(binary)],text=True,capture_output=True);print(r.stdout+r.stderr);return r.returncode
if __name__=='__main__':raise SystemExit(main())
