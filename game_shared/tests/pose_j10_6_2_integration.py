#!/usr/bin/env python3
"""Compile unchanged production CPU/packet/draw methods with real project types.

Only engine services, render bookkeeping and IK scene locks use explicit adapters.
The Studio animation, IK solver, procedural, matrix and vector code are real.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

def function(text, signature):
    start=text.index(signature)
    brace=text.index('{',start)
    cursor=brace+1
    depth=1
    while depth:
        if text[cursor]=='{': depth+=1
        elif text[cursor]=='}': depth-=1
        cursor+=1
    return text[start:cursor]

def run(args):
    result=subprocess.run(args,cwd=ROOT,capture_output=True,text=True)
    if result.returncode:
        raise RuntimeError((result.stdout+result.stderr)[-7000:])
    if result.stdout.strip(): print(result.stdout.strip())

def main(extra_cases=None, ubsan=False):
    draw=(ROOT/'client/render/gl_studio_draw.cpp').read_text()
    init=(ROOT/'client/render/gl_studio_init.cpp').read_text()
    methods=[]
    methods.append(draw[draw.index('static physicsasset::CollisionPoseState StudioCollisionPoseState'):
        draw.index('static mstudioseqdesc_t* StudioCpuSequence')])
    methods.append(draw[draw.index('static mstudioseqdesc_t* StudioCpuSequence'):
        draw.index('int CStudioModelRenderer :: StudioGetBounds')])
    for signature in [
        'void CStudioModelRenderer :: StudioSetBonesExternal',
        'static bool RagdollSequenceNewer', 'static bool NormalizeRagdollQuat',
        'void CStudioModelRenderer :: StudioSetRagdollPose',
        'void CStudioModelRenderer::StudioSetPartPose',
        'void CStudioModelRenderer::UpdateLatchedVars',
        'void CStudioModelRenderer::StudioSetupBones',
        'void CStudioModelRenderer::StudioPublishCollisionPose',
        'mstudioanim_t *CStudioModelRenderer :: StudioGetAnim',
    ]: methods.append(function(draw,signature))
    for signature in [
        'CStudioModelRenderer :: CStudioModelRenderer',
        'CStudioModelRenderer :: ~CStudioModelRenderer',
        'CStudioModelRenderer::ModelInstance_t* CStudioModelRenderer::StudioEnsureCpuInstance',
        'void CStudioModelRenderer::StudioInitCpuInstance',
        'void CStudioModelRenderer :: DestroyInstance',
        'void CStudioModelRenderer :: DestroyAllModelInstances',
    ]: methods.append(function(init,signature))
    fixture=(ROOT/'game_shared/tests/fixtures/j10_6_2/pose_adapter.cpp.in').read_text()
    fixture=fixture.replace('@ADDITIONAL_CASES@', extra_cases.read_text() if extra_cases else '')
    fixture=fixture.replace('@RUN_ADDITIONAL_CASES@', 'additionalCases();' if extra_cases else '')
    with tempfile.TemporaryDirectory(prefix='j1062_pose_') as tmp:
        tmp=Path(tmp)
        unit=tmp/'pose.cpp'; unit.write_text(fixture.replace('@PRODUCTION@','\n\n'.join(methods)))
        binary=tmp/'pose'
        sources=['bone_setup.cpp','ikcontext.cpp','procbones.cpp','jigglebones.cpp',
            'vector.cpp','matrix.cpp','mathlib.cpp','stringlib.cpp']
        includes=['client','client/render','common','engine','game_shared','public']
        sanitizers=['-fsanitize=undefined','-fno-sanitize-recover=all'] if ubsan else []
        run([os.environ.get('CXX','c++'),*sanitizers,'-std=c++17','-O0','-g','-ffunction-sections','-fdata-sections',
            '-D_LINUX','-DLINUX','-DCLIENT_DLL','-DCLIENT_WEAPONS','-Dstricmp=strcasecmp',
            '-Dstrnicmp=strncasecmp','-D_snprintf=snprintf','-D_vsnprintf=vsnprintf',
            '-Wno-write-strings','-Wno-invalid offsetof'.replace(' ','-'),
            *['-I'+str(ROOT/p) for p in includes],str(unit),
            *[str(ROOT/'game_shared'/p) for p in sources],'-Wl,--gc-sections','-o',str(binary)])
        run([str(binary)])

if __name__=='__main__': main()
