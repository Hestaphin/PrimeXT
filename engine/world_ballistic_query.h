/* J.10.6.1: optional, versioned world-penetration query.
 * Keep this header identical in PrimeXT and Xash. No physics-table growth.
 * Distances are game units along a unit direction from request.start.
 * Surface pointers belong to the current world model; do not cache them.
 */
#ifndef WORLD_BALLISTIC_QUERY_H
#define WORLD_BALLISTIC_QUERY_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define WORLD_BALLISTIC_QUERY_OBJECT "Xash.WorldBallisticQuery.1"
#define WORLD_BALLISTIC_QUERY_VERSION 1u

struct msurface_s;
typedef struct world_ballistic_request_s
{
    uint32_t struct_size;
    float start[3];
    float direction[3];
    float contact_distance;
    float maximum_depth;
    float preferred_offset;
} world_ballistic_request_t;

typedef struct world_ballistic_result_s
{
    uint32_t struct_size;
    float entry_distance;
    float exit_distance;
    float clear_end_distance;
    float entry_position[3];
    float exit_position[3];
    float entry_normal[3];
    float exit_normal[3];
    struct msurface_s *entry_surface;
    struct msurface_s *exit_surface;
} world_ballistic_result_t;

typedef struct world_ballistic_query_api_s
{
    uint32_t version;
    uint32_t struct_size;
    /* Zero means unavailable, unresolved, malformed or out of bounds. */
    int (*query)( const world_ballistic_request_t *, world_ballistic_result_t * );
} world_ballistic_query_api_t;

/* Internal engine accessor used by Sys_GetNativeObject. */
const world_ballistic_query_api_t *SV_GetWorldBallisticQuery( void );

#ifdef __cplusplus
}
#endif
#endif
