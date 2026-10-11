#!/usr/bin/env python3
"""Execute production player selection, model lifecycle and render bookkeeping.

PrimeXT uses real model/entity/Studio types, animation and math. GPU allocation,
engine services and PhysX calls are explicit adapters, not full engine builds.
--source-tree selects another PrimeXT source tree for before/after reproduction.
"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import tempfile
from pose_j10_6_2_integration import function

TESTS = Path(__file__).resolve().parent

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-tree', type=Path, default=TESTS.parents[1])
    parser.add_argument('--ubsan', action='store_true')
    parser.add_argument('--record', type=Path)
    args = parser.parse_args()
    root = args.source_tree.resolve()
    draw = (root/'client/render/gl_studio_draw.cpp').read_text()
    init = (root/'client/render/gl_studio_init.cpp').read_text()
    methods = [draw[draw.index('static physicsasset::CollisionPoseState StudioCollisionPoseState'):
        draw.index('int CStudioModelRenderer :: StudioGetBounds')]]
    methods += [function(draw, s) for s in [
        'void CStudioModelRenderer::UpdateLatchedVars',
        'void CStudioModelRenderer::StudioSetupBones',
        'bool CStudioModelRenderer :: CheckBoneCache',
        'mstudioanim_t *CStudioModelRenderer :: StudioGetAnim']]
    methods += [function(init, s) for s in [
        'CStudioModelRenderer :: CStudioModelRenderer',
        'CStudioModelRenderer :: ~CStudioModelRenderer',
        'CStudioModelRenderer::ModelInstance_t* CStudioModelRenderer::StudioEnsureCpuInstance',
        'bool CStudioModelRenderer::StudioSetupInstance',
        'void CStudioModelRenderer::StudioInitCpuInstance',
        'void CStudioModelRenderer :: DestroyInstance',
        'void CStudioModelRenderer :: DestroyAllModelInstances',
        'void CStudioModelRenderer :: ProcessUserData']]
    rmain = (root/'client/render/gl_rmain.cpp').read_text()
    has_report = 'void R_RenderReport' in rmain
    methods += [function(rmain, 'void R_InitRefState')]
    if has_report:
        methods += ['#define J1067_RENDER_REPORT',function(rmain,'bool R_SkyIsVisible'),
            function(rmain,'void R_RenderReport')]
    adapter = (TESTS/'fixtures/j10_6_2/pose_adapter.cpp.in').read_text().split('int main()')[0]
    adapter = adapter.replace(function(adapter, 'bool CStudioModelRenderer::CheckBoneCache'), '')
    adapter = adapter.replace('@ADDITIONAL_CASES@', '')
    adapter = '#include <cstdarg>\n#include <cstdio>\n'+adapter
    adapter = adapter.replace('void ALERT(ALERT_TYPE,char*,...) {}',
        'static std::string capturedReport;\n'
        'void ALERT(ALERT_TYPE,char* format,...) {char text[2048];va_list args;'
        'va_start(args,format);std::vsnprintf(text,sizeof(text),format,args);'
        'va_end(args);capturedReport+=text;}')
    # Extra declarations precede the extracted production implementations.
    extra = (TESTS/'fixtures/j10_6_7/render_adapter.cpp.in').read_text()
    declarations, cases = extra.split('@CASES@')
    unit = adapter.replace('@PRODUCTION@', declarations+'\n'+'\n\n'.join(methods)) + cases
    records = []
    with tempfile.TemporaryDirectory(prefix='j1067_render_') as directory:
        directory = Path(directory)
        source = directory/'render.cpp'; source.write_text(unit)
        binary = directory/'render'
        includes = ['client','client/render','common','engine','game_shared','public']
        math = ['bone_setup.cpp','ikcontext.cpp','procbones.cpp','jigglebones.cpp',
            'vector.cpp','matrix.cpp','mathlib.cpp','stringlib.cpp']
        sanitizers = ['-fsanitize=undefined','-fno-sanitize-recover=all'] if args.ubsan else []
        command = [os.environ.get('CXX','c++'),*sanitizers,'-std=c++17','-O0','-g',
            '-ffunction-sections','-fdata-sections','-D_LINUX','-DLINUX','-DCLIENT_DLL',
            '-DCLIENT_WEAPONS','-Dstricmp=strcasecmp','-Dstrnicmp=strncasecmp',
            '-D_snprintf=snprintf','-D_vsnprintf=vsnprintf','-Wno-write-strings',
            '-Wno-invalid offsetof'.replace(' ','-'),*['-I'+str(root/p) for p in includes],
            str(source),*[str(root/'game_shared'/p) for p in math],'-Wl,--gc-sections','-o',str(binary)]
        build = subprocess.run(command, capture_output=True, text=True, cwd=root)
        if build.returncode: raise RuntimeError((build.stdout+build.stderr)[-7000:])
        names = ['player-index','model-context','model-null','model-no-buffer',
            'instance-cubemap','physical-gl','coincident-cube','initial-cube','sky-model']
        if has_report: names += ['report-menu','report-map']
        for name in names:
            result = subprocess.run([str(binary),name], capture_output=True, text=True, cwd=directory)
            output = (result.stdout+result.stderr).strip()
            records.append({'case':name,'returncode':result.returncode,'output':output})
            print(('PASS ' if result.returncode == 0 else 'FAIL ')+name+': '+output[-1200:], flush=True)
    if args.record:
        args.record.parent.mkdir(parents=True,exist_ok=True)
        args.record.write_text(json.dumps(records,indent=2)+'\n')
    return 0 if all(r['returncode']==0 for r in records) else 1

if __name__ == '__main__': raise SystemExit(main())
