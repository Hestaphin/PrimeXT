#!/usr/bin/env python3
"""J.10.6.10.1 / F1 templates, transaction UI, resolved motion and examples.

Adapters are explicitly limited to unavailable GUI/OS/SDK services. Native
Windows/Qt/PhysX links, a real pxmv run, and live game stability are separate.
"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
TESTS=ROOT/'game_shared/tests'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ubsan',action='store_true')
    parser.add_argument('--inherited',action='store_true')
    parser.add_argument('--json',type=Path)
    parser.add_argument('--barney-model',type=Path,
                        help='Also exercise actual MDL bounds and main editor edit/save/reload using the supplied matching model')
    args=parser.parse_args();results=[]
    def check(name,command,**kwargs):
        run=subprocess.run(command,cwd=ROOT,text=True,capture_output=True,**kwargs)
        output=(run.stdout+run.stderr).strip()
        if output: print(output[-14000:],flush=True)
        results.append({'test':name,'exit_code':run.returncode})
        print(('PASS ' if run.returncode==0 else 'FAIL ')+name,flush=True)
        return run.returncode==0
    cxx=os.environ.get('CXX','c++')
    flags=['-std=c++17','-O0','-g','-fno-exceptions','-Wall','-Wextra','-Werror','-Wpedantic',
           '-I'+str(ROOT/'game_shared')]
    if args.ubsan: flags+=['-fsanitize=undefined','-fno-sanitize-recover=all']
    core=[str(ROOT/'game_shared'/name) for name in ['ragdoll_template.cpp','physics_asset_format.cpp']]
    adapters=['--ubsan'] if args.ubsan else []
    with tempfile.TemporaryDirectory(prefix='f1_verify_') as tmp:
        tmp=Path(tmp)
        binary=tmp/'core'
        if check('strict actual template/format compilation',[cxx,*flags,str(TESTS/'ragdoll_template_f1_tests.cpp'),*core,'-o',str(binary)]):
            check('parser, aliases/topology, geometry, preservation, deterministic generation and cancel/apply/undo', [str(binary),str(tmp)])
        binary=tmp/'motion'
        if check('production angular setter compilation (SDK facade)',[cxx,*flags,'-I'+str(TESTS/'fixtures/j10_6_10_1'),str(TESTS/'ragdoll_template_motion_tests.cpp'),'-o',str(binary)]):
            check('free/limited/mixed angular modes and unchanged translations (NO solver)',[str(binary)])
        check('complete Windows template persistence branch (declarations; NO SDK link)',
              [cxx,*flags,'-D_WIN32','-I'+str(TESTS/'fixtures/j10_6_10_1'),'-fsyntax-only',core[0]])
        check('actual template dialog/editor methods (GUI/engine services adapted)',
              [sys.executable,str(TESTS/'ragdoll_template_dialog_integration.py'),*adapters])
        if args.barney_model:
            check('actual supplied MDL bounds and editor body/joint edit/save/reload (GUI services adapted)',
                  [sys.executable,str(TESTS/'ragdoll_template_model_integration.py'),'--model',str(args.barney_model.resolve()),*adapters])
        else:
            print('SKIP optional actual-model integration: supply --barney-model /path/to/matching/barney.mdl.',flush=True)
        check('actual toolkit label lifetime/close notifications (Qt/Win32 services adapted)',
              [sys.executable,str(TESTS/'ragdoll_template_toolkit_integration.py'),*adapters])
        unit=tmp/'examples.cpp';unit.write_text((TESTS/'fixtures/j10_6_10_1/template_examples.cpp.in').read_text())
        binary=tmp/'examples'
        if check('example factory actual production compilation',[cxx,*flags,'-I'+str(TESTS),str(unit),*core,'-o',str(binary)]):
            check('published presets/candidates equal actual generation and support concrete edits',
                  [str(binary),str(ROOT/'documentation/examples/ragdoll_templates_f3'),'--verify'])
        check('F1 runtime/editor/format/toolkit source wiring', [sys.executable,str(TESTS/'ragdoll_template_source_tests.py')])
        check('inherited editor methods and complete pxmv syntax (GL declarations; PhysX off)',
              [sys.executable,str(TESTS/'verify_j10_6_8.py'),'--skip-inherited',*adapters])
        check('inherited F0 recorder/measurements (skip only F0 unchanged-physics hashes)',
              [sys.executable,str(TESTS/'verify_j10_6_10.py'),'--skip-baseline-scope',*adapters,
               *(['--inherited'] if args.inherited else [])])
    summary={'milestone':'J.10.6.10.1','phase':'F1','results':results,
             'passed':sum(r['exit_code']==0 for r in results),'total':len(results),
             'native_gui_physx_windows_game_certification':False}
    if args.json:
        args.json.parent.mkdir(parents=True,exist_ok=True);args.json.write_text(json.dumps(summary,indent=2)+'\n')
    print(f"J.10.6.10.1 F1 available groups: {summary['passed']}/{summary['total']} passed",flush=True)
    print('Native linking, real GUI/solver, Windows file commit, live Barney/FPS remain pending.',flush=True)
    return 0 if summary['passed']==summary['total'] else 1

if __name__=='__main__': raise SystemExit(main())
