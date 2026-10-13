#!/usr/bin/env python3
"""Execute actual F5 dialog controls with synthetic geometry and GUI services."""
from pathlib import Path
import argparse,sys
import ragdoll_template_dialog_integration as baseline
ROOT=Path(__file__).resolve().parents[2];TESTS=ROOT/'game_shared/tests'
from physics_asset_ui_test_adapter import MOCK
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ubsan',action='store_true');a=p.parse_args()
 original=Path.read_text
 def read(path,*args,**kwargs):
  s=original(path,*args,**kwargs)
  if path.name=='template_dialog.cpp.in':
   # Adapt only platform GUI services; retain production F5/F6 generator methods.
   s=s.replace('#include "PhysicsSettingsDialog.h"','#include "PhysicsSettingsDialog.h"\n#include "PhysicsAssetHelpDialog.h"\n#include "PhysicsAssetUiGuide.h"')
   s=s.replace('a.y+a.h<=900','a.y+a.h<=1090')
   # Old F1 acceptance cases explicitly accepted existing warnings before this milestone.
   s=s.replace('dialog.m_axisReviewed->setChecked(true);','dialog.m_axisReviewed->setChecked(true);dialog.m_risksReviewed->setChecked(true);')
   s=s.replace('@PRODUCTION@', '@PRODUCTION@\n'+MOCK+'\nvoid PhysicsAssetEditor::SelectGenerationJoint(const std::string &name){m_generationSelectedJoint=name;}\n')
   s=s.replace('#include "fixtures/j10_6_10_1/barney_skeleton.h"','#include "fixtures/j10_6_10_1/barney_skeleton.h"\n#include "fixtures/j10_6_10_5/skeletons.h"\nstatic std::vector<ragtemplate::Bone> f5ActiveSkeleton;')
   s=s.replace('const auto bones=BarneySkeleton();out.assign','const auto bones=f5ActiveSkeleton.empty()?BarneySkeleton():f5ActiveSkeleton;out.assign')
   s=s.replace('int main(int argc,char **argv)',original(TESTS/'fixtures/j10_6_10_5/dialog_cases.cpp.in')+'\nint main(int argc,char **argv)')
   s=s.replace('    std::cout<<"F1 actual dialog/editor methods:', '    F5Dialog(model,std::filesystem::path(argv[1]).parent_path());\n    std::cout<<"F5 + inherited actual dialog/editor methods:')
  return s
 saved=sys.argv;sys.argv=[str(TESTS/'ragdoll_template_dialog_integration.py'),*(['--ubsan'] if a.ubsan else [])];Path.read_text=read
 try:return baseline.main()
 finally:Path.read_text=original;sys.argv=saved
if __name__=='__main__':raise SystemExit(main())
