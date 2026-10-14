#!/usr/bin/env python3
"""F2 anatomical generation, bind fitting, resolved frames and authoring checks.
Native SDK/Windows linking and actual Barney drop/mesh acceptance are separate.
"""
from pathlib import Path
import argparse, json, os, subprocess, sys, tempfile
ROOT=Path(__file__).resolve().parents[2]
TESTS=ROOT/'game_shared/tests'
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan',action='store_true');parser.add_argument('--inherited',action='store_true')
    parser.add_argument('--barney-model',type=Path);parser.add_argument('--json',type=Path);args=parser.parse_args();results=[]
    def check(name,command):
        run=subprocess.run(command,cwd=ROOT,text=True,capture_output=True);output=(run.stdout+run.stderr).strip()
        if output:print(output[-25000:],flush=True)
        results.append({'test':name,'exit_code':run.returncode});print(('PASS ' if run.returncode==0 else 'FAIL ')+name,flush=True);return run.returncode==0
    flags=['-std=c++17','-O0','-g','-fno-exceptions','-Wall','-Wextra','-Werror','-Wpedantic','-I'+str(ROOT/'game_shared')]
    if args.ubsan:flags+=['-fsanitize=undefined','-fno-sanitize-recover=all']
    with tempfile.TemporaryDirectory(prefix='f2_verify_') as tmp:
        binary=Path(tmp)/'anatomy'
        if check('strict F2 production core compile',[os.environ.get('CXX','c++'),*flags,str(TESTS/'ragdoll_f2_tests.cpp'),str(ROOT/'game_shared/ragdoll_template.cpp'),str(ROOT/'game_shared/physics_asset_format.cpp'),'-o',str(binary)]):
            check('F2 anatomy, mass regions, fitting, flexion sign, frames, scale and analytic overlap',[str(binary)])
        adapters=['--ubsan'] if args.ubsan else []
        check('actual shared child frame and runtime/editor setters (SDK facade)',[sys.executable,str(TESTS/'ragdoll_f2_frame_integration.py'),*adapters])
        check('actual bind fitting and frame edit/save/reload/undo (synthetic Studio input, GUI adapters)',[sys.executable,str(TESTS/'ragdoll_f2_editor_integration.py'),*adapters])
        command=[sys.executable,str(TESTS/'verify_j10_6_10_1.py'),*adapters]
        if args.inherited:command+=['--inherited']
        if args.barney_model:command+=['--barney-model',str(args.barney_model.resolve())]
        check('F1/F0 and requested inherited regressions (F2 anatomy expectations)',command)
    summary={'milestone':'J.10.6.10.2','phase':'F2','results':results,'passed':sum(r['exit_code']==0 for r in results),'total':len(results),
        'actual_barney_model_requested':bool(args.barney_model),'actual_barney_drop_mesh_stability':'NOT_RUN',
        'native_windows_qt_physx_link_game_certification':False}
    if args.json:args.json.parent.mkdir(parents=True,exist_ok=True);args.json.write_text(json.dumps(summary,indent=2)+'\n')
    print(f"J.10.6.10.2 F2 available groups: {summary['passed']}/{summary['total']} passed",flush=True)
    print('Recorded Barney bounds lack bind rotations and filtered vertices; regenerate using the matching MDL. Native drops/mesh/game remain pending.',flush=True)
    return 0 if summary['passed']==summary['total'] else 1
if __name__=='__main__':raise SystemExit(main())
