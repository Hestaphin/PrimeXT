#!/usr/bin/env python3
"""Audit runtime body-to-bone capture with the recording SDK mathematical facade."""
from pathlib import Path
import argparse,os,subprocess,tempfile
from ragdoll_f0_integration import function
ROOT=Path(__file__).resolve().parents[2];TESTS=ROOT/'game_shared/tests'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ubsan',action='store_true');a=p.parse_args()
 runtime=(ROOT/'server/physx/physics_asset.cpp').read_text()
 assert 'runtime.bodyToBonePose = runtime.bodyLocalPose.getInverse();' in runtime
 assert 'physicsasset::CreateAssetJoint(' in runtime
 preview=(ROOT/'utils/pxmv/PhysicsAssetEditor.cpp').read_text()
 assert 'actor->getGlobalPose() * local.getInverse()' in preview and 'SetToolPhysicalRootBone(m_preview->diagnosticsRootBone)' in preview
 source=(TESTS/'fixtures/j10_6_10_4/runtime_pose.cpp.in').read_text().replace('@PRODUCTION@',function(runtime,'void PhysicsAssetInstance::CaptureSimulationPose'))
 with tempfile.TemporaryDirectory(prefix='f4_runtime_') as tmp:
  tmp=Path(tmp);unit=tmp/'capture.cpp';unit.write_text(source);binary=tmp/'capture'
  flags=['-std=c++17','-O0','-Wall','-Wextra','-Werror','-Wpedantic','-fno-exceptions']
  if a.ubsan:flags+=['-fsanitize=undefined','-fno-sanitize-recover=all']
  r=subprocess.run([os.environ.get('CXX','c++'),*flags,'-I'+str(TESTS/'fixtures/j10_6_10_3'),'-I'+str(ROOT/'game_shared'),str(unit),str(ROOT/'game_shared/performance_profile.cpp'),'-o',str(binary)],text=True,capture_output=True)
  if r.returncode:print(r.stdout+r.stderr);return r.returncode
  r=subprocess.run([str(binary)],text=True,capture_output=True);print(r.stdout+r.stderr);return r.returncode
if __name__=='__main__':raise SystemExit(main())
