#!/usr/bin/env python3
"""K.11.4.1.5 renderer policy/method/driver checks and retained gameplay suites.

Recording GL/engine services, actual math/renderer methods and Linux objects.
No native GPU/game/Windows run or FPS benchmark.
"""
import argparse
import ast
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
from verify_map_transition import function

ROOT=Path(__file__).resolve().parents[3]
TESTS=ROOT/'client/render/tests'
FIXTURE=TESTS/'fixtures/render_budget_k11415'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir',type=Path);parser.add_argument('--json',type=Path)
    parser.add_argument('--retained',action='store_true');args=parser.parse_args()
    temporary=tempfile.TemporaryDirectory(prefix='render-budget-k11415-') if not args.work_dir else None
    work=args.work_dir.resolve() if args.work_dir else Path(temporary.name);work.mkdir(parents=True,exist_ok=True)
    compiler=shlex.split(os.environ.get('CXX','g++'));results=[]
    def run(name,command,env=None):
        result=subprocess.run([str(x) for x in command],cwd=ROOT,capture_output=True,text=True,env=env)
        (work/(name+'.log')).write_text(result.stdout+result.stderr)
        results.append({'name':name,'returncode':result.returncode})
        print(name,'PASS' if result.returncode==0 else 'FAIL',flush=True)
        if result.returncode:print((result.stdout+result.stderr)[-6500:],flush=True)
        return result.returncode==0
    common=['-std=c++17','-fno-exceptions','-Wno-write-strings','-ffunction-sections','-fdata-sections']
    sanitizers=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-pie','-no-pie']
    definitions=['-D_LINUX=1','-DLINUX=1','-DCLIENT_DLL=1','-DCLIENT_WEAPONS=1','-D_vsnprintf=vsnprintf',
        '-D_snprintf=snprintf','-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp']
    includes=['-I'+str(ROOT/p) for p in ['client','client/render','client/ui','game_shared','common','engine','public']]
    services=[ROOT/'game_shared/performance_profile.cpp',ROOT/'game_shared/performance_gpu.cpp']
    math=[ROOT/p for p in ['client/render/texture_handle.cpp','client/render/gl_viewport.cpp',
        'game_shared/stringlib.cpp','game_shared/matrix.cpp','game_shared/mathlib.cpp','game_shared/frustum.cpp']]
    base=(TESTS/'map_transition_harness.cpp.in').read_text().split('int main(int argc, char **argv) {')[0]
    base=base.replace('/* ASAN_DEFAULT_OPTIONS */','detect_leaks=0:halt_on_error=1')
    parsed=ast.parse((TESTS/'verify_j10_6_9_10.py').read_text())
    selection=next(ast.literal_eval(n.value) for n in ast.walk(parsed) if isinstance(n,ast.Assign) and
        any(isinstance(t,ast.Name) and t.id=='selection' for t in n.targets))
    chunks=[function((ROOT/path).read_text(),name) for path,names in selection.items() for name in names]
    chunks.append(function((ROOT/'client/render/gl_world_new.cpp').read_text(),'Mod_SetWorldSurfaceTextureFlags'))
    shader=(ROOT/'client/render/gl_shader.cpp').read_text()
    for name in ['SetShader','IsValid']:
        chunks.append(function(shader.replace('shader_t :: '+name,'shader_t_'+name),'shader_t_'+name).replace('shader_t_'+name,'shader_t::'+name))
    adapter=(TESTS/'subview_j9_10_harness.cpp.in').read_text()
    for name in ['R_AllocateSubviewTexture','GL_BindFBO','GL_BindFrameBuffer','R_FreeFrameBuffer','GL_Bind']:
        adapter=adapter.replace(function(adapter,name),'')
    adapter=adapter.replace(next(line for line in adapter.splitlines() if line.startswith('decltype(pglCopyTexImage2D)')),'')
    driver=(TESTS/'subview_alloc_k11_0_adapters.cpp.in').read_text()
    backend=(ROOT/'client/render/gl_backend.cpp').read_text()
    alloc='\n'.join(function(backend,name) for name in ['R_AllocFrameBuffer','R_FreeFrameBuffer','GL_BindFrameBuffer','GL_BindFBO'])
    alloc+='\n'+function((ROOT/'client/render/gl_subview.cpp').read_text(),'R_AllocateSubviewTexture')
    adapter=adapter.replace('std::string test = argv[1];','std::string test = argv[1]; SetupDriverApi();glState.frameBuffer=driverFramebuffer=77;')
    adapter=adapter.replace('if(test == "bsp_portal" || test == "bsp_monitor") {',
        'if(test.find("budget_")==0) { BudgetSubviewCase(test,surface,extra,parent,camera); } else if(test == "bsp_portal" || test == "bsp_monitor") {')
    pre,case=adapter.split('/* SUBVIEW_MAIN */')
    subview_base=base.replace('bool GL_Support(int) { return false; }',
        'static bool npot=true,fbo=true;bool GL_Support(int cap){return cap==R_ARB_TEXTURE_NPOT_EXT?npot:cap==R_FRAMEBUFFER_OBJECT?fbo:false;}')
    subview=subview_base.replace('/* PRODUCTION_FUNCTIONS */',pre+'\n'+driver+'\n'+alloc+'\n'+'\n'.join(chunks)+'\n'+(FIXTURE/'subviews.cpp.in').read_text())+'\n'+case
    origins={'PORTAL_ORIGIN':'-103.4,0.6,-40','DESTINATION_ORIGIN':'-106,890,-872','DESTINATION_ANGLES':'28.8,61.5,30',
        'MONITOR_ORIGIN':'-65,1083,-925','CAMERA_ORIGIN':'-188,764,-880','CAMERA_ANGLES':'4,64.1,0','PORTAL_TEXTURE_NAMES':'"PORTAL","MONITOR","REFLECT","SKY"'}
    for marker,value in origins.items():subview=subview.replace('/* '+marker+' */',value)
    subview_unit=work/'subview.cpp';subview_unit.write_text(subview)
    refmethods='\n'.join(function((ROOT/'client/render/gl_rmain.cpp').read_text(),name) for name in
        ['R_InitRefState','R_ClearFrameLists','R_ResetRefState','R_PushRefState','R_PopRefState','R_GetPrevInstance'])
    shadow_method=function((ROOT/'client/render/gl_shadows.cpp').read_text(),'R_RenderShadowmaps')
    shadow_fixture=(FIXTURE/'shadows.cpp.in').read_text().replace('@PRODUCTION@',refmethods+'\n'+shadow_method)
    shadow_base=base.replace('bool R_FullBright() { return true; }','bool R_FullBright() { return false; }')
    shadow_base=shadow_base.replace('bool GL_Support(int) { return false; }','bool GL_Support(int cap) { return cap==R_TEXTURECUBEMAP_EXT; }')
    shadow_unit=work/'shadows.cpp';shadow_unit.write_text(shadow_base.replace('/* PRODUCTION_FUNCTIONS */',shadow_fixture))
    subview_cases=['budget_monitor_30','budget_monitor_15','budget_fallback','budget_pin','budget_camera','budget_resize',
        'budget_failed_then_retry','budget_expire_reset','budget_reflection','budget_portal','budget_clones']
    shadow_cases=['shadow_atomic','shadow_cube','shadow_cache','shadow_slots','shadow_dynamic','shadow_feeds','shadow_brush','shadow_secondary','shadow_zero_legacy']
    for optimization in ['O0','O2']:
        core=work/('policies_'+optimization)
        if run('policies_compile_'+optimization,[*compiler,*common,'-'+optimization,*sanitizers,'-Wall','-Wextra','-Werror',
            '-I'+str(ROOT/'game_shared'),ROOT/'game_shared/tests/render_budget_k11415_tests.cpp','-o',core]):run('policies_run_'+optimization,[core])
        for name,unit,cases in [('subview',subview_unit,subview_cases),('shadows',shadow_unit,shadow_cases)]:
            exe=work/(name+'_'+optimization)
            if run(name+'_compile_'+optimization,[*compiler,*common,'-'+optimization,'-g',*sanitizers,*definitions,*includes,unit,
                *services,*math,'-Wl,--gc-sections','-o',exe]):
                for case in cases:run(case+'_'+optimization,[exe,case])
        for source in ['gl_subview','gl_shadows','gl_cvars','gl_rmisc','gl_world_new','gl_rmain']:
            run('native_'+source+'_'+optimization,[*compiler,*common,'-'+optimization,*definitions,*includes,
                '-Wno-invalid offsetof'.replace(' ','-'),'-c',ROOT/'client/render'/(source+'.cpp'),'-o',work/(source+'_'+optimization+'.o')])
        menu=work/('menu_'+optimization)
        if run('menu_compile_'+optimization,[*compiler,*common,'-'+optimization,*sanitizers,*definitions,*includes,
            '-I'+str(ROOT/'game_shared/tests/fixtures/scheduling_k11414'),FIXTURE/'menu.cpp',ROOT/'client/ui/ui_physics_menu.cpp','-o',menu]):run('menu_run_'+optimization,[menu])
    macro=work/'macros'
    if run('windows_macros_compile',[*compiler,*common,'-O2',*sanitizers,'-DRENDER_BUDGET_WINDOWS_MACROS',
        '-I'+str(ROOT/'game_shared'),ROOT/'game_shared/tests/render_budget_k11415_tests.cpp','-o',macro]):run('windows_macros_run',[macro])
    if args.retained:
        run('retained_k11414',[sys.executable,ROOT/'game_shared/tests/verify_scheduling_k11414.py','--retained',
            '--work-dir',work/'retained','--json',work/'retained.json'])
    report={'milestone':'K.11.4.1.5','passed':sum(x['returncode']==0 for x in results),'total':len(results),'results':results,
        'scope':'Actual renderer/matrix/texture types and methods, recording GL/engine services, Linux objects and ASan/UBSan O0/O2. No full game/native GPU/Windows/FPS measurement.'}
    if args.json:args.json.parent.mkdir(parents=True,exist_ok=True);args.json.write_text(json.dumps(report,indent=2)+'\n')
    print(f"K.11.4.1.5: {report['passed']}/{report['total']} PASS",flush=True)
    return int(report['passed']!=report['total'])

if __name__=='__main__':raise SystemExit(main())
