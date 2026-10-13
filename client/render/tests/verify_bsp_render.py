#!/usr/bin/env python3
"""Compile actual engine/client texture classifiers and sky-pass construction.

Real supplied BSP face/texture data is used. These are production-function
checks with engine/GL adapters; this does not render screenshots or run a game.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_map_transition import function


def bsp(path):
    data = path.read_bytes()
    assert struct.unpack_from('<i', data)[0] == 30, 'Expected BSP30'
    lumps = [struct.unpack_from('<ii', data, 4 + i * 8) for i in range(15)]
    assert all(off >= 0 and length >= 0 and off + length <= len(data) for off, length in lumps)
    offset, length = lumps[2]
    count = struct.unpack_from('<i', data, offset)[0]
    names = []
    for i in range(count):
        relative = struct.unpack_from('<i', data, offset + 4 + i * 4)[0]
        names.append(data[offset + relative:offset + relative + 16].split(b'\0')[0].decode('ascii')
                     if relative >= 0 else '')
    offset, length = lumps[6]
    info = [struct.unpack_from('<ii', data, offset + i + 32) for i in range(0, length, 40)]
    offset, length = lumps[7]
    faces = []
    for i in range(0, length, 20):
        texinfo = struct.unpack_from('<h', data, offset + i + 10)[0]
        texture, flags = info[texinfo]
        faces.append({'name': names[texture], 'texinfo_flags': flags})
    offset, length = lumps[0]
    entities = data[offset:offset + length].decode('latin1').rstrip('\0')
    return {'filename': path.name, 'sha256': hashlib.sha256(data).hexdigest(),
            'texture_names': names, 'faces': faces,
            'entity_classes': re.findall(r'"classname"\s+"([^"]+)"', entities)}


def legacy_client_flags(source):
    start = source.index('texture_t *tex = surf->texinfo->texture;',
                         source.index('static void Mod_LoadWorld('))
    end = source.index('if( FBitSet( surf->flags, SURF_REFLECT ))', start)
    return 'static void Mod_SetWorldSurfaceTextureFlags(msurface_t *surf)\n{\n' + source[start:end] + '\n}'


def legacy_engine_flags(source):
    start = source.index('texture_t\t*tex = out->texinfo->texture;',
                         source.index('static void Mod_LoadSurfaces('))
    end = source.index('Mod_CalcSurfaceBounds(', start)
    return 'static void Mod_SetSurfaceTextureFlags(msurface_t *out)\n{\n' + source[start:end] + '\n}'


def run(command, work, name):
    result = subprocess.run(command, capture_output=True, text=True)
    (work / (name + '.log')).write_text(result.stdout + result.stderr)
    return result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[3])
    p.add_argument('--xash-root', type=Path)
    p.add_argument('--mirror-map', type=Path, required=True)
    p.add_argument('--cubemap-map', type=Path, required=True)
    p.add_argument('--work-dir', type=Path, required=True)
    p.add_argument('--json', type=Path, required=True)
    p.add_argument('--expect-legacy-failures', action='store_true')
    a = p.parse_args()
    root = a.source_root.resolve()
    xash = (a.xash_root or root.parent / 'xash3d-fwgs-master').resolve()
    work = a.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=True)
    maps = [bsp(a.mirror_map), bsp(a.cubemap_map)]
    assert 'REFLECT' in maps[0]['texture_names']
    assert {'WATER1', 'SKY'}.issubset(maps[1]['texture_names'])
    assert maps[1]['entity_classes'].count('env_static') == 4
    assert 'env_sky' in maps[1]['entity_classes']
    records = maps[0]['faces'] + maps[1]['faces']
    source = (xash / 'engine/common/mod_bmodel.c').read_text()
    engine_flags = function(source, 'Mod_SetSurfaceTextureFlags', True) or legacy_engine_flags(source)
    encoded = ',\n'.join('{' + json.dumps(r['name']) + ',' + str(r['texinfo_flags']) + '}' for r in records)
    engine = '''#include "port.h"
#include "com_model.h"
#include "crtlib.h"
/* ENGINE_CONSTANTS */
#include <stdio.h>
#include <ctype.h>
static qboolean Host_IsQuakeCompatible(void) {return false;}
/* ENGINE_FUNCTIONS */
static struct {const char *name; int flags;} input[] = {/* BSP_INPUT */};
int main(void) {
    int errors=0;
    for(size_t i=0;i<sizeof(input)/sizeof(input[0]);++i) {
        texture_t original={0},lower={0};mtexinfo_t ti={0};msurface_t surf={0};
        Q_strncpy(original.name,input[i].name,sizeof(original.name));lower=original;
        for(size_t j=0;lower.name[j];++j)lower.name[j]=(char)tolower((unsigned char)lower.name[j]);
        ti.flags=input[i].flags;ti.texture=&original;surf.texinfo=&ti;
        Mod_SetSurfaceTextureFlags(&surf);int originalFlags=surf.flags;
        ti.texture=&lower;surf.flags=0;Mod_SetSurfaceTextureFlags(&surf);
        printf("%zu %d %d\\n",i,originalFlags,surf.flags);
        if(originalFlags!=surf.flags)++errors;
    }
    return errors?1:0;
}
'''.replace('/* ENGINE_FUNCTIONS */', function(source, 'Mod_LooksLikeWaterTexture') + '\n' + engine_flags)
    default_texture = re.search(r'^#define REF_DEFAULT_TEXTURE\s+.*$',
        (xash / 'engine/ref_api.h').read_text(), re.M).group()
    engine = engine.replace('/* BSP_INPUT */', encoded).replace('/* ENGINE_CONSTANTS */', default_texture)
    c = work / 'engine_classifier.c'
    c.write_text(engine)
    # The archive omits the shared generated build.h. Use the supplied PrimeXT
    # shared platform header; all model/surface/string headers are Xash's own.
    includes = ['-I' + str(xash / f) for f in ['common', 'public', 'engine', 'engine/common']]
    includes += ['-I' + str(root / 'public')]
    command = ['gcc', '-std=c11', '-O1', '-g', '-DHAVE_STRCASECMP=1', '-include', 'strings.h', '-fsanitize=address,undefined',
               '-fno-pie', '-no-pie', '-ffunction-sections', '-fdata-sections', *includes,
               str(c), str(xash / 'public/crtlib.c'), '-Wl,--gc-sections', '-o', str(work / 'engine_classifier')]
    # Runtime options are compiled in for managed runners which drop env vars.
    c.write_text('const char *__asan_default_options(void){return "detect_leaks=0:halt_on_error=1";}\n' + engine)
    compiled = run(command, work, 'engine_compile')
    if compiled.returncode:
        print(compiled.stderr);raise SystemExit(compiled.returncode)
    classified = run([str(work / 'engine_classifier')], work, 'engine_bsp_classification')
    rows = [line.split() for line in classified.stdout.splitlines() if re.match(r'^\d+ \d+ \d+$', line)]
    assert len(rows) == len(records)
    engine_actual = {r['name']: int(row[1]) for r, row in zip(records, rows)}

    template = Path(__file__).with_name('map_transition_harness.cpp.in').read_text()
    shader_body = '''#define GL_DEBUG_SCOPE() ((void)0)
ref_stats_t r_stats;
static std::vector<int> encodedNormals;
void GL_EncodeNormal(char *, TextureHandle texture) { encodedNormals.push_back(texture.ToInt()); }
static RefParams capturedSkyFlags=RP_NONE;
static Vector capturedSkyOrigin;
void GL_BindShader(glsl_program_t *shader) { RI->currentshader=shader; }
void R_SetupDynamicLights() {}
void R_RenderScene(const ref_viewpass_t *view, RefParams flags) {
    capturedSkyFlags=flags;capturedSkyOrigin=view->vieworigin;
}
'''
    template = template.replace('void GL_EncodeNormal(char *, TextureHandle) {}', shader_body)
    template = template.replace('/* ASAN_DEFAULT_OPTIONS */', 'detect_leaks=0:halt_on_error=1')
    selection = {
        'client/render/gl_world_new.cpp': ['Mod_ClearRenderContext', 'Mod_CopyMaterialDesc',
            'Mod_LoadWorldMaterials', 'Surf_CheckSubview', 'Mod_ShaderSceneForward', 'Mod_ShaderLightForward',
            'Mod_PrecacheShaders', 'Mod_DeleteBufferObject', 'Mod_ThrowModelInstances', 'Mod_FreeWorld', 'R_ProcessWorldData'],
        'client/render/gl_rsurf.cpp': ['R_TextureAnimation'],
        'client/render/gl_rmain.cpp': ['R_InitRefState', 'R_ClearFrameLists', 'R_ResetRefState',
            'R_PushRefState', 'R_PopRefState', 'HUD_BuildLightmaps'],
        'client/render/gl_decals.cpp': ['ClearDecals'],
        'client/render/gl_sky.cpp': ['R_DrawSkyPortal']}
    chunks = [function((root / path).read_text(), n, n=='Mod_ClearRenderContext')
              for path, names in selection.items() for n in names]
    world = (root / 'client/render/gl_world_new.cpp').read_text()
    chunks.append(function(world, 'Mod_SetWorldSurfaceTextureFlags', True) or legacy_client_flags(world))
    shader = (root / 'client/render/gl_shader.cpp').read_text()
    for name in ['SetShader', 'IsValid']:
        normalized = shader.replace('shader_t :: ' + name, 'shader_t_' + name)
        chunks.append(function(normalized, 'shader_t_' + name).replace('shader_t_' + name, 'shader_t::' + name))
    template = template.replace('/* PRODUCTION_FUNCTIONS */', '\n\n'.join(chunks))
    test_inputs = ','.join('{' + json.dumps(n) + ',' + str(flags) + '}' for n, flags in engine_actual.items())
    extra = r'''    } else if (test=="bsp_mirror_features" || test=="bsp_water_materials" || test=="bsp_sky_features") {
        skipShader=true;
        const struct {const char *name;int flags;} inputs[]={/* MAP_TEXTURE_FLAGS */};
        for (const auto &input:inputs) {
            WorldFixture f;f.model.numtextures=1;
            std::strncpy(f.texture.name,input.name,sizeof(f.texture.name)-1);f.Load();
            f.surface.flags=input.flags;Mod_SetWorldSurfaceTextureFlags(&f.surface);
            if(test=="bsp_mirror_features" && !std::strcmp(input.name,"REFLECT")) {
                assert(f.surface.flags&SURF_REFLECT);assert(world->features&WORLD_HAS_MIRRORS);
                assert(world->materials[0].flags&BRUSH_REFLECT);
            }
            if(test=="bsp_water_materials" && !std::strcmp(input.name,"WATER1")) {
                assert(f.surface.flags&SURF_DRAWTURB);assert(world->materials[0].flags&BRUSH_LIQUID);
                assert(world->materials[0].impl->reflectScale==1.0f);
                assert(world->materials[0].impl->refractScale==2.5f);
            }
            if(test=="bsp_sky_features" && !std::strcmp(input.name,"SKY")) {
                assert(f.surface.flags&SURF_DRAWSKY);assert(world->features&WORLD_HAS_SKYBOX);
            }
            assert(!std::strcmp(f.texture.name,input.name));f.Unload();
        }
    } else if(test=="sky_default_cubemap" || test=="sky_reflection" || test=="sky_normal") {
        RI->params = test=="sky_default_cubemap" ? RP_DRAW_WORLD|RP_SKYVIEW :
            test=="sky_reflection" ? RP_MIRRORVIEW|RP_CLIPPLANE|RP_MERGE_PVS|RP_WATERPASS : RP_DRAW_WORLD;
        const auto saved=RI->params;
        RI->view.origin=Vector(128,64,32);RI->view.fov_x=90;RI->view.fov_y=60;
        RI->view.port=CViewport(0,0,640,480);
        cl_entity_t camera={};camera.curstate.scale=64;camera.curstate.origin=Vector(-54,889,-618);
        R_DrawSkyPortal(&camera);
        assert(RI->params==saved && glState.stack_position==0);
        assert(capturedSkyFlags&RP_SKYPORTALVIEW);assert(!(capturedSkyFlags&RP_CLIPPLANE));
        if(test=="sky_default_cubemap") {
            assert(!(capturedSkyFlags&RP_SKYVIEW));assert(capturedSkyFlags&RP_ENVVIEW);
        }
        if(test=="sky_reflection")assert((capturedSkyFlags&(RP_MIRRORVIEW|RP_WATERPASS))==(RP_MIRRORVIEW|RP_WATERPASS));
        assert(capturedSkyOrigin==Vector(-52,890,-617.5f));
    } else if(test=="water_normal_scene" || test=="water_normal_light") {
        skipShader=true;WorldFixture f;f.model.numtextures=1;std::strcpy(f.texture.name,"water1");f.Load();
        texture_t normal={};normal.gl_texturenum=42;
        world->materials[0].impl->gl_normalmap_id=TextureHandle(&normal);
        world->materials[0].flags|=BRUSH_HAS_BUMP;
        cl_entity_t entity={};engineEntity=&entity;RI->currententity=&entity;
        if(test=="water_normal_scene")Mod_ShaderSceneForward(&f.surface);
        else {CDynLight light;light.type=LIGHT_SPOT;Mod_ShaderLightForward(&light,&f.surface);}
        assert(encodedNormals.size()==1 && encodedNormals[0]==42);f.Unload();
    } else {
        assert(false && "Unknown case");
'''.replace('/* MAP_TEXTURE_FLAGS */', test_inputs).replace('\\"', '"')
    template = template.replace('    } else {\n        assert(false && "Unknown case");\n', extra)
    cpp = work / 'client_render.cpp';cpp.write_text(template)
    command_cpp = ['g++', '-std=c++17', '-O1', '-g', '-fno-pie', '-no-pie', '-fsanitize=address,undefined',
        '-ffunction-sections', '-fdata-sections', '-Wno-write-strings', '-DCLIENT_DLL=1', '-DCLIENT_WEAPONS=1',
        '-D_LINUX=1', '-DLINUX=1', '-D_vsnprintf=vsnprintf',
        *['-I' + str(root / f) for f in ['client', 'client/render', 'common', 'engine', 'game_shared', 'public']],
        str(cpp), *[str(root / f) for f in ['client/render/texture_handle.cpp', 'game_shared/stringlib.cpp',
        'game_shared/matrix.cpp', 'game_shared/mathlib.cpp', 'client/render/gl_viewport.cpp', 'game_shared/frustum.cpp']],
        '-Wl,--gc-sections', '-o', str(work / 'client_render')]
    compiled = run(command_cpp, work, 'client_compile')
    if compiled.returncode:
        print(compiled.stderr);raise SystemExit(compiled.returncode)
    results = {'engine_bsp_classification': classified.returncode==0}
    for case in ['bsp_mirror_features','bsp_water_materials','bsp_sky_features', 'sky_default_cubemap',
                 'sky_reflection','sky_normal','water_normal_scene','water_normal_light']:
        result = run([str(work / 'client_render'), case], work, case)
        results[case] = result.returncode==0
    fail_cases = set(results)-{'sky_reflection','sky_normal'}
    accepted = all(passed == (not a.expect_legacy_failures or case not in fail_cases) for case, passed in results.items())
    a.json.parent.mkdir(parents=True, exist_ok=True)
    a.json.write_text(json.dumps({'accepted':accepted,'cases':results,'maps':maps,
        'engine_classified_faces':len(rows),'engine_texture_flags':engine_actual,
        'compile_commands':[command,command_cpp],
        'scope':'Production classifiers/material/shader/sky-pass bodies; real BSP data; no GPU game run'},indent=2)+'\n')
    for case,passed in results.items():print(case+': '+('PASS' if passed else 'FAIL'))
    if not accepted:
        for case,passed in results.items():
            if not passed:print((work/(case+'.log')).read_text()[:2500])
    raise SystemExit(0 if accepted else 1)


if __name__=='__main__':main()
