#!/usr/bin/env python3
"""F5 presets, bounded chain generation, production GUI/GL adapters and regressions."""
from pathlib import Path
import argparse,json,os,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[2];TESTS=ROOT/'game_shared/tests'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ubsan',action='store_true');p.add_argument('--inherited',action='store_true');p.add_argument('--barney-model',type=Path);p.add_argument('--barney-reference',type=Path);p.add_argument('--export-dir',type=Path);p.add_argument('--json',type=Path);a=p.parse_args();results=[]
 if bool(a.barney_model)!=bool(a.barney_reference):p.error('Supply both exact Barney inputs')
 def check(label,command):
  r=subprocess.run(command,cwd=ROOT,text=True,capture_output=True)
  if r.stdout or r.stderr:print(r.stdout+r.stderr,flush=True)
  results.append({'test':label,'exit_code':r.returncode});print(('PASS ' if r.returncode==0 else 'FAIL ')+label,flush=True);return r.returncode==0
 flags=['-std=c++17','-O0','-g','-fno-exceptions','-Wall','-Wextra','-Werror','-Wpedantic','-I'+str(ROOT/'game_shared')]
 if a.ubsan:flags+=['-fsanitize=undefined','-fno-sanitize-recover=all']
 adapter=['--ubsan'] if a.ubsan else []
 with tempfile.TemporaryDirectory(prefix='f5_verify_') as tmp:
  core=Path(tmp)/'core'
  if check('strict portable F5 production core compile',[os.environ.get('CXX','c++'),*flags,str(TESTS/'ragdoll_f5_tests.cpp'),str(ROOT/'game_shared/ragdoll_template.cpp'),str(ROOT/'game_shared/physics_asset_format.cpp'),'-o',str(core)]):
   check('seven presets, quad/creature chains, topology, mass, pairs, bounds and deterministic exports',[str(core),*([str(a.export_dir.resolve())] if a.export_dir else [])])
  guide=Path(tmp)/'guide'
  if check('strict F5 shared runtime-frame/guide compile (mathematical SDK facade)',[os.environ.get('CXX','c++'),*flags,'-I'+str(ROOT/'server/physx'),'-I'+str(TESTS/'fixtures/j10_6_10_3'),str(TESTS/'ragdoll_f5_guides.cpp'),str(ROOT/'game_shared/physics_asset_format.cpp'),'-o',str(guide)]):
   check('D6/hinge guide frame parity, arcs, anchor gaps and transactional validation',[str(guide)])
  check('actual production dialog chain controls, review gate, stale preview, apply, undo and cancel',[sys.executable,str(TESTS/'ragdoll_f5_dialog_integration.py'),*adapter])
  check('actual selected-joint overlay with GL-state adapter',[sys.executable,str(TESTS/'ragdoll_f5_overlay_integration.py'),*adapter])
  if a.barney_model:
   command=[sys.executable,str(TESTS/'ragdoll_f5_barney_integration.py'),*adapter,'--model',str(a.barney_model.resolve()),'--reference',str(a.barney_reference.resolve())]
   if a.export_dir:command+=['--export-dir',str(a.export_dir.resolve())]
   check('actual Barney MDL Heavy/Loose generation, save/reload and inherited authoring',command)
  command=[sys.executable,str(TESTS/'verify_j10_6_10_4.py'),*adapter]
  if a.inherited:command+=['--inherited']
  if a.barney_model:command+=['--barney-model',str(a.barney_model.resolve()),'--barney-reference',str(a.barney_reference.resolve())]
  check('F4/F3/F2/F1/F0 and requested inherited regression chain',command)
 summary={'milestone':'J.10.6.10.5','phase':'F5','results':results,'passed':sum(r['exit_code']==0 for r in results),'total':len(results),'actual_barney_requested':bool(a.barney_model),'quad_creature_second_naming_convention':'SYNTHETIC_FIXTURES','native_windows_qt_physx_solver_gpu_game_fps':'NOT_RUN','phase_exit':'PENDING_ADDITIONAL_ACTUAL_MODEL_AND_NATIVE_ACCEPTANCE'}
 if a.json:a.json.parent.mkdir(parents=True,exist_ok=True);a.json.write_text(json.dumps(summary,indent=2)+'\n')
 print(f"J.10.6.10.5 available groups: {summary['passed']}/{summary['total']} passed",flush=True);return 0 if summary['passed']==summary['total'] else 1
if __name__=='__main__':raise SystemExit(main())
