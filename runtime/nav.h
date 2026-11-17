#ifndef PIP_NAV_H
#define PIP_NAV_H
#include "pip.h"
#define NAV_MAX_CELLS 8388608u
#define NAV_MAX_REGIONS 65535u
#define NAV_MAX_PORTALS 4096u
#define NAV_PATH_POINTS 64u
enum {NAV_INSIDE=1,NAV_CALM=2,NAV_WALK=4,NAV_GATE=8};
typedef struct NavPortal {
    PipPortal policy;
    uint32_t cell_a,cell_b,path_count;
    PipPoint path[NAV_PATH_POINTS]; /* native A* order: A to B */
} NavPortal;
typedef struct NavView {PipWorld world;uint32_t degree,offset;} NavView;
typedef struct Nav {
    int64_t x0,y0;
    uint32_t w,h,n,region_count,portal_count;
    float cell_size;
    uint64_t revision;
    uint8_t *flags;
    uint32_t *label,*distance,*portal,*queue;
    PipRegion *regions;
    NavPortal *portals;
    NavView *views;
    PipRegion *view_regions;
    PipPortal *view_portals;
} Nav;
int nav_create(Nav *n,uint32_t w,uint32_t h);
void nav_destroy(Nav *n);
int nav_components(Nav *n);
int nav_connect(Nav *n,uint32_t a,uint32_t b,const PipPoint *path,uint32_t count);
int nav_finish(Nav *n,uint64_t revision);
uint32_t nav_cell(const Nav *n,PipPoint p);
PipPoint nav_point(const Nav *n,uint32_t cell);
/* Complete route is represented by an origin flow plus a validated gate path. */
int nav_direction(const Nav *n,uint32_t cell,uint32_t portal,uint64_t origin,PipPoint at,PipPoint *direction);
int32_t nav_query(void *,const PipPerson *,uint64_t,uint64_t,PipPoint,float *);
uint32_t nav_selftest(void);
#endif
