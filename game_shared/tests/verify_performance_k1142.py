#!/usr/bin/env python3
"""Compile production client cache/uniform methods and real mesh/math code.
Engine/GL/SDK services are recording adapters; no native solver/GPU/FPS claim.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from pose_j10_6_2_integration import function

ROOT=Path(__file__).resolve().parents[2]
TESTS=ROOT/'game_shared/tests'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir',type=Path,required=True)
    parser.add_argument('--json',type=Path)
    args=parser.parse_args()
    work=args.work_dir.resolve();work.mkdir(parents=True,exist_ok=True)
    results=[]
    def run(label,command):
        result=subprocess.run([str(x) for x in command],cwd=ROOT,capture_output=True,text=True)
        (work/(label+'.log')).write_text(result.stdout+result.stderr)
        results.append({'test':label,'returncode':result.returncode})
        print(label,'PASS' if result.returncode==0 else 'FAIL',flush=True)
        if result.returncode:print((result.stdout+result.stderr)[-5000:],flush=True)
        elif result.stdout:print(result.stdout.strip(),flush=True)
        return result.returncode==0
    shader=(ROOT/'client/render/gl_shader.cpp').read_text()
    methods='\n\n'.join(function(shader,signature) for signature in [
        'int uniform_t :: GetSizeInBytes','bool uniform_t::CachedValue',
        'void uniform_t::RememberValue','void uniform_t::FreeValueCache','void uniform_t :: SetValue'])
    utils=(ROOT/'client/utils.cpp').read_text()
    prediction=utils[utils.index('static std::unordered_map<std::string, std::shared_ptr<physicsasset::AssetDef>>'):
        utils.index('void Physic_SweepTest(')]
    source=(TESTS/'fixtures/k11_4_2/client_performance.cpp.in').read_text()
    unit=work/'client.cpp';unit.write_text(source.replace('@UNIFORM_METHODS@',methods).replace('@PREDICTION_METHODS@',prediction))
    flags=['-std=c++17','-g','-fno-exceptions','-ffunction-sections','-fdata-sections','-fno-pie','-no-pie',
        '-Wno-write-strings','-Wno-invalid offsetof'.replace(' ','-'),'-DCLIENT_DLL=1','-DCLIENT_WEAPONS=1',
        '-D_LINUX=1','-DLINUX=1','-D_vsnprintf=vsnprintf','-D_snprintf=snprintf',
        '-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp']
    flags+=['-I'+str(ROOT/p) for p in ['client','client/render','common','engine','game_shared','public']]
    sources=[ROOT/'game_shared'/p for p in ['meshdesc.cpp','meshdesc_factory.cpp','virtualfs.cpp',
        'stringlib.cpp','matrix.cpp','mathlib.cpp','physics_asset_format.cpp']]
    for config,opt in [('debug','-O0'),('release','-O2')]:
        binary=work/('client_'+config)
        if run('client_'+config+'_compile',[os.environ.get('CXX','c++'),*flags,opt,
            '-fsanitize=address,undefined','-fno-sanitize-recover=all',unit,*sources,'-Wl,--gc-sections','-o',binary]):
            run('client_'+config,[binary])
    physics=(ROOT/'server/physx/physx_impl.cpp').read_text()
    header=(ROOT/'server/physx/physx_impl.h').read_text()
    helpers='\n\n'.join(function(physics,name) for name in ['static mstudiobodyparts_t *FindBodyPartByName', 'static int ResolveBodygroupVariant',
        'static bool CollisionSectionActive','static int FindStudioBoneIndex'])
    types='\n\n'.join(function(header,'struct '+name+'\n')+';' for name in
        ['PhysicsPartShape','PhysicsPartRuntime','PhysicsAssetCollisionState'])
    methods='\n\n'.join(function(physics,name) for name in [
        'void CPhysicPhysX::BuildAnimatedTraceKey','void CPhysicPhysX::RebuildAnimatedCollisionTraceMesh',
        'void CPhysicPhysX::SetAnimatedCollisionEnabled'])
    server=(TESTS/'fixtures/k11_4_2/server_performance.cpp.in').read_text()
    unit=work/'server.cpp';unit.write_text(server.replace('@STUDIO_HELPERS@',helpers).replace('@STATE_TYPES@',types)
        .replace('@PERFORMANCE_TYPE@',function(header,'struct PerformanceState')).replace('@SERVER_METHODS@',methods))
    for config,opt in [('debug','-O0'),('release','-O2')]:
        binary=work/('server_'+config)
        if run('server_'+config+'_compile',[os.environ.get('CXX','c++'),*flags,opt,
            '-I'+str(TESTS/'fixtures/j10_6_10_3'),'-fsanitize=address,undefined','-fno-sanitize-recover=all',
            unit,*sources,'-Wl,--gc-sections','-o',binary]):
            run('server_'+config,[binary])
    frame=(TESTS/'fixtures/k11_4_2/frame_performance.cpp.in').read_text()
    unit=work/'frame.cpp';unit.write_text(frame.replace('@PERFORMANCE_TYPE@',function(header,'struct PerformanceState'))
        .replace('@UPDATE_METHOD@',function(physics,'void CPhysicPhysX :: Update( float flTimeDelta )')))
    binary=work/'frame'
    if run('frame_compile',[os.environ.get('CXX','c++'),'-std=c++17','-O2','-fsanitize=address,undefined',
        '-fno-sanitize-recover=all','-fno-pie','-no-pie','-I'+str(ROOT/'game_shared'),unit,'-o',binary]):
        run('frame',[binary])
    driver=work/'pose_driver.py'
    driver.write_text('import sys\nfrom pathlib import Path\nsys.path.insert(0,'+repr(str(TESTS))+')\n'
        'from pose_j10_6_2_integration import main\nmain(Path('+repr(str(TESTS/'fixtures/k11_4_2/pose_cases.cpp.in'))+'),True)\n')
    run('pose_reuse',[sys.executable,driver])
    run('capture_restore',[sys.executable,TESTS/'ragdoll_f4_runtime_integration.py','--ubsan'])
    for name in ['physics_asset_trace_i102_source_tests.py','physics_asset_primitive_i104_source_tests.py',
        'physics_part_pose_cache_i105_source_tests.py','physics_collision_pose_i106_source_tests.py',
        'physics_asset_collision_ownership_i92_source_tests.py','physics_asset_cache_lifecycle_i107_source_tests.py']:
        run(Path(name).stem,[sys.executable,TESTS/name])
    summary={'milestone':'K.11.4.2','results':results,'passed':sum(r['returncode']==0 for r in results),
        'total':len(results),'scope':'Production methods, real mesh/math/Studio code, recording GL/engine/SDK services; no full SDK link, solver, GPU or in-game FPS test.'}
    if args.json:args.json.write_text(json.dumps(summary,indent=2)+'\n')
    print(f"K.11.4.2 performance checks: {summary['passed']}/{summary['total']} PASS",flush=True)
    return int(summary['passed']!=summary['total'])

if __name__=='__main__':raise SystemExit(main())
