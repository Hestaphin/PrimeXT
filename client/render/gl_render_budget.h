#pragma once
// Included after the renderer types. No timers or synchronous GPU queries.
#include "gl_local.h"
#include "render_budget.h"
#include "exact_geometry_cache.h"
#include "performance_profile.h"
#include "performance_gpu.h"
#include "gl_world.h"
#include "cvardef.h"
#include "brush_material.h"
#include <array>
#include <cstring>

inline float R_BudgetValue(const char *name,float fallback)
{
    cvar_t *value=gEngfuncs.pfnGetCvarPointer ? gEngfuncs.pfnGetCvarPointer(name) : nullptr;
    return value ? value->value : fallback;
}
struct RenderSubviewCache
{
    const msurface_t *surface=nullptr;
    posecache::Stamp stamp;
    int slot=0,seen=-1;
    double rendered=0;
};
struct RenderShadowCache
{
    posecache::Stamp stamp;
    TextureHandle texture[MAX_SHADOWMAPS];
    matrix4x4 matrix[MAX_SHADOWMAPS];
    float glMatrix[MAX_SHADOWMAPS][16]{};
    int slot=-1;
    std::uint64_t write=0;
};
struct RenderBudgetState
{
    // Pinned subviews leave half the existing pool for current-view rendering.
    std::array<RenderSubviewCache,MAX_SUBVIEW_TEXTURES/2> subviews;
    std::array<RenderShadowCache,MAX_DLIGHTS> shadows;
    std::array<std::uint64_t,MAX_SHADOWS> writes2D{},writesCube{};
    std::uint64_t sunWrite=0;
    posecache::Stamp shadowScene;
    std::uint64_t sceneRevision=1;
    renderbudget::PassBudget mainShadows,secondaryShadows;
    int frame=-1;
    double time=0;
    const model_t *model=nullptr;
    bool dimensionsValid=false;
    float reflectionScale=1,monitorSize=1024,hdr=0;
};
inline RenderBudgetState &R_RenderBudget() { static RenderBudgetState state;return state; }
inline void R_ResetRenderBudgetCache()
{
    auto &state=R_RenderBudget();
    for(auto &entry:state.subviews) entry=RenderSubviewCache{};
    for(auto &entry:state.shadows) entry.stamp.Invalidate();
    state.shadowScene.Invalidate();++state.sceneRevision;
    for(auto &write:state.writes2D) ++write;
    for(auto &write:state.writesCube) ++write;
    ++state.sunWrite;state.frame=-1;state.dimensionsValid=false;
    for(auto &texture:tr.subviewTextures) texture.cachePinned=false;
}
inline void R_FreeBudgetSubviewPool()
{
    for(int i=0;i<tr.num_subview_used;++i)
        if(tr.subviewTextures[i].texturenum.Initialized()) FREE_TEXTURE(tr.subviewTextures[i].texturenum);
    for(int i=0;i<tr.num_framebuffers;++i) if(tr.frame_buffers[i].init) R_FreeFrameBuffer(i);
    std::memset(tr.subviewTextures,0,sizeof(tr.subviewTextures));
    tr.num_subview_used=tr.num_framebuffers=0;
}
inline void R_BeginRenderBudgetFrame()
{
    auto &state=R_RenderBudget();
    if(state.model!=worldmodel || tr.realframecount<state.frame || tr.time+1e-6<state.time)
        R_ResetRenderBudgetCache();
    const float scale=renderbudget::ReflectionScale(R_BudgetValue("r_reflection_scale",1));
    const float size=float(renderbudget::MonitorSize(R_BudgetValue("r_monitor_size",1024)));
    const float hdr=R_BudgetValue("gl_hdr",0);
    if(state.dimensionsValid && (scale!=state.reflectionScale || size!=state.monitorSize || hdr!=state.hdr))
    {
        // Configuration changes happen between displayed frames. Never free a
        // texture while a recursive view is still rendering into the pool.
        if(glState.stack_position==0) { R_FreeBudgetSubviewPool();R_ResetRenderBudgetCache(); }
    }
    state.dimensionsValid=true;state.reflectionScale=scale;state.monitorSize=size;state.hdr=hdr;
    if(state.frame!=tr.realframecount)
    {
        state.mainShadows.Reset();state.secondaryShadows.Reset();
        for(auto &entry:state.subviews)
        {
            if(entry.slot && entry.seen+2<tr.realframecount)
            {
                tr.subviewTextures[entry.slot-1].cachePinned=false;
                entry=RenderSubviewCache{};
            }
        }
    }
    state.frame=tr.realframecount;state.time=tr.time;state.model=worldmodel;
}
inline void R_BudgetBytes(posecache::Key &key,const void *data,std::size_t bytes)
{
    const auto *p=static_cast<const unsigned char *>(data);
    for(std::size_t i=0;i<bytes;i+=8)
    {
        std::uint64_t value=0;std::memcpy(&value,p+i,(std::min)(std::size_t(8),bytes-i));key.Add(value);
    }
}
inline void R_BudgetMatrix(posecache::Key &key,const matrix4x4 &matrix)
{ float values[16];matrix.CopyToArray(values);R_BudgetBytes(key,values,sizeof(values)); }
inline posecache::Key R_BudgetSceneKey(bool shadow)
{
    posecache::Key key;key.Pointer(worldmodel);key.Add(tr.glsl_valid_sequence);
    key.Add(world ? world->features : 0);key.Add(tr.num_draw_entities);
    bool live=!shadow || R_BudgetValue("r_shadow_particles",1)>0;
    // Alpha-tested depth shaders can sample subview/movie images. Their pixel
    // contents have no revision here, so these worlds keep fresh shadow draws.
    bool unknownGeometry=world && FBitSet(world->features,shadow ?
        (WORLD_HAS_SCREENS|WORLD_HAS_PORTALS|WORLD_HAS_MIRRORS|WORLD_HAS_MOVIES) :
        (WORLD_HAS_SCREENS|WORLD_HAS_PORTALS|WORLD_HAS_MOVIES));
    if(world && FBitSet(world->features,WORLD_HAS_GRASS) && R_BudgetValue("r_grass",1)>0 &&
        (!shadow || R_BudgetValue("r_grass_shadows",1)>0)) live=true;
    if(worldmodel)
    {
        key.Pointer(worldmodel->surfaces);key.Add(worldmodel->numsurfaces);
        for(int i=0;worldmodel->textures && i<worldmodel->numtextures;++i)
        {
            const texture_t *texture=worldmodel->textures[i];key.Pointer(texture);
            if(!texture) continue;
            key.Add(texture->gl_texturenum);key.Pointer(texture->material);
            if(texture->material) key.Add(texture->material->flags);
            if(texture->anim_total || texture->alternate_anims) live=true;
            if(texture->material && FBitSet(texture->material->flags,BRUSH_LIQUID|BRUSH_CONVEYOR)) live=true;
        }
    }
    for(int i=0;i<tr.num_draw_entities;++i)
    {
        const cl_entity_t *entity=tr.draw_entities[i];key.Pointer(entity);
        if(!entity || !entity->model) { unknownGeometry=true;continue; }
        key.Pointer(entity->model);key.Add(entity->model->modelCRC);
        key.Vector(entity->origin);key.Vector(entity->angles);
        R_BudgetBytes(key,&entity->curstate,sizeof(entity->curstate));
        if(entity->model!=worldmodel && entity->model->type==mod_brush)
        {
            for(int j=0;entity->model->textures && j<entity->model->numtextures;++j)
            {
                const auto *texture=entity->model->textures[j];key.Pointer(texture);
                if(!texture) continue;
                key.Add(texture->gl_texturenum);
                if(texture->material) key.Add(texture->material->flags);
                if(texture->anim_total || texture->alternate_anims || (texture->material &&
                    FBitSet(texture->material->flags,BRUSH_LIQUID|BRUSH_CONVEYOR))) live=true;
            }
        }
        // Studio procedural/animation/physical blend inputs and engine particle
        // state are not all exposed here. Time prevents unsafe cross-frame hits.
        if(entity->model->type!=mod_brush && !(shadow && FBitSet(entity->curstate.effects,EF_NOSHADOW))) unknownGeometry=true;
    }
    if(live) R_BudgetBytes(key,&tr.time,sizeof(tr.time));
    // Clock equality alone does not prove Studio packet/procedural or external
    // particle state is unchanged. These paths keep their per-frame rendering.
    if(unknownGeometry || (shadow && R_BudgetValue("r_shadow_particles",1)>0) ||
        (!shadow && !tr.fGamePaused)) key.Add(tr.realframecount);
    for(const char *name:{"r_shadows","r_renderplayershadow","r_grass","r_grass_shadows","gl_shadow_offset",
        "r_nocull","r_novis","r_lockpvs","r_lockfrustum","r_shadow_particles","gl_shadowmap_size",
        "gl_sun_shadows","r_grass_alpha","r_grass_fade_start","r_grass_fade_dist","r_drawentities"})
        key.Float(R_BudgetValue(name,0));
    if(!shadow)
    {
        key.Vector(tr.fogColor);key.Float(tr.fogDensity);key.Add(tr.fogEnabled);
        key.Vector(tr.sky_normal);key.Pointer(tr.sky_camera);
        for(const auto &texture:tr.skyboxTextures) key.Add(texture.ToInt());
        R_BudgetBytes(key,tr.lightstyle,sizeof(tr.lightstyle));
        for(int i=0;i<MAX_DLIGHTS;++i)
        {
            const CDynLight &light=tr.dlights[i];key.Vector(light.origin);key.Vector(light.angles);
            key.Vector(light.color);key.Float(light.radius);key.Float(light.die);key.Float(light.fov);key.Add(light.flags);
        }
    }
    return key;
}
inline RenderSubviewCache *R_FindBudgetSubview(const msurface_t *surface)
{
    auto &entries=R_RenderBudget().subviews;
    for(auto &entry:entries) if(entry.surface==surface) return &entry;
    for(auto &entry:entries) if(!entry.surface) { entry.surface=surface;return &entry; }
    return nullptr; // bounded cache; retain the ordinary rendering path
}
inline void R_InvalidateBudgetSubview(const msurface_t *surface)
{
    for(auto &entry:R_RenderBudget().subviews) if(entry.surface==surface)
    {
        if(entry.slot) tr.subviewTextures[entry.slot-1].cachePinned=false;
        entry=RenderSubviewCache{};return;
    }
}
inline bool R_BudgetSubviewValid(const RenderSubviewCache &entry,const CViewport &port)
{
    return entry.slot>0 && entry.slot<=tr.num_subview_used &&
        tr.subviewTextures[entry.slot-1].cachePinned &&
        tr.subviewTextures[entry.slot-1].texturenum.Initialized() &&
        tr.subviewTextures[entry.slot-1].texturenum.GetWidth()==port.GetWidth() &&
        tr.subviewTextures[entry.slot-1].texturenum.GetHeight()==port.GetHeight();
}
inline posecache::Key R_BudgetSubviewKey(msurface_t *surface,cl_entity_t *camera,
    const ref_viewpass_t &view,const matrix4x4 &matrix,const ref_instance_t *parent)
{
    posecache::Key key;key.Pointer(surface);key.Pointer(surface->info->parent);
    key.Pointer(surface->info->parent->model);key.Add(surface->info->parent->model->modelCRC);
    key.Pointer(camera);key.Pointer(camera->model);key.Add(camera->model ? camera->model->modelCRC : 0);
    key.Add(camera->curstate.number);key.Add(camera->player);key.Add(surface->flags & (SURF_SCREEN|SURF_PORTAL|SURF_REFLECT|SURF_REFLECT_PUDDLE));
    key.Add(glState.stack_position);key.Float(surface->info->parent->curstate.fuser2);
    key.Float(R_BudgetValue("gl_hdr",0));key.Float(parent->view.farClip);
    R_BudgetBytes(key,view.viewport,sizeof(view.viewport));key.Float(view.fov_x);key.Float(view.fov_y);key.Add(view.flags);
    if(!FBitSet(surface->flags,SURF_SCREEN))
    {
        key.Vector(view.vieworigin);key.Vector(view.viewangles);R_BudgetMatrix(key,matrix);
        const auto scene=R_BudgetSceneKey(false);key.words.insert(key.words.end(),scene.words.begin(),scene.words.end());
    }
    return key;
}
inline std::uint64_t R_BudgetShadowScene()
{
    auto key=R_BudgetSceneKey(true);auto &state=R_RenderBudget();
    if(key.words.size()>8192) { state.shadowScene.Invalidate();return 0; }
    if(!state.shadowScene.Matches(key)) { state.shadowScene.Commit(key);++state.sceneRevision; }
    return state.sceneRevision;
}
inline posecache::Key R_BudgetShadowKey(const CDynLight *light,std::uint64_t scene)
{
    posecache::Key key;key.Add(scene);key.Add(light->key);key.Add(light->type);key.Add(light->flags);
    key.Pointer(light->parentEntity);key.Vector(light->origin);key.Vector(light->angles);
    key.Float(light->radius);key.Float(light->fov);R_BudgetMatrix(key,light->projectionMatrix);
    key.Add(tr.fbo_shadow2D.GetWidth());key.Add(tr.fbo_shadowCM.GetWidth());
    key.Add(GL_Support(R_FRAMEBUFFER_OBJECT));
    if(light->type==LIGHT_DIRECTIONAL)
    {
        key.Vector(RI->view.origin);key.Vector(RI->view.angles);key.Float(RI->view.fov_x);key.Float(RI->view.fov_y);
        key.Float(RI->view.farClip);key.Vector(tr.sky_normal);
        R_BudgetBytes(key,RI->view.parallelSplitDistances,sizeof(RI->view.parallelSplitDistances));
        for(int i=0;i<MAX_SHADOWMAPS;++i) key.Add(tr.sunShadowFBO[i].GetWidth());
    }
    return key;
}
inline std::uint64_t R_BudgetShadowWrite(int type,int slot)
{
    auto &state=R_RenderBudget();
    if(type==LIGHT_DIRECTIONAL) return state.sunWrite;
    if(slot<0 || slot>=MAX_SHADOWS) return 0;
    return type==LIGHT_OMNI ? state.writesCube[slot] : state.writes2D[slot];
}
inline void R_MarkShadowWrite(int type,int slot)
{
    auto &state=R_RenderBudget();
    if(type==LIGHT_DIRECTIONAL) ++state.sunWrite;
    else if(slot>=0 && slot<MAX_SHADOWS)
    { if(type==LIGHT_OMNI) ++state.writesCube[slot];else ++state.writes2D[slot]; }
}
inline bool R_ReuseBudgetShadow(CDynLight *light,int index,const posecache::Key &key)
{
    auto &cached=R_RenderBudget().shadows[index];
    const int slot=light->type==LIGHT_OMNI ? tr.num_CM_shadows_used : tr.num_2D_shadows_used;
    if(!cached.stamp.Matches(key) || cached.write!=R_BudgetShadowWrite(light->type,cached.slot)) return false;
    if(light->type==LIGHT_DIRECTIONAL)
    {
        for(int i=0;i<MAX_SHADOWMAPS;++i)
            if(!tr.sunShadowFBO[i].Active() || tr.sunShadowFBO[i].GetTexture()!=cached.texture[i]) return false;
    }
    if(light->type!=LIGHT_DIRECTIONAL)
    {
        // Reuse consumes its ordinary per-frame slot. Later allocations cannot
        // overwrite the depth image while this view still references it.
        if(cached.slot!=slot || slot>=MAX_SHADOWS) return false;
        const auto texture=light->type==LIGHT_OMNI ? tr.shadowCubemaps[slot] : tr.shadowTextures[slot];
        if(!texture.Initialized() || texture!=cached.texture[0]) return false;
        if(light->type==LIGHT_OMNI) ++tr.num_CM_shadows_used;else ++tr.num_2D_shadows_used;
    }
    for(int i=0;i<MAX_SHADOWMAPS;++i)
    {
        light->shadowTexture[i]=cached.texture[i];light->shadowMatrix[i]=cached.matrix[i];
        std::memcpy(light->gl_shadowMatrix[i],cached.glMatrix[i],sizeof(cached.glMatrix[i]));
    }
    pxperf::Get().Add(pxperf::Metric::ShadowCacheHits,1);return true;
}
inline void R_CommitBudgetShadow(CDynLight *light,int index,const posecache::Key &key,int slot)
{
    auto &cached=R_RenderBudget().shadows[index];cached.stamp.Invalidate();
    const int count=light->type==LIGHT_DIRECTIONAL ? MAX_SHADOWMAPS : 1;
    for(int i=0;i<count;++i)
    {
        if(light->type==LIGHT_DIRECTIONAL && !tr.sunShadowFBO[i].Active()) return;
        if(!light->shadowTexture[i].Initialized() || light->shadowTexture[i]==tr.depthTexture ||
            light->shadowTexture[i]==tr.depthCubemap) return;
    }
    cached.slot=slot;cached.write=R_BudgetShadowWrite(light->type,slot);
    for(int i=0;i<MAX_SHADOWMAPS;++i)
    {
        cached.texture[i]=light->shadowTexture[i];cached.matrix[i]=light->shadowMatrix[i];
        std::memcpy(cached.glMatrix[i],light->gl_shadowMatrix[i],sizeof(cached.glMatrix[i]));
    }
    cached.stamp.Commit(key);
}
