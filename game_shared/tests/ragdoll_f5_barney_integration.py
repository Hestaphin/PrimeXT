#!/usr/bin/env python3
"""Generate Heavy/Loose candidates from the SHA-checked actual Barney MDL."""
from pathlib import Path
import argparse,hashlib,os,sys
import ragdoll_template_model_integration as baseline
ROOT=Path(__file__).resolve().parents[2];TESTS=ROOT/'game_shared/tests'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--model',type=Path,required=True);p.add_argument('--reference',type=Path,required=True);p.add_argument('--export-dir',type=Path);p.add_argument('--ubsan',action='store_true');a=p.parse_args()
 expected={a.model.resolve():'d326ef89e83489db85e1cc1d3c554f1b7cbc9c43c4bc7a940d76b0d75e1a393f',a.reference.resolve():'8040edc7a4b5de91279730f692a64df41167a4ee42cdcb18777b58f848777490'}
 for path,h in expected.items():
  if hashlib.sha256(path.read_bytes()).hexdigest()!=h:raise ValueError('Input differs from supplied matching Barney: '+path.name)
 original=Path.read_text
 def read(path,*args,**kwargs):
  text=original(path,*args,**kwargs)
  if path.name=='template_model_roundtrip.cpp.in':
   text=text.replace('int main(int argc,char **argv)',original(TESTS/'fixtures/j10_6_10_5/actual_barney_cases.cpp.in')+'\nint main(int argc,char **argv)')
   text=text.replace('    const auto expected=BarneySkeleton();','    F5Barney(model,editor,hdr,bones,std::getenv("F5_EXPORT_DIR")?std::getenv("F5_EXPORT_DIR"):"");\n    const auto expected=BarneySkeleton();')
  return text
 saved=sys.argv;old=os.environ.get('F5_EXPORT_DIR');sys.argv=[str(TESTS/'ragdoll_template_model_integration.py'),'--model',str(a.model.resolve()),'--asset',str(a.reference.resolve()),*(['--ubsan'] if a.ubsan else [])]
 if a.export_dir:os.environ['F5_EXPORT_DIR']=str(a.export_dir.resolve())
 Path.read_text=read
 try:return baseline.main()
 finally:
  Path.read_text=original;sys.argv=saved
  if old is None:os.environ.pop('F5_EXPORT_DIR',None)
  else:os.environ['F5_EXPORT_DIR']=old
if __name__=='__main__':raise SystemExit(main())
