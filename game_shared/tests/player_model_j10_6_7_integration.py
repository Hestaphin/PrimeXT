#!/usr/bin/env python3
"""Run the exact Xash player selector with explicit entity/engine adapters.

File lookup, model loading and client services are adapters. No OpenGL build or
live networking or engine ABI validation is claimed. --source-tree selects a
Xash tree for reproduction. The selector's player-model cache type is unchanged.
"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile
from pose_j10_6_2_integration import function

TESTS=Path(__file__).resolve().parent

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-tree',type=Path,default=TESTS.parents[2]/'xash3d-fwgs-master')
    parser.add_argument('--ubsan',action='store_true')
    parser.add_argument('--record',type=Path)
    args=parser.parse_args();root=args.source_tree.resolve()
    studio=(root/'ref/gl/gl_studio.c').read_text()
    selector=function(studio,'static model_t *R_StudioSetupPlayerModel')
    begin=studio.index('typedef struct');end=studio.index('} player_model_t;',begin)+len('} player_model_t;')
    template=(TESTS/'fixtures/j10_6_7/player_model_adapter.c.in').read_text()
    records=[]
    with tempfile.TemporaryDirectory(prefix='j1067_player_') as directory:
        directory=Path(directory);source=directory/'player.c';binary=directory/'player'
        source.write_text(template.replace('@PLAYER_MODEL_TYPE@',studio[begin:end]).replace('@SELECTOR@',selector))
        sanitizers=['-fsanitize=undefined','-fno-sanitize-recover=all'] if args.ubsan else []
        command=[os.environ.get('CC','cc'),*sanitizers,'-std=c99','-O0','-g',
            *['-I'+str(root/p) for p in ['common','public','engine']],str(source),'-o',str(binary)]
        build=subprocess.run(command,capture_output=True,text=True,cwd=root)
        if build.returncode:raise RuntimeError((build.stdout+build.stderr)[-6000:])
        for name in ['first-sp','first-dev','stale-sp','no-info','bad-index',
                'fallback-update','menu-preview','dead-body']:
            result=subprocess.run([str(binary),name],capture_output=True,text=True,cwd=directory)
            output=(result.stdout+result.stderr).strip()
            records.append({'case':name,'returncode':result.returncode,'output':output})
            print(('PASS ' if result.returncode==0 else 'FAIL ')+name+': '+output[-1200:],flush=True)
    if args.record:
        args.record.parent.mkdir(parents=True,exist_ok=True)
        args.record.write_text(json.dumps(records,indent=2)+'\n')
    return 0 if all(r['returncode']==0 for r in records) else 1

if __name__=='__main__':raise SystemExit(main())
