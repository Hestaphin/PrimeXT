#!/usr/bin/env python3
"""K.11.4.1.3 sleep capture, presentation/network, impulses/save and client CPU tests.

Recording services and real project Studio/matrix code. Not a complete game,
native PhysX solver, actual GPU or FPS benchmark.
"""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
from ragdoll_f0_integration import function

ROOT=Path(__file__).resolve().parents[2]
FIXTURE=ROOT/'game_shared/tests/fixtures/sleep_k11413'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir',type=Path)
    parser.add_argument('--json',type=Path)
    parser.add_argument('--retained',action='store_true')
    args=parser.parse_args()
    temporary=tempfile.TemporaryDirectory(prefix='sleep-k11413-') if not args.work_dir else None
    work=args.work_dir.resolve() if args.work_dir else Path(temporary.name)
    work.mkdir(parents=True,exist_ok=True)
    compiler=shlex.split(os.environ.get('CXX','g++'))
    results=[]

    def run(name,command,env=None):
        result=subprocess.run([str(x) for x in command],cwd=ROOT,capture_output=True,text=True,env=env)
        (work/(name+'.log')).write_text(result.stdout+result.stderr)
        results.append({'name':name,'returncode':result.returncode})
        print(name,'PASS' if result.returncode==0 else 'FAIL',flush=True)
        if result.returncode:print((result.stdout+result.stderr)[-6500:],flush=True)
        return result.returncode==0

    runtime=(ROOT/'server/physx/physics_asset.cpp').read_text()
    header=(ROOT/'server/physx/physics_asset.h').read_text()
    impl=(ROOT/'server/physx/physx_impl.cpp').read_text()
    impl_header=(ROOT/'server/physx/physx_impl.h').read_text()
    methods='\n'.join(function(runtime,name) for name in [
        'void PhysicsAssetInstance::CaptureSimulationPose','bool PhysicsAssetInstance::IsSleeping',
        'bool PhysicsAssetInstance::CanFreeze','bool PhysicsAssetInstance::NeedsAnimationControl',
        'void PhysicsAssetInstance::Thaw','bool PhysicsAssetInstance::UpdateSleepFreeze',
        'void PhysicsAssetInstance::SetBodyKinematicState','bool PhysicsAssetInstance::AddImpulseToBody',
        'void PhysicsAssetInstance::AddRadialImpulse','bool PhysicsAssetInstance::GetRootBonePose',
        'bool PhysicsAssetInstance::GetSimulationBounds','bool PhysicsAssetInstance::SaveArticulatedBodies',
        'bool PhysicsAssetInstance::RestoreArticulatedBodies'])
    methods+='\n'+'\n'.join(function(impl,name) for name in [
        'static std::int16_t QuantizeRagdollRelativePosition','static std::int16_t QuantizeRagdollQuaternion',
        'void CPhysicPhysX::SendRagdollPose','void CPhysicPhysX::UpdateRagdollPresentation'])
    states='\n'.join(function(impl_header,'struct '+name+'\n')+';' for name in ['RagdollEntityState','RagdollNetworkState'])
    fixture=(FIXTURE/'runtime.cpp.in').read_text().replace('@STATES@',states).replace('@PRODUCTION@',methods)
    fixture=fixture.replace('@INVALIDATE@',function(header,'void InvalidateSimulationCapture'))
    fixture=fixture.replace('@BASELINE@',(FIXTURE/'baseline.cpp.in').read_text())
    unit=work/'runtime.cpp';unit.write_text(fixture)
    definitions=['-D_LINUX=1','-DLINUX=1','-D_vsnprintf=vsnprintf','-D_snprintf=snprintf','-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp']
    shared=['-I'+str(ROOT/p) for p in ['game_shared','common','engine','public','server']]
    flags=['-std=c++17','-fno-exceptions','-Wno-write-strings','-ffunction-sections','-fdata-sections',*definitions]
    math=[ROOT/'game_shared'/p for p in ['matrix.cpp','vector.cpp','mathlib.cpp','stringlib.cpp']]
    sanitize=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-pie','-no-pie']
    for optimization in ['O0','O2']:
        exe=work/('runtime_'+optimization)
        if run('runtime_compile_'+optimization,[*compiler,*flags,'-'+optimization,'-g',*sanitize,'-I'+str(FIXTURE),*shared,unit,*math,ROOT/'game_shared/performance_profile.cpp','-Wl,--gc-sections','-o',exe]):
            run('runtime_enabled_'+optimization,[exe])
            run('runtime_disabled_'+optimization,[exe,'profiling_off'])
    # Actual client CPU evaluator with the existing real Studio/IK/matrix fixture.
    client_driver=work/'client_driver.py'
    client_driver.write_text('import sys\nfrom pathlib import Path\nsys.path.insert(0,'+repr(str(ROOT/'game_shared/tests'))+')\nfrom pose_j10_6_2_integration import main\nmain(Path('+repr(str(FIXTURE/'client.cpp.in'))+'),ubsan=True)\n')
    wrapper=work/'profile_cxx'
    wrapper.write_text('#!'+sys.executable+'\nimport subprocess,sys\nargs=sys.argv[1:]\nroot='+repr(str(ROOT))+'\nargs += ["-I"+root+"/game_shared","-include",root+"/game_shared/performance_profile.h"]\nif not any(x in args for x in ["-c","-E","-fsyntax-only"]):\n    if not any(x.endswith("/performance_profile.cpp") for x in args): args.append(root+"/game_shared/performance_profile.cpp")\nsys.exit(subprocess.call('+repr(compiler)+'+args))\n')
    wrapper.chmod(0o755);environment=dict(os.environ,CXX=str(wrapper))
    run('client_cpu_sleep_ubsan',[sys.executable,client_driver],environment)
    for name in ['ragdoll_f4_runtime_integration.py','ragdoll_f3_physx_integration.py']:
        run(Path(name).stem,[sys.executable,ROOT/'game_shared/tests'/name,'--ubsan'],environment)
    run('death_partial_damage_and_impulses_ubsan',[sys.executable,ROOT/'game_shared/tests/physics_j10_6_3_integration.py'],dict(environment,J1063_UBSAN='1'))
    client_includes=['-I'+str(ROOT/p) for p in ['client','client/render','client/ui']]
    for optimization in ['O0','O2']:
        for name in ['gl_studio_draw','gl_studio_init']:
            run('native_'+name+'_'+optimization,[*compiler,*flags,'-'+optimization,'-DCLIENT_DLL=1','-DCLIENT_WEAPONS=1',*client_includes,*shared,'-c',ROOT/'client/render'/(name+'.cpp'),'-o',work/(name+'_'+optimization+'.o')])
    if args.retained:
        run('retained_k11412',[sys.executable,ROOT/'game_shared/tests/verify_pose_cache_k11412.py','--retained','--work-dir',work/'retained','--json',work/'retained.json'])
    report={'milestone':'K.11.4.1.3','passed':sum(x['returncode']==0 for x in results),'total':len(results),'results':results,
            'scope':'Native Linux objects, extracted production sleep/capture/presentation/network/impulse/save methods under ASan/UBSan at O0/O2; client CPU tests with real Studio/IK/matrix code. No full game/native PhysX solver/GPU/FPS run.'}
    if args.json:args.json.parent.mkdir(parents=True,exist_ok=True);args.json.write_text(json.dumps(report,indent=2)+'\n')
    print(f"K.11.4.1.3: {report['passed']}/{report['total']} PASS",flush=True)
    return int(report['passed']!=report['total'])


if __name__=='__main__':raise SystemExit(main())
