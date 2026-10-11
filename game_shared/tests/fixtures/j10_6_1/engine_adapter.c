/* Isolated engine-services adapter. This compiles the actual production
 * query implementation with explicit model/edict/surface service adapters.
 * It is not a replacement for an SDK engine build or a live BSP map.
 */
#include <assert.h>
#include <string.h>
#include <math.h>
#include "world_ballistic_query.h"
#include "world_solid_interval.h"
#define GAME_EXPORT
#define CONTENTS_EMPTY (-1)
#define CONTENTS_SOLID (-2)
#define QBSP2_VERSION 2
#define FRAC_EPSILON (1.0f/1024.0f)
#define mod_brush 1
#define ss_active 1
typedef float vec3_t[3];
typedef struct { float normal[3],dist; } mplane_t;
typedef struct { int planenum; short children[2]; } mclipnode16_t;
typedef struct { int planenum; int children[2]; } mclipnode32_t;
typedef struct { union {mclipnode16_t *clipnodes16; mclipnode32_t *clipnodes32;}; mplane_t *planes; int firstclipnode,lastclipnode; } hull_t;
typedef struct { mplane_t *edges; int numedges; vec3_t origin; float radius; } mfacebevel_t;
typedef struct { mfacebevel_t *bevel; } mextrasurf_t;
typedef struct { int present; } texture_t;
typedef struct { texture_t *texture; } mtexinfo_t;
typedef struct msurface_s { mplane_t *plane; mextrasurf_t *info; mtexinfo_t *texinfo; int material; } msurface_t;
typedef struct { mplane_t *plane; int first,num; } mnode_t;
typedef struct { int type,numnodes,numplanes,numsurfaces; hull_t hulls[1]; mplane_t *planes; mnode_t *nodes; msurface_t *surfaces; } model_t;
typedef struct { struct {vec3_t origin,angles; int modelindex;} v; } edict_t;
static struct {int state; model_t *worldmodel;} sv;
static struct {int version;} world;
static const vec3_t vec3_origin={0,0,0};
#define VectorCopy(a,b) memcpy((b),(a),sizeof(vec3_t))
#define VectorSubtract(a,b,c) do{int k;for(k=0;k<3;++k)(c)[k]=(a)[k]-(b)[k];}while(0)
#define DotProduct(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define PlaneDiff(p,n) (DotProduct((p),(n)->normal)-(n)->dist)
static int VectorIsNull(const float *p) {return p[0]==0&&p[1]==0&&p[2]==0;}
static int node_firstsurface(const mnode_t *n,const model_t *m){(void)m;return n->first;}
static int node_numsurfaces(const mnode_t *n,const model_t *m){(void)m;return n->num;}
static edict_t entity;
static model_t model;
static mplane_t planes[4],edge_planes[4][4];
static mclipnode16_t nodes16[4];
static mclipnode32_t nodes32[4];
static mnode_t nodes[4];
static msurface_t surfaces[5];
static mextrasurf_t infos[4];
static mfacebevel_t bevels[4];
static mtexinfo_t texinfo;
static texture_t texture;
static float second_begin,second_end;
static edict_t *SV_EdictNum(int index){return index==0?&entity:NULL;}
static model_t *SV_ModelHandle(int index){return index==1?&model:NULL;}
static hull_t *SV_HullForBsp(edict_t *e,const float *mins,const float *maxs,float *offset){
    (void)e;(void)mins;(void)maxs;VectorCopy(vec3_origin,offset);return &model.hulls[0];
}
static int PM_SampleMiptex(const msurface_t *surface,const float *point){
    (void)point;return surface->material<0?CONTENTS_EMPTY:CONTENTS_SOLID;
}
#include "world_ballistic_query_impl.h"

