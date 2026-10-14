#!/usr/bin/env python3
"""K.11.4.1.6 curve accuracy, actual manager/penetration/time/save checks.

Independent RK4 reference, recording collision/entity services, ASan/UBSan and
native Linux objects. Retains the actual BSP/shape and v4 damage regressions.
No full game/PhysX/Windows run or in-game FPS measurement.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
from ragdoll_f0_integration import function

ROOT=Path(__file__).resolve().parents[2]
FIXTURE=ROOT/'game_shared/tests/fixtures/ballistic_k11416/manager.cpp.in'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir',type=Path);parser.add_argument('--json',type=Path)
    parser.add_argument('--retained',action='store_true');args=parser.parse_args()
    temporary=tempfile.TemporaryDirectory(prefix='ballistic-k11416-') if not args.work_dir else None
    work=args.work_dir.resolve() if args.work_dir else Path(temporary.name);work.mkdir(parents=True,exist_ok=True)
    compiler=shlex.split(os.environ.get('CXX','g++'));results=[]
    def run(name,command,env=None):
        result=subprocess.run([str(x) for x in command],cwd=ROOT,capture_output=True,text=True,env=env)
        (work/(name+'.log')).write_text(result.stdout+result.stderr)
        results.append({'name':name,'returncode':result.returncode})
        print(name,'PASS' if result.returncode==0 else 'FAIL',flush=True)
        if result.returncode:print((result.stdout+result.stderr)[-6000:],flush=True)
        return result.returncode==0
    source=(ROOT/'server/modular_ballistics.cpp').read_text()
    template=(ROOT/'game_shared/tests/fixtures/j10_6_3/ballistic_manager_adapter.cpp.in').read_text()
    # Reuse recording engine/entity declarations; replace their old trace and
    # penetration stubs. All manager/build/sample/impact/transit methods below
    # are extracted unchanged from production, including their real constants.
    template=template[:template.index('bool near(double')]
    template=template.replace('#include "ballistic_flight.h"',
        '#include "ballistic_flight.h"\n#include "ballistic_penetration.h"\n#include "performance_profile.h"')
    template=template.replace('Vector Normalize()const{',
        'float Length()const{return std::sqrt(x*x+y*y+z*z);}\n'
        '    bool operator==(Vector b)const{return x==b.x&&y==b.y&&z==b.z;}\n    Vector Normalize()const{')
    constants='\n'.join(line for line in source.splitlines() if line.startswith('constexpr ')
        and not line.startswith('constexpr int kShotDiskVersion'))
    old='\n'.join(line for line in template.splitlines() if line.startswith('constexpr '))
    template=template.replace(old,constants+'\nconstexpr int SOLID_NOT=0,MOVETYPE_NONE=0,EF_NODRAW=1,DMG_BULLET=2;')
    template=template.replace('float gravityScale=0,airDensity=0;',
        'float gravityScale=0,airDensity=0,curveHz=50;\nextern const Vector g_vecZero;')
    template=template.replace(function(template,'float CVAR_GET_FLOAT'),
        'float CVAR_GET_FLOAT(const char* n){return std::string(n)=="sv_ballistics_hz"?curveHz:\n'
        '    std::string(n)=="sv_ballistics_gravity_scale"?gravityScale:\n'
        '    std::string(n)=="sv_ballistics_air_density"?airDensity:0;}')
    template=template.replace('struct PenetrationCandidate {};',function(source,'struct PenetrationCandidate {')+';')
    template=template.replace(function(template,'bool PreparePenetration'),
        'bool PreparePenetration(Shot&,const TraceResult&,int,const Vector&,const Vector&,\n'
        '    const modular::BallisticState&,PenetrationCandidate&);')
    template=template.replace(function(template,'bool BeginPenetration'),'')
    template=template.replace(function(template,'int BulletTrace'),
        'int BulletTrace(Vector,Vector,CBaseEntity*,TraceResult&);')
    production=source[source.index('Vector GamePoint('):source.index('// These are gameplay-oriented')]
    for name in ['bool BuildCurve','modular::BallisticState At','void Impact','bool BeginPenetration',
        'bool AdvanceShot','void CModularBallisticManager::Spawn','bool CModularBallisticManager::Add',
        'void CModularBallisticManager::Update','CModularBallisticManager::~CModularBallisticManager',
        'int CModularBallisticManager::Save','int CModularBallisticManager::Restore']:
        production+=function(source,name)+'\n'
    template=template.replace('@SHOT_DECLARATION@',function(source,'struct Shot {')+';')
    template=template.replace('@DISK_DECLARATION@',function(source,'struct ShotDisk {')+';')
    declaration=function(source,'class CModularBallisticManager :').replace('private:','public:')+';'
    template=template.replace('@MANAGER_DECLARATION@',declaration)
    template=template.replace('@DISK_VERSION@',next(x for x in source.splitlines() if x.startswith('constexpr int kShotDiskVersion')))
    template=template.replace('@PRODUCTION@',production)+'\n'+FIXTURE.read_text()
    manager=work/'manager.cpp';manager.write_text(template)
    common=['-std=c++17','-fno-exceptions','-Wno-write-strings']
    strict=['-Wall','-Wextra','-Werror','-Wpedantic']
    sanitize=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-pie','-no-pie']
    shared=['-I'+str(ROOT/'game_shared'),'-I'+str(ROOT/'server')]
    math=ROOT/'game_shared/tests/ballistic_curve_k11416_tests.cpp'
    implementations=[ROOT/'game_shared'/name for name in ['ballistic_flight.cpp','ballistic_path_cache.cpp']]
    for optimization in ['O0','O2']:
        exe=work/('curves_'+optimization)
        if run('curves_compile_'+optimization,[*compiler,*common,'-'+optimization,'-g',*strict,*sanitize,
            *shared,math,*implementations,'-o',exe]):run('curves_run_'+optimization,[exe])
        exe=work/('manager_'+optimization)
        if run('manager_compile_'+optimization,[*compiler,*common,'-'+optimization,'-g',*sanitize,*shared,manager,
            *implementations,ROOT/'game_shared/ballistic_penetration.cpp',ROOT/'game_shared/modular_weapons.cpp',
            ROOT/'game_shared/performance_profile.cpp','-o',exe]):
            for case in ['rates','fps','thin','penetration','multiple','transit_save','restore','rate_change','damage','backlog']:
                run('manager_'+case+'_'+optimization,[exe,case,ROOT/'documentation/examples/modular-format3'])
    exe=work/'windows_macros'
    if run('windows_macros_compile',[*compiler,*common,'-O2',*strict,*sanitize,*shared,'-DBALLISTIC_WINDOWS_MACROS',
        math,*implementations,'-o',exe]):run('windows_macros_run',[exe])
    exe=work/'curve_microbenchmark'
    if run('curve_microbenchmark_compile',[*compiler,*common,'-O2',*strict,*shared,math,*implementations,'-o',exe]):
        run('curve_microbenchmark_run',[exe,'benchmark'])
    definitions=['-D_LINUX=1','-DLINUX=1','-D_vsnprintf=vsnprintf','-D_snprintf=snprintf',
        '-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp']
    native=['-I'+str(ROOT/p) for p in ['server','server/entities','server/monsters','server/weapons',
        'server/gamerules','common','engine','public','game_shared']]
    native_sources=['server/game.cpp','server/modular_ballistics.cpp','game_shared/ballistic_flight.cpp',
        'game_shared/ballistic_path_cache.cpp','game_shared/performance_profile.cpp']
    for optimization in ['O0','O2']:
        for path in native_sources:
            name='native_'+Path(path).stem+'_'+optimization
            run(name,[*compiler,*common,'-'+optimization,*definitions,*native,'-Wno-invalid offsetof'.replace(' ','-'),
                '-c',ROOT/path,'-o',work/(name+'.o')])
    game=(ROOT/'server/game.cpp').read_text()
    wiring='cvar_t sv_ballistics_hz = { "sv_ballistics_hz", "50", FCVAR_ARCHIVE | FCVAR_SERVER };' in game and \
        'CVAR_REGISTER( &sv_ballistics_hz );' in game and 'CVAR_GET_FLOAT("sv_ballistics_hz")' in source
    results.append({'name':'server_rate_registration','returncode':0 if wiring else 1})
    print('server_rate_registration','PASS' if wiring else 'FAIL',flush=True)
    spec=importlib.util.spec_from_file_location('extractor',ROOT/'tools/performance/extract_profile_csv.py')
    extractor=importlib.util.module_from_spec(spec);spec.loader.exec_module(extractor)
    metrics={row[2] for row in extractor.extract((work/'manager_rates_O2.log').read_text())}
    invalid=extractor.extract('PERF_K11411,server,1,20invalid,count,1,1,1,1,1,1,1\n'
        'PERF_K11411,server,1,invalid-name,count,1,1,1,1,1,1,1\n')
    csv_ok={'curve_builds_20ms','curve_builds_10ms','curve_accuracy_fallbacks','curve_samples'}.issubset(metrics) and not invalid
    results.append({'name':'curve_metrics_csv','returncode':0 if csv_ok else 1})
    print('curve_metrics_csv','PASS' if csv_ok else 'FAIL',flush=True)
    if args.retained:
        run('retained_k11415',[sys.executable,ROOT/'client/render/tests/verify_render_budget_k11415.py','--retained',
            '--work-dir',work/'retained','--json',work/'retained.json'])
    report={'milestone':'K.11.4.1.6','passed':sum(x['returncode']==0 for x in results),'total':len(results),'results':results,
        'source_sha256':{path:hashlib.sha256((ROOT/path).read_bytes()).hexdigest() for path in native_sources+[
            'game_shared/ballistic_curve_policy.h','game_shared/ballistic_flight.h','game_shared/ballistic_path_cache.h',
            'game_shared/performance_profile.h','game_shared/tests/verify_ballistics_k11416.py',
            'game_shared/tests/ballistic_curve_k11416_tests.cpp',
            'game_shared/tests/fixtures/ballistic_k11416/manager.cpp.in',
            'game_shared/tests/fixtures/j10_6_3/ballistic_manager_adapter.cpp.in',
            'tools/performance/extract_profile_csv.py']},
        'scope':'Independent RK4 accuracy; production curve/manager/BeginPenetration/save/time/damage methods; recording collision/entity services; Linux objects; ASan/UBSan O0/O2 and Windows min/max macro smoke check. No full native game/PhysX/Windows/GPU/FPS measurement.'}
    if args.json:args.json.parent.mkdir(parents=True,exist_ok=True);args.json.write_text(json.dumps(report,indent=2)+'\n')
    print(f"K.11.4.1.6: {report['passed']}/{report['total']} PASS",flush=True)
    return int(report['passed']!=report['total'])

if __name__=='__main__':raise SystemExit(main())
