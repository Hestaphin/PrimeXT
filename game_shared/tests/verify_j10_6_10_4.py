#!/usr/bin/env python3
"""F4 real pose, CPU palette, compact GLSL expression and inherited checks.
Adapters are explicitly described per test; no native SDK/GPU/game certification.
"""
from pathlib import Path
import argparse,json,os,subprocess,sys
ROOT=Path(__file__).resolve().parents[2];TESTS=ROOT/'game_shared/tests'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ubsan',action='store_true');p.add_argument('--inherited',action='store_true');p.add_argument('--barney-model',type=Path);p.add_argument('--barney-reference',type=Path);p.add_argument('--export-dir',type=Path);p.add_argument('--json',type=Path);a=p.parse_args();results=[]
 if bool(a.barney_model)!=bool(a.barney_reference):p.error('Supply both --barney-model and --barney-reference')
 def check(name,command):
  r=subprocess.run(command,cwd=ROOT,text=True,capture_output=True);output=(r.stdout+r.stderr).strip()
  if output:print(output,flush=True)
  results.append({'test':name,'exit_code':r.returncode});print(('PASS ' if r.returncode==0 else 'FAIL ')+name,flush=True)
 adapter=['--ubsan'] if a.ubsan else []
 for script,label in [('preview','production preview, branched hierarchy and CPU skin palette'),('client','production packet continuity, CPU prediction/draw, parts and transitions'),('shader','actual compact GLSL expressions versus complete matrices (no driver compile)'),('runtime','production body-to-bone capture (mathematical SDK facade)')]:
  check(label,[sys.executable,str(TESTS/f'ragdoll_f4_{script}_integration.py'),*adapter])
 if a.barney_model:
  command=[sys.executable,str(TESTS/'ragdoll_f4_barney_integration.py'),*adapter,'--model',str(a.barney_model.resolve()),'--reference',str(a.barney_reference.resolve())]
  if a.export_dir:command+=['--export-dir',str(a.export_dir.resolve())]
  check('actual SHA-checked Barney MDL generation, passive bone/vertex poses and authoring',command)
 if sys.platform.startswith('linux'):
  flags=['-std=c++17','-fsyntax-only','-fno-exceptions','-Wno-invalid offsetof'.replace(' ','-'),'-Wno-write-strings','-D_LINUX','-DLINUX','-DCLIENT_DLL','-DCLIENT_WEAPONS','-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp','-D_snprintf=snprintf','-D_vsnprintf=vsnprintf']
  includes=['client','client/render','client/visualizer','client/events','common','engine','game_shared','public']
  for source in ['gl_studio_draw.cpp','gl_studiodecal_new.cpp','gl_shader.cpp']:
   check('complete Linux client '+source+' translation unit syntax',[os.environ.get('CXX','c++'),*flags,*['-I'+str(ROOT/x) for x in includes],str(ROOT/'client/render'/source)])
 command=[sys.executable,str(TESTS/'verify_j10_6_10_3.py'),*adapter]
 if a.inherited:command+=['--inherited']
 if a.barney_model:command+=['--barney-model',str(a.barney_model.resolve())]
 check('F3/F2/F1/F0 and requested inherited regression chain',command)
 summary={'milestone':'J.10.6.10.4','phase':'F4','results':results,'passed':sum(x['exit_code']==0 for x in results),'total':len(results),'actual_barney_requested':bool(a.barney_model),'native_windows_physx_solver_gpu_game_fps':'NOT_RUN','physical_exit':'PENDING_NATIVE_AND_GAME_ACCEPTANCE'}
 if a.json:a.json.parent.mkdir(parents=True,exist_ok=True);a.json.write_text(json.dumps(summary,indent=2)+'\n')
 print(f"J.10.6.10.4 available groups: {summary['passed']}/{summary['total']} passed",flush=True)
 return 0 if summary['passed']==summary['total'] else 1
if __name__=='__main__':raise SystemExit(main())