void TestWorldSetup(int bits,float begin,float end,int exit_material) {
    const float positions[4]={0,2.08f,begin,end};
    const int children[4][2]={{1,-1},{2,-2},{3,-1},{-1,-2}};
    int i,j;
    memset(&entity,0,sizeof(entity));entity.v.modelindex=1;
    memset(&model,0,sizeof(model));
    memset(planes,0,sizeof(planes));memset(edge_planes,0,sizeof(edge_planes));
    memset(nodes,0,sizeof(nodes));memset(surfaces,0,sizeof(surfaces));
    memset(bevels,0,sizeof(bevels));
    world.version=bits==32?QBSP2_VERSION:1;
    sv.state=ss_active;sv.worldmodel=&model;
    model.type=mod_brush;model.numnodes=4;model.numplanes=4;model.numsurfaces=4;
    model.planes=planes;model.nodes=nodes;model.surfaces=surfaces;
    model.hulls[0].planes=planes;model.hulls[0].firstclipnode=0;model.hulls[0].lastclipnode=3;
    if(bits==32) model.hulls[0].clipnodes32=nodes32;
    else model.hulls[0].clipnodes16=nodes16;
    texinfo.texture=&texture;
    second_begin=begin;second_end=end;
    for(i=0;i<4;++i) {
        planes[i].normal[0]=1;planes[i].dist=positions[i];
        nodes16[i].planenum=nodes32[i].planenum=i;
        for(j=0;j<2;++j) {nodes16[i].children[j]=children[i][j];nodes32[i].children[j]=children[i][j];}
        nodes[i].plane=&planes[i];nodes[i].first=i;nodes[i].num=1;
        surfaces[i].plane=&planes[i];surfaces[i].info=&infos[i];surfaces[i].texinfo=&texinfo;
        surfaces[i].material=i<2?0:1;
        infos[i].bevel=&bevels[i];bevels[i].edges=edge_planes[i];bevels[i].numedges=4;
        bevels[i].origin[0]=positions[i];bevels[i].radius=100;
        edge_planes[i][0].normal[1]=1;edge_planes[i][1].normal[1]=-1;
        edge_planes[i][2].normal[2]=1;edge_planes[i][3].normal[2]=-1;
        for(j=0;j<4;++j) edge_planes[i][j].dist=5;
    }
    surfaces[1].material=exit_material;
}
int TestWorldMaterial(const struct msurface_s *surface){return surface->material;}
int TestWorldContents(const float *point){
    return (point[0]>=0&&point[0]<2.08f)||(point[0]>=second_begin&&point[0]<second_end)?
        CONTENTS_SOLID:CONTENTS_EMPTY;
}
void TestWorldServiceChecks(void) {
    world_ballistic_request_t request={0};
    world_ballistic_result_t result={0};
    const world_ballistic_query_api_t *api=SV_GetWorldBallisticQuery();
    int bits;
    request.struct_size=sizeof(request);request.start[0]=-1;request.direction[0]=1;
    request.contact_distance=0.96875f;request.maximum_depth=96;request.preferred_offset=.5f;
    for(bits=16;bits<=32;bits+=16) {
        TestWorldSetup(bits,2.18f,4,0);result.struct_size=sizeof(result);
        assert(api->query(&request,&result));
        assert(fabs(result.exit_position[0]-2.08)<1e-5);
        assert(fabs(result.clear_end_distance-3.18)<1e-5);
        assert(result.entry_surface==&surfaces[0]&&result.exit_surface==&surfaces[1]);
        assert(result.entry_normal[0]==-1&&result.exit_normal[0]==1);
    }
    result.struct_size=8;assert(!api->query(&request,&result));
    result.struct_size=sizeof(result);request.direction[0]=2;assert(!api->query(&request,&result));
    request.direction[0]=1;request.contact_distance=2;assert(!api->query(&request,&result));
    request.contact_distance=.96875f;request.maximum_depth=100;assert(!api->query(&request,&result));
    request.maximum_depth=96;infos[1].bevel=NULL;assert(!api->query(&request,&result));
    TestWorldSetup(16,2.18f,4,-1);assert(!api->query(&request,&result));
    TestWorldSetup(16,2.18f,4,0);
    surfaces[4]=surfaces[0];surfaces[1]=surfaces[0];
    nodes[0].num=2;assert(!api->query(&request,&result)); /* overlapping faces */
    TestWorldSetup(16,2.18f,4,0);sv.state=0;assert(!api->query(&request,&result));
    TestWorldSetup(16,2.18f,4,0);model.hulls[0].clipnodes16=NULL;assert(!api->query(&request,&result));
}
