#!/usr/bin/env python3
"""F3 v5 settings, shared PhysX setup, pair filtering and editor persistence.
Portable tests use declared recording/GUI adapters; native SDK tests are separate.
"""
from pathlib import Path
import argparse, json, os, subprocess, sys, tempfile
ROOT=Path(__file__).resolve().parents[2];TESTS=ROOT/'game_shared/tests'
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ubsan',action='store_true');p.add_argument('--inherited',action='store_true');p.add_argument('--json',type=Path);p.add_argument('--barney-model',type=Path);a=p.parse_args();results=[]
    def check(name,cmd):
        r=subprocess.run(cmd,cwd=ROOT,text=True,capture_output=True);output=(r.stdout+r.stderr).strip()
        if output:print(output[-32000:],flush=True)
        results.append({'test':name,'exit_code':r.returncode});print(('PASS ' if not r.returncode else 'FAIL ')+name,flush=True);return not r.returncode
    flags=['-std=c++17','-O0','-g','-fno-exceptions','-Wall','-Wextra','-Werror','-Wpedantic','-I'+str(ROOT/'game_shared')]
    if a.ubsan:flags+=['-fsanitize=undefined','-fno-sanitize-recover=all']
    with tempfile.TemporaryDirectory(prefix='f3_verify_') as tmp:
        binary=Path(tmp)/'core'
        if check('strict portable v5/template compile',[os.environ.get('CXX','c++'),*flags,str(TESTS/'ragdoll_f3_tests.cpp'),str(ROOT/'game_shared/ragdoll_template.cpp'),str(ROOT/'game_shared/physics_asset_format.cpp'),'-o',str(binary)]):check('v5 profiles, overrides, scale, collision generation and large immutable tables',[str(binary)])
        adapter=['--ubsan'] if a.ubsan else []
        check('actual shared SDK construction, shaders, CCD handoff and release lifetime (recording facade)',[sys.executable,str(TESTS/'ragdoll_f3_physx_integration.py'),*adapter])
        check('actual settings/editor/template transactions and persistence (GUI and Studio adapters)',[sys.executable,str(TESTS/'ragdoll_f3_editor_integration.py'),*adapter])
        check('actual Studio bind extractor malformed/reference tests',[sys.executable,str(TESTS/'prepare_f3_drop_input_tests.py')])
        check('native capture C++ syntax (recording declarations; no real SDK link)',[os.environ.get('CXX','c++'),'-std=c++17','-fsyntax-only',*['-I'+str(ROOT/x) for x in ['game_shared/tests/fixtures/j10_6_10_3','game_shared','server/physx']],str(TESTS/'native_f3/drop_capture.cpp')])
        command=[sys.executable,str(TESTS/'verify_j10_6_10_2.py'),*adapter]
        if a.inherited:command+=['--inherited']
        if a.barney_model:command+=['--barney-model',str(a.barney_model.resolve())]
        check('F2/F1/F0 and requested inherited regression chain under v5',command)
    summary={'milestone':'J.10.6.10.3','phase':'F3','results':results,'passed':sum(x['exit_code']==0 for x in results),'total':len(results),'native_physx_solver_windows_qt_game_fps':'NOT_RUN','actual_model_requested':bool(a.barney_model),'physical_exit':'PENDING_NATIVE_AND_GAME_ACCEPTANCE'}
    if a.json:a.json.parent.mkdir(parents=True,exist_ok=True);a.json.write_text(json.dumps(summary,indent=2)+'\n')
    print(f"J.10.6.10.3 F3 available groups: {summary['passed']}/{summary['total']} passed",flush=True);return 0 if summary['passed']==summary['total'] else 1
if __name__=='__main__':raise SystemExit(main())
