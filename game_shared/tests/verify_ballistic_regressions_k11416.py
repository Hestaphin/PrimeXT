#!/usr/bin/env python3
"""Retained SI/time/range/material/shape regressions and optional exact BSP query.

Pass --xash-root for the matching patched engine's three production query
headers. Only those headers are used; collision/model/edict services are
recording adapters. This is not a full engine or live-map test.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir',type=Path);parser.add_argument('--json',type=Path)
    parser.add_argument('--xash-root',type=Path);args=parser.parse_args()
    temporary=tempfile.TemporaryDirectory(prefix='ballistic-retained-k11416-') if not args.work_dir else None
    work=args.work_dir.resolve() if args.work_dir else Path(temporary.name);work.mkdir(parents=True,exist_ok=True)
    cxx=shlex.split(os.environ.get('CXX','g++'));cc=shlex.split(os.environ.get('CC','gcc'));results=[]
    environment=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1')
    def run(name,command):
        result=subprocess.run([str(x) for x in command],cwd=ROOT,capture_output=True,text=True,env=environment)
        (work/(name+'.log')).write_text(result.stdout+result.stderr)
        results.append({'name':name,'returncode':result.returncode})
        print(name,'PASS' if result.returncode==0 else 'FAIL',flush=True)
        if result.returncode:print((result.stdout+result.stderr)[-6000:],flush=True)
        return result.returncode==0
    sanitize=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-pie','-no-pie']
    # LeakSanitizer needs process introspection unavailable in this runner.
    # Use the primary suite's runtime hook; Address/UB checks stay enabled.
    asan_defaults=work/'asan_defaults.cpp'
    asan_defaults.write_text('extern "C" const char *__asan_default_options(){return "detect_leaks=0:halt_on_error=1";}\n')
    shared=['-I'+str(ROOT/'game_shared'),'-I'+str(ROOT/'engine')]
    implementations=[ROOT/'game_shared'/name for name in ['ballistic_flight.cpp','ballistic_path_cache.cpp','ballistic_penetration.cpp']]
    tests=['ballistic_flight_test','ballistic_j8_tests','ballistic_j9_tests','ballistic_j10_1_tests',
        'ballistic_j10_2_tests','ballistic_j10_6_1_tests','ballistic_shape_j10_6_3_tests']
    query_hashes={}
    bsp_game=None
    if args.xash_root:
        xash=args.xash_root.resolve()
        for name in ['engine/world_ballistic_query.h','engine/common/world_solid_interval.h','engine/server/world_ballistic_query_impl.h']:
            path=xash/name
            if not path.is_file():parser.error('Missing patched engine header: '+str(path))
            query_hashes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
        assert (xash/'engine/world_ballistic_query.h').read_bytes()==(ROOT/'engine/world_ballistic_query.h').read_bytes()
        # Same production-method extraction used by the original J.10.6.1 test.
        source=(ROOT/'server/modular_ballistics.cpp').read_text()
        coordinates=source[source.index('Vector GamePoint('):source.index('modular::BallisticVec3 Mix(')]
        materials=source[source.index('struct PenetrationMaterial '):source.index('struct Shot {')]
        methods=source[source.index('struct PenetrationCandidate {'):source.index('// Invoked once per shot')]
        fixture=ROOT/'game_shared/tests/fixtures/j10_6_1'
        bsp_game=work/'bsp_game.cpp'
        bsp_game.write_text((fixture/'game_adapter.cpp.in').read_text().replace('// @PRODUCTION_CODE@',coordinates+materials+methods))
    for optimization in ['O0','O2']:
        for name in tests:
            exe=work/(name+'_'+optimization)
            extras=[ROOT/'game_shared/modular_weapons.cpp'] if name=='ballistic_flight_test' else []
            if run(name+'_compile_'+optimization,[*cxx,'-std=c++17','-'+optimization,'-g','-Wall','-Wextra','-Werror',
                *sanitize,*shared,ROOT/'game_shared/tests'/(name+'.cpp'),*implementations,*extras,asan_defaults,'-o',exe]):
                params=[ROOT/'documentation/examples/modular-ballistics'] if name=='ballistic_flight_test' else []
                run(name+'_run_'+optimization,[exe,*params])
        if bsp_game:
            obj=work/('engine_query_'+optimization+'.o');exe=work/('bsp_exit_'+optimization)
            if run('bsp_engine_compile_'+optimization,[*cc,'-std=gnu99','-'+optimization,'-g','-Wall','-Wextra','-Werror',*sanitize,
                *['-I'+str(xash/p) for p in ['engine','engine/common','engine/server']],
                ROOT/'game_shared/tests/fixtures/j10_6_1/engine_adapter.c','-c','-o',obj]):
                if run('bsp_game_compile_'+optimization,[*cxx,'-std=c++17','-'+optimization,'-g','-Wno-unknown-pragmas',*sanitize,
                    *['-I'+str(ROOT/p) for p in ['common','public','engine','game_shared']],bsp_game,obj,
                    ROOT/'game_shared/ballistic_flight.cpp',ROOT/'game_shared/ballistic_penetration.cpp',asan_defaults,'-o',exe]):
                    run('bsp_exit_run_'+optimization,[exe])
    report={'milestone':'K.11.4.1.6','passed':sum(x['returncode']==0 for x in results),'total':len(results),'results':results,
        'bsp_query_tested':bool(bsp_game),'engine_query_header_sha256':query_hashes,
        'scope':'Retained actual SI/curve/time/range/penetration/shape policies under ASan/UBSan O0/O2. Optional production engine first-solid query and game Prepare/Begin with recording hull/entity/material services. No full engine/native solver/live-map/Windows run.'}
    if args.json:args.json.parent.mkdir(parents=True,exist_ok=True);args.json.write_text(json.dumps(report,indent=2)+'\n')
    print(f"K.11.4.1.6 ballistic regressions: {report['passed']}/{report['total']} PASS",flush=True)
    return int(report['passed']!=report['total'])

if __name__=='__main__':raise SystemExit(main())
