#!/usr/bin/env python3
"""Production sky/render/ref-stack methods with explicit view/GPU adapters.

Run against --source-tree for before/after reproduction. Actual GPU images,
framebuffer implementations and native game maps remain separate acceptance.
"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile
from hud_enttools_j10_6_9_2_integration import function

TESTS=Path(__file__).resolve().parent


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-tree',type=Path,default=TESTS.parents[1])
    parser.add_argument('--ubsan',action='store_true')
    parser.add_argument('--record',type=Path)
    args=parser.parse_args();root=args.source_tree.resolve()
    if os.name=='posix':
        import resource
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    main=(root/'client/render/gl_rmain.cpp').read_text()
    sky=(root/'client/render/gl_sky.cpp').read_text()
    misc=(root/'client/render/gl_rmisc.cpp').read_text()
    header=(root/'client/render/gl_local.h').read_text()
    code='\n'.join(function(main,name) for name in ['void R_InitRefState','void R_ResetRefState',
        'void R_PushRefState','void R_PopRefState','void R_Clear(', 'void R_RenderScene('])
    code+='\n'+'\n'.join(function(sky,name) for name in ['void R_DrawSkyBox(', 'void R_DrawSkyPortal(', 'void R_CheckSkyPortal('])
    code+='\n'+(function(misc,'void R_SyncSkyboxTextures(') if 'void R_SyncSkyboxTextures(' in misc else 'void R_SyncSkyboxTextures() {}')
    template=(TESTS/'fixtures/j10_6_9_3/sky_adapter.cpp.in').read_text()
    template=template.replace('@PRODUCTION@',code).replace('@PARAMS@',function(header,'enum RefParams')+';\n'+function(header,'inline RefParams operator|('))
    template=template.replace('@STATE_MACRO@','#define PER_VIEW_SKY' if 'bool skyPortalRendered;' in header else '')
    records=[]
    with tempfile.TemporaryDirectory(prefix='j10693_sky_',dir=TESTS.parents[1]) as temporary:
        directory=Path(temporary);unit=directory/'sky.cpp';binary=directory/'sky';unit.write_text(template)
        command=[os.environ.get('CXX','c++'),'-std=c++17','-O0','-g',str(unit),'-o',str(binary)]
        if args.ubsan:command[1:1]=['-fsanitize=undefined','-fno-sanitize-recover=all']
        env=os.environ.copy();env['TMPDIR']=str(directory)
        built=subprocess.run(command,cwd=root,env=env,capture_output=True,text=True)
        if built.returncode:raise RuntimeError((built.stdout+built.stderr)[-10000:])
        for name in ['map-sky-sync','sky-unload','recursive-state','fresh-frame-state','portal-clear',
            'ordinary-clear','masked-clear','nested-scene-order','cubemap-portal-clear','reflection-portal-clear',
            'portal-disabled','portal-overview','portal-recursion','portal-null','portal-fov','portal-pass-flags','missing-sky-sides']:
            result=subprocess.run([str(binary),name],cwd=root,env=env,capture_output=True,text=True)
            output=(result.stdout+result.stderr).strip();records.append({'case':name,'returncode':result.returncode,'output':output[-2000:]})
            print(('PASS ' if not result.returncode else 'FAIL ')+name+(': '+output if output else ''),flush=True)
    if args.record:args.record.write_text(json.dumps(records,indent=2)+'\n')
    passed=sum(r['returncode']==0 for r in records)
    print(f'J.10.6.9.3 sky production cases: {passed}/{len(records)} passed',flush=True)
    return 0 if passed==len(records) else 1


if __name__=='__main__':raise SystemExit(main())
