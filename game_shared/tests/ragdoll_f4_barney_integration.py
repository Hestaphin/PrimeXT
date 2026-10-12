#!/usr/bin/env python3
"""Run actual Barney MDL authoring/pose checks and export new v5 review rigs.
The supplied v4 .phys is SHA-checked as a historical reference, never modified.
"""
from pathlib import Path
import argparse,hashlib,os
import ragdoll_template_model_integration as baseline
ROOT=Path(__file__).resolve().parents[2];TESTS=ROOT/'game_shared/tests'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--model',type=Path,required=True);p.add_argument('--reference',type=Path,required=True);p.add_argument('--export-dir',type=Path);p.add_argument('--ubsan',action='store_true');a=p.parse_args()
 # Reuse the reviewed F1 adapters and production extraction method selection;
 # intercept only the temporary generated C++ fixture to add F4 actual-model cases.
 original_read=Path.read_text
 expected={a.model.resolve():'d326ef89e83489db85e1cc1d3c554f1b7cbc9c43c4bc7a940d76b0d75e1a393f',a.reference.resolve():'8040edc7a4b5de91279730f692a64df41167a4ee42cdcb18777b58f848777490'}
 for path,h in expected.items():
  if hashlib.sha256(path.read_bytes()).hexdigest()!=h:raise ValueError('Input differs from the supplied audited Barney file: '+path.name)
 def read(path,*args,**kwargs):
  content=original_read(path,*args,**kwargs)
  if path.name=='template_model_roundtrip.cpp.in':
   cases=original_read(TESTS/'fixtures/j10_6_10_4/actual_barney_cases.cpp.in')
   content=content.replace('int main(int argc,char **argv)',cases+'\nint main(int argc,char **argv)')
   content=content.replace('    const auto expected=BarneySkeleton();','    F4Barney(model,editor,hdr,bones,std::getenv("F4_EXPORT_DIR")?std::getenv("F4_EXPORT_DIR"):"");\n    const auto expected=BarneySkeleton();')
   content=content.replace('#include <limits>','#include <limits>\n#include "studio_physical_hierarchy.h"')
  return content
 import sys
 previous=sys.argv;sys.argv=[str(TESTS/'ragdoll_template_model_integration.py'),'--model',str(a.model.resolve()),'--asset',str(a.reference.resolve()),*(['--ubsan'] if a.ubsan else [])]
 old_export=os.environ.get('F4_EXPORT_DIR')
 if a.export_dir:os.environ['F4_EXPORT_DIR']=str(a.export_dir.resolve())
 Path.read_text=read
 try:return baseline.main()
 finally:
  Path.read_text=original_read;sys.argv=previous
  if old_export is None:os.environ.pop('F4_EXPORT_DIR',None)
  else:os.environ['F4_EXPORT_DIR']=old_export
if __name__=='__main__':raise SystemExit(main())
