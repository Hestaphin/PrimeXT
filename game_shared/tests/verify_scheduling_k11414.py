#!/usr/bin/env python3
"""K.11.4.1.4 fixed scheduling, drives, menu and production interpolation checks.

Linux objects, project Studio/matrix code and recording API services. Does not
link a full game, certify real PhysX contacts/solver stability or measure FPS.
"""
import argparse
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
FIXTURE=ROOT/'game_shared/tests/fixtures/scheduling_k11414'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir',type=Path)
    parser.add_argument('--json',type=Path)
    parser.add_argument('--retained',action='store_true')
    args=parser.parse_args()
    temporary=tempfile.TemporaryDirectory(prefix='scheduling-k11414-') if not args.work_dir else None
    work=args.work_dir.resolve() if args.work_dir else Path(temporary.name)
    work.mkdir(parents=True,exist_ok=True)
    compiler=shlex.split(os.environ.get('CXX','g++'))
    results=[]
    def run(name,command,env=None):
        result=subprocess.run([str(x) for x in command],cwd=ROOT,capture_output=True,text=True,env=env)
        (work/(name+'.log')).write_text(result.stdout+result.stderr)
        results.append({'name':name,'returncode':result.returncode})
        print(name,'PASS' if result.returncode==0 else 'FAIL',flush=True)
        if result.returncode:print((result.stdout+result.stderr)[-6000:],flush=True)
        return result.returncode==0
    common=['-std=c++17','-fno-exceptions','-Wno-write-strings']
    sanitize=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-pie','-no-pie']
    strict=['-Wall','-Wextra','-Werror','-Wpedantic']
    shared=['-I'+str(ROOT/'game_shared')]
    impl=(ROOT/'server/physx/physx_impl.cpp').read_text()
    physics=work/'physics.cpp'
    physics.write_text((FIXTURE/'physics.cpp.in').read_text().replace('@PRODUCTION@',function(impl,'void CPhysicPhysX :: Update(')))
    held=(ROOT/'server/physx/holdable_item_controller.cpp').read_text()
    holdable=work/'holdable.cpp'
    holdable.write_text((FIXTURE/'holdable.cpp.in').read_text().replace('@CONSTANTS@',function(held,'namespace\n'))
        .replace('@PRODUCTION@',function(held,'void HoldableItemController::ApplyForces')))
    environment=work/'environment.cpp'
    math=(ROOT/'game_shared/mathlib.h').read_text()
    water_constants='\n'.join(line for line in math.splitlines() if line.startswith('#define METERS_PER_INCH'))+'\n'
    water_constants+='\n'.join(function(math,s) for s in ['constexpr inline float CubicInchesPerCubicMeter',
        'constexpr inline float MetricDensityToEngine'])+'\n'
    water_constants+='\n'.join(line for line in impl.splitlines() if line.startswith('constexpr float k_Water'))
    environment.write_text((FIXTURE/'environment.cpp.in').read_text().replace('@CONSTANTS@',water_constants)
        .replace('@PRODUCTION@','\n'.join(function(impl,s) for s in [
            'physx::PxBounds3 CPhysicPhysX::GetIntersectionBounds','void CPhysicPhysX :: AddForce',
            'void CPhysicPhysX :: AddTorque','void CPhysicPhysX::ApplyWaterForces'])))
    runtime=(ROOT/'server/physx/physics_asset.cpp').read_text()
    combat=(ROOT/'server/combat.cpp').read_text();weapons=(ROOT/'server/weapons.cpp').read_text()
    methods=[function(runtime,s) for s in ['bool PhysicsAssetInstance::NeedsAnimationControl',
        'void PhysicsAssetInstance::SetBodyKinematicState','bool PhysicsAssetInstance::IsBodyDescendantOf',
        'bool PhysicsAssetInstance::SetBodyPhysicsWeight','void PhysicsAssetInstance::ApplyAnimationControl',
        'bool PhysicsAssetInstance::AddImpulseToBody','bool PhysicsAssetInstance::SetRagdoll']]
    methods += [function(impl,s) for s in ['void CPhysicPhysX::SuspendEntityForRagdoll',
        'bool CPhysicPhysX::SetRagdollBodyPhysicsWeight','void CPhysicPhysX::TransferPendingRagdollImpulses',
        'void CPhysicPhysX::SetRagdoll(','void CPhysicPhysX::QueuePhysicsAssetImpulse',
        'bool CPhysicPhysX::TraceBallisticPhysicsExit']]
    methods += [function(weapons,'void ApplyPhysicsBulletImpulse'),function(combat,'void CBaseMonster :: Killed')]
    partial=work/'partial.cpp'
    template=(ROOT/'game_shared/tests/fixtures/j10_6_3/physics_adapter.cpp.in').read_text()
    template=template.replace('int main(){deathCases();shapeCases();}',(FIXTURE/'partial.cpp.in').read_text()+'\nint main(){stepDriveCases();deathCases();shapeCases();}\n')
    partial.write_text(template.replace('@PRODUCTION@','\n'.join(methods)))
    definitions=['-D_LINUX=1','-DLINUX=1','-D_vsnprintf=vsnprintf','-D_snprintf=snprintf','-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp']
    client=['-I'+str(ROOT/p) for p in ['client','client/ui','client/render','common','engine','public']]
    for optimization in ['O0','O2']:
        for name,source,extras,flags in [
            ('scheduler',ROOT/'game_shared/tests/physics_step_k11414_tests.cpp',[],strict),
            ('physics',physics,[ROOT/'game_shared/performance_profile.cpp'],strict),
            ('holdable',holdable,[],strict),
            ('environment',environment,[],strict),
            ('partial',partial,[ROOT/'game_shared/performance_profile.cpp'],[]),
            ('menu',FIXTURE/'menu.cpp',[ROOT/'client/ui/ui_physics_menu.cpp'],
                [*definitions,'-DCLIENT_DLL=1','-DCLIENT_WEAPONS=1',*client,'-I'+str(FIXTURE)])]:
            exe=work/(name+'_'+optimization)
            if run(name+'_compile_'+optimization,[*compiler,*common,'-'+optimization,'-g',*sanitize,*flags,*shared,source,*extras,'-o',exe]):
                run(name+'_run_'+optimization,[exe])
    exe=work/'windows_macros'
    if run('windows_macros_compile',[*compiler,*common,'-O2',*strict,*sanitize,*shared,'-DSCHEDULER_WINDOWS_MACROS',ROOT/'game_shared/tests/physics_step_k11414_tests.cpp','-o',exe]):
        run('windows_macros_run',[exe])
    # Real engine/client types with a recording ImGui API, including window link
    # and the production menu translation unit. Full ImGui linking is separate.
    for optimization in ['O0','O2']:
        run('client_ui_registration_'+optimization,[*compiler,*common,'-'+optimization,*definitions,
            '-DCLIENT_DLL=1','-DCLIENT_WEAPONS=1',*client,*shared,'-c',ROOT/'client/ui/ui_main.cpp','-o',work/('ui_main_'+optimization+'.o')])
        server=['-I'+str(ROOT/p) for p in ['server','server/entities','server/monsters','server/weapons',
            'server/gamerules','common','engine','public','game_shared']]
        for source in ['server/physic.cpp','server/entities/trigger_push.cpp','server/entities/gibshooter.cpp']:
            name=Path(source).stem+'_'+optimization
            run('native_'+name,[*compiler,*common,'-'+optimization,*definitions,'-Wno-invalid offsetof'.replace(' ','-'),
                *server,'-c',ROOT/source,'-o',work/(name+'.o')])
    wrapper=work/'profile_cxx'
    wrapper.write_text('#!'+sys.executable+'\nimport subprocess,sys\nargs=sys.argv[1:]\nroot='+repr(str(ROOT))+'\nargs += ["-I"+root+"/game_shared","-include",root+"/game_shared/performance_profile.h"]\nif not any(x in args for x in ["-c","-E","-fsyntax-only"]):\n    if not any(x.endswith("/performance_profile.cpp") for x in args): args.append(root+"/game_shared/performance_profile.cpp")\nsys.exit(subprocess.call('+repr(compiler)+'+args))\n')
    wrapper.chmod(0o755)
    client_driver=work/'client_driver.py'
    client_driver.write_text('import sys\nfrom pathlib import Path\nsys.path.insert(0,'+repr(str(ROOT/'game_shared/tests'))+')\nfrom pose_j10_6_2_integration import main\nmain(Path('+repr(str(FIXTURE/'client.cpp.in'))+'),ubsan=True)\n')
    run('studio_interpolation_ubsan',[sys.executable,client_driver],dict(os.environ,CXX=str(wrapper)))
    spec=importlib.util.spec_from_file_location('extractor',ROOT/'tools/performance/extract_profile_csv.py')
    extractor=importlib.util.module_from_spec(spec);spec.loader.exec_module(extractor)
    rows=extractor.extract('PERF_K11411,server,1,physics_hz,hz,10,60,60,60,60,60,10\nPERF_K11411,server,1,physics_dropped_time,ms,10,5,0,5,10,10,10\n')
    results.append({'name':'scheduler_csv_units','returncode':0 if len(rows)==2 else 1})
    print('scheduler_csv_units','PASS' if len(rows)==2 else 'FAIL',flush=True)
    # Whole-world teardown and explicit restore both reach the generation/reset
    # paths exercised above; catch accidental omission of production wiring.
    cache=(ROOT/'server/studio_pose_cache.cpp').read_text();base=(ROOT/'server/cbase.cpp').read_text()
    wiring='m_stepScheduler.Reset();' in function(impl,'void CPhysicPhysX::ClearWorldAssetCaches') and \
        'ClearServerStudioPoseCache();' in function(base,'int CBaseEntity::Restore') and \
        '++g_generation' in cache
    results.append({'name':'world_restore_reset_wiring','returncode':0 if wiring else 1})
    print('world_restore_reset_wiring','PASS' if wiring else 'FAIL',flush=True)
    if args.retained:
        run('retained_k11413',[sys.executable,ROOT/'game_shared/tests/verify_sleep_k11413.py','--retained','--work-dir',work/'retained','--json',work/'retained.json'])
    report={'milestone':'K.11.4.1.4','passed':sum(x['returncode']==0 for x in results),'total':len(results),'results':results,
        'scope':'Linux objects and actual scheduling/drive/menu/Studio methods, recording scene/body/ImGui services, ASan/UBSan O0/O2. No full game/real solver/Windows link/GPU/FPS measurement.'}
    if args.json:args.json.parent.mkdir(parents=True,exist_ok=True);args.json.write_text(json.dumps(report,indent=2)+'\n')
    print(f"K.11.4.1.4: {report['passed']}/{report['total']} PASS",flush=True)
    return int(report['passed']!=report['total'])

if __name__=='__main__':raise SystemExit(main())
