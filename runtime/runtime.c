/* Original build-specific adapter. No game code or assets are embedded. */
#include "nav.h"
#include "win.h"
#define NONE 0xffffffffu
#define MAX_PEOPLE 4194304u
typedef struct Array {void *data;int64_t len,cap;void *allocator_proc,*allocator_data;} Array;
typedef struct Gate {PipPoint spot,dir,inward;int64_t cell;} Gate;
typedef struct Agent {PipState policy;PipPoint direction;uint64_t tick;uint32_t active,reserved;} Agent;
typedef uint64_t (*Dispatch)(uint64_t,void *,uint64_t,int64_t,void *,void *,void *,void *);
typedef uint64_t (*Astar)(void *,uint64_t,uint64_t,Array *,int64_t,uint8_t,void *);
typedef struct RuntimeStats {
    uint64_t size,enabled,frames,tick,population,regions,breached,portals,native_paths;
    uint64_t steered,arrivals,learned,no_route,rebuilds,move_observed,errors,overrides,civilian_hook;
} RuntimeStats;
__declspec(dllexport) RuntimeStats pip_runtime_stats={.size=sizeof(RuntimeStats)};
static uint8_t *image,*units,*buildings;
static Dispatch original_dispatch;
static Nav nav;
static Agent *agents;
static uint32_t agent_capacity;
static uint64_t topology_hash,label_hash,last_tick,last_next_id;
static Handle logfile;
static int installed;
static uint32_t requested=1;
void *pip_move_return;
void *pip_collision_return;
static uint32_t bridge_test_branch;
extern void pip_move_bridge(void);
extern uint32_t pip_bridge_selftest(void);
int pip_install_hooks(uint8_t *,void *,void *,void **,void **);
static int64_t i64(const void *p,size_t o){return *(const int64_t *)((const uint8_t *)p+o);}
static float f32(const void *p,size_t o){return *(const float *)((const uint8_t *)p+o);}
static Array *array(void *p,size_t o){return (Array *)((uint8_t *)p+o);}
static int valid_array(const Array *a,int64_t minimum,int64_t maximum){return a->len>=minimum&&a->len<=maximum&&a->cap>=a->len&&a->cap<=0x10000000&&(!a->len||(uintptr_t)a->data>0x10000);}
static void log_text(const char *s){if(!logfile)return;uint32_t count=0,written;while(s[count])count++;WriteFile(logfile,s,count,&written,0);}
static void log_number(uint64_t x){char out[24],rev[24];uint32_t n=0;do{rev[n++]=(char)('0'+x%10);x/=10;}while(x);for(uint32_t i=0;i<n;i++)out[i]=rev[n-i-1];out[n]=0;log_text(out);}
static void log_status(void){
    log_text("tick=");log_number(pip_runtime_stats.tick);log_text(" civilians-steered=");log_number(pip_runtime_stats.steered);
    log_text(" arrivals=");log_number(pip_runtime_stats.arrivals);log_text(" informed=");log_number(pip_runtime_stats.learned);
    log_text(" no-route=");log_number(pip_runtime_stats.no_route);log_text(" regions=");log_number(pip_runtime_stats.regions);
    log_text(" breached=");log_number(pip_runtime_stats.breached);log_text(" portals=");log_number(pip_runtime_stats.portals);
    log_text(" native-paths=");log_number(pip_runtime_stats.native_paths);log_text(" movement-observed=");log_number(pip_runtime_stats.move_observed);log_text("\r\n");
    log_text("delivered=");log_number(pip_runtime_stats.overrides);log_text(" civilian-hook=");log_number(pip_runtime_stats.civilian_hook);log_text("\r\n");
}
static uint64_t packed(PipPoint p){union{PipPoint p;uint64_t v;}u={p};return u.v;}
static float distance2(PipPoint a,PipPoint b){float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;}
static int path_valid(const Nav *n,uint32_t a,uint32_t b,const PipPoint *p,uint32_t count){
    uint32_t ra=n->label[a],rb=n->label[b];
    for(uint32_t i=1;i<count;i++){
        PipPoint from=p[i-1],to=p[i];float length=__builtin_sqrtf(distance2(from,to));
        if(!(length<80*n->cell_size))return 0;
        uint32_t steps=(uint32_t)(length/(n->cell_size*.3f))+1;
        for(uint32_t k=0;k<=steps;k++){
            float t=(float)k/(float)steps;PipPoint q={from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t};
            uint32_t c=nav_cell(n,q);
            if(c>=n->n||!(n->flags[c]&NAV_WALK))return 0;
            if(n->label[c]!=ra&&n->label[c]!=rb&&!(n->flags[c]&NAV_GATE))return 0;
        }
    }
    return 1;
}
static uint32_t gate_side(const Gate *g,float sign){
    for(uint32_t step=1;step<=30;step++){
        float d=(.25f+.5f*(float)step)*nav.cell_size;
        PipPoint q={g->spot.x-g->dir.y*d*sign,g->spot.y+g->dir.x*d*sign};
        uint32_t c=nav_cell(&nav,q);
        if(c<nav.n&&nav.label[c])return c;
    }
    return NONE;
}
static int connect_gates(void *context){
    Array *list=array(buildings,0x1930);
    if(!valid_array(list,0,NAV_MAX_PORTALS))return 0;
    /* Caller owns this output buffer. A* cannot exceed max_nodes+2 points. */
    PipPoint *native_points=alloc(4096*sizeof(PipPoint));if(!native_points)return 0;
    Array result={native_points,0,4096,0,0};
    Astar find_path=(Astar)(image+0x115770);
    for(int64_t i=0;i<list->len;i++){
        const Gate *g=(const Gate *)list->data+i;
        uint32_t a=gate_side(g,1),b=gate_side(g,-1);
        if(a>=nav.n||b>=nav.n||nav.label[a]==nav.label[b])continue;
        PipPoint pa=nav_point(&nav,a),pb=nav_point(&nav,b);
        result.len=0;
        if(!(find_path(buildings,packed(pa),packed(pb),&result,512,0,context)&0xff)||result.len<1||result.len>NAV_PATH_POINTS-2)continue;
        PipPoint path[NAV_PATH_POINTS];uint32_t n=0;path[n++]=pa;
        int reverse=distance2(native_points[0],pa)>distance2(native_points[result.len-1],pa);
        for(int64_t j=0;j<result.len;j++){
            PipPoint q=native_points[reverse?result.len-1-j:j];
            if(distance2(path[n-1],q)>.0001f)path[n++]=q;
        }
        if(distance2(path[n-1],pb)>.0001f)path[n++]=pb;
        if(!path_valid(&nav,a,b,path,n))continue;
        if(nav_connect(&nav,a,b,path,n))pip_runtime_stats.native_paths++;
    }
    release(native_points);return 1;
}
static uint64_t hash_add(uint64_t h,uint64_t x){return (h^x)*1099511628211ULL;}
static int rebuild(void *context,int force){
    uint8_t *safe=buildings+0x1878;
    int64_t x0=i64(safe,0),y0=i64(safe,8),w=i64(safe,16),h=i64(safe,24);
    int64_t cols=i64(buildings,0x3c8),rows=i64(buildings,0x3d0);float size=f32(buildings,0x3c0);
    Array *cells=array(buildings,0x3d8),*inside=array(safe,32),*calm=array(safe,72),*gates=array(buildings,0x1908);
    if(w<1||h<1||w>NAV_MAX_CELLS||h>NAV_MAX_CELLS||(uint64_t)w*h>NAV_MAX_CELLS||x0<0||y0<0||cols<1||rows<1||cols>1000000||rows>1000000||x0+w>cols||y0+h>rows||!(size>.01f&&size<1000))return 0;
    int64_t words=(w*h+63)/64;
    if(!valid_array(cells,cols*rows,cols*rows)||!valid_array(inside,words,words)||!valid_array(calm,words,words)||!valid_array(gates,0,100000))return 0;
    int shape=nav.w!=w||nav.h!=h||nav.x0!=x0||nav.y0!=y0||nav.cell_size!=size;
    if(shape){if(!nav_create(&nav,(uint32_t)w,(uint32_t)h))return 0;nav.x0=x0;nav.y0=y0;nav.cell_size=size;force=1;}
    uint64_t *in=inside->data,*ca=calm->data;
    uint32_t c=0;
    for(uint32_t y=0;y<nav.h;y++){
        const uint8_t *row=(const uint8_t *)cells->data+(y0+y)*cols+x0;
        for(uint32_t x=0;x<nav.w;x++,c++){
            uint8_t value=row[x];
            nav.flags[c]=(uint8_t)(((in[c>>6]>>(c&63))&1)?NAV_INSIDE:0)|
                         (uint8_t)(((ca[c>>6]>>(c&63))&1)?NAV_CALM:0)|(value==0?NAV_WALK:0);
        }
    }
    for(int64_t i=0;i<gates->len;i++){
        int64_t g=((int64_t *)gates->data)[i];if(g<0||g>=cols*rows)continue;
        int64_t x=g%cols-x0,y=g/cols-y0;
        if(x>=0&&y>=0&&x<w&&y<h)nav.flags[y*w+x]|=NAV_GATE|NAV_WALK;
    }
    uint64_t hash=14695981039346656037ULL;
    for(uint32_t c=0;c<nav.n;c++)hash=hash_add(hash,nav.flags[c]);
    if(!force&&hash==topology_hash)return 1;
    if(!nav_components(&nav)||!connect_gates(context)||!nav.region_count||!nav_finish(&nav,pip_runtime_stats.rebuilds+1))return 0;
    topology_hash=hash;pip_runtime_stats.rebuilds++;
    uint64_t labels=14695981039346656037ULL;for(uint32_t c=0;c<nav.n;c++)labels=hash_add(labels,nav.label[c]);
    int changed_labels=label_hash!=labels;label_hash=labels;
    pip_runtime_stats.regions=nav.region_count;pip_runtime_stats.portals=nav.portal_count;pip_runtime_stats.breached=0;
    for(uint32_t i=1;i<=nav.region_count;i++)if(nav.regions[i].flags&PIP_REGION_BREACHED)pip_runtime_stats.breached++;
    if(agents&&changed_labels)bytes_zero(agents,(size_t)agent_capacity*sizeof(Agent));
    log_text("Navigation rebuilt. ");log_status();return 1;
}
static int ensure_agents(uint64_t next){
    if(next>MAX_PEOPLE)return 0;
    if(next<=agent_capacity)return 1;
    uint32_t capacity=1024;while(capacity<next)capacity*=2;
    Agent *fresh=alloc((size_t)capacity*sizeof(Agent));if(!fresh)return 0;
    if(agents)bytes_copy(fresh,agents,(size_t)agent_capacity*sizeof(Agent));release(agents);agents=fresh;agent_capacity=capacity;return 1;
}
static void prepare(void *params,void *context){
    __atomic_store_n(&pip_runtime_stats.enabled,0,__ATOMIC_RELEASE);
    int64_t count=i64(units,0x1b8),tick=i64(units,0x1388),next=i64(units,0x210);
    Array *positions=array(units,0),*ids=array(units,0x190),*kinds=array(units,0x50),*converting=array(units,0xc8),*panic_levels=array(units,0x140);
    if(count<0||count>MAX_PEOPLE||tick<0||next<0||!valid_array(positions,count,MAX_PEOPLE)||!valid_array(ids,count,MAX_PEOPLE)||!valid_array(kinds,count,MAX_PEOPLE)||!valid_array(converting,count,MAX_PEOPLE)||!valid_array(panic_levels,count,MAX_PEOPLE)||!ensure_agents((uint64_t)next))goto failed;
    int reset=(uint64_t)tick<last_tick||(uint64_t)next<last_next_id;
    if(reset&&agents)bytes_zero(agents,(size_t)agent_capacity*sizeof(Agent));
    last_tick=(uint64_t)tick;last_next_id=(uint64_t)next;
    pip_runtime_stats.tick=(uint64_t)tick;pip_runtime_stats.population=(uint64_t)count;pip_runtime_stats.frames++;
    if(!__atomic_load_n(&requested,__ATOMIC_ACQUIRE))return;
    if(!rebuild(context,reset))goto failed;
    float panic=f32(params,1172);if(!(panic>=0&&panic<1e6f))goto failed;
    double time=(double)tick/30.;
    for(uint32_t i=1;i<=nav.region_count;i++)nav.views[i].world.sim_time=time;
    uint64_t steered=0,no_route=0;
    for(int64_t i=0;i<count;i++){
        uint64_t id=((uint64_t *)ids->data)[i];if(id>=agent_capacity)continue;
        Agent *a=&agents[id];pip_runtime_stats.overrides+=a->reserved;a->reserved=0;a->active=0;a->tick=(uint64_t)tick;
        if(((uint8_t *)kinds->data)[i]!=0||((uint8_t *)converting->data)[i]){pip_reset(&a->policy);continue;}
        PipPoint pos=((PipPoint *)positions->data)[i];uint32_t cell=nav_cell(&nav,pos);
        uint64_t region=cell<nav.n?nav.label[cell]:0;
        uint64_t view=region?region:a->policy.origin_region;
        if(!view||view>nav.region_count){pip_reset(&a->policy);continue;}
        int healthy=region&&!(nav.regions[region].flags&PIP_REGION_BREACHED);
        if(healthy&&!a->policy.target_region&&a->policy.known_region==region&&a->policy.informed_revision==nav.revision)continue;
        int arrived=healthy&&a->policy.target_region;
        if(arrived)pip_runtime_stats.arrivals++;
        PipPerson person={id+1,region,pos,3,0};PipDecision decision;
        uint64_t known=a->policy.known_region;
        pip_step(&nav.views[view].world,&person,&a->policy,&decision);
        if(arrived&&pip_runtime_stats.arrivals<=16){
            log_text("Arrival id=");log_number(id);log_text(" tick=");log_number((uint64_t)tick);
            log_text(" informed-region=");log_number(a->policy.known_region);
            log_text(" next-fallback=");log_number(a->policy.fallback_region);log_text("\r\n");
        }
        if(healthy&&a->policy.known_region!=known)pip_runtime_stats.learned++;
        if(decision.action==PIP_NO_ROUTE){no_route++;continue;}
        if(decision.action==PIP_EVACUATE&&nav_direction(&nav,cell,(uint32_t)decision.portal_id,a->policy.origin_region,pos,&a->direction)){
            a->active=1;steered++;
            if(steered==1&&tick%150==1){log_text("sample id=");log_number(id);log_text(" cell=");log_number(cell);log_text(" distance=");log_number(nav.distance[cell]);log_text(" portal=");log_number(decision.portal_id);log_text("\r\n");}
            float *level=(float *)panic_levels->data+i;
            if(*level<panic)*level=panic;
        }
    }
    pip_runtime_stats.steered=steered;pip_runtime_stats.no_route=no_route;
    __atomic_store_n(&pip_runtime_stats.enabled,1,__ATOMIC_RELEASE);
    if(tick%150==1||pip_runtime_stats.frames==1)log_status();
    return;
failed:
    pip_runtime_stats.errors++;if(pip_runtime_stats.errors<4)log_text("Runtime snapshot rejected; native movement retained.\r\n");
}
uint64_t pip_dispatch_hook(uint64_t mode,void *params,uint64_t world,int64_t count,void *danger,void *home,void *finish,void *context){
    if((mode&1)==0)prepare(params,context);
    return original_dispatch(mode,params,world,count,danger,home,finish,context);
}
uint32_t pip_native_steer(void *u,uint64_t index,float *registers,uint8_t *stack){
    if(!image&&!u)return bridge_test_branch;
    if(u!=units||!__atomic_load_n(&pip_runtime_stats.enabled,__ATOMIC_ACQUIRE)||!__atomic_load_n(&requested,__ATOMIC_ACQUIRE)||index>=(uint64_t)i64(units,0x1b8))return 0;
    if(!__atomic_load_n(&pip_runtime_stats.move_observed,__ATOMIC_RELAXED))__atomic_store_n(&pip_runtime_stats.move_observed,1,__ATOMIC_RELAXED);
    if(((uint8_t *)array(units,0x50)->data)[index]||((uint8_t *)array(units,0xc8)->data)[index])return 0;
    if(!__atomic_load_n(&pip_runtime_stats.civilian_hook,__ATOMIC_RELAXED))__atomic_store_n(&pip_runtime_stats.civilian_hook,1,__ATOMIC_RELAXED);
    uint64_t id=((uint64_t *)array(units,0x190)->data)[index];if(id>=agent_capacity)return 0;
    Agent *a=&agents[id];if(!a->active||a->tick!=last_tick)return 0;
    PipPoint position=((PipPoint *)array(units,0)->data)[index];
    /* Preserve this civilian's actual speed, including native flee-speed rolls. */
    float dt=*(float *)(stack+0x90),speed=*(float *)(stack+0xb0);
    if(!(dt>0&&dt<.25f&&speed>0&&speed<10000))return 0;
    float stride=speed*dt;
    if(stride>nav.cell_size*.45f)stride=nav.cell_size*.45f;
    PipPoint proposed={position.x+a->direction.x*stride,position.y+a->direction.y*stride};
    uint32_t c=nav_cell(&nav,proposed);
    if(c>=nav.n||!(nav.flags[c]&NAV_WALK))return 0;
    a->reserved=1;
    /* XMM8 is the proposed position; XMM11/12 and these stack scalars are
     * the native DDA's direction. XMM9 (old position) stays untouched. */
    registers[0]=proposed.x;registers[1]=proposed.y;
    registers[12]=a->direction.x;registers[16]=a->direction.y;
    *(float *)(stack+0x70)=a->direction.x;*(float *)(stack+0x280)=a->direction.y;
    return 1;
}
typedef struct BootArgs {uint32_t size,mode;const Wide *root;} BootArgs;
static int open_log(const Wide *root){
    Wide path[512];uint32_t n=0;while(root[n]&&n<400){path[n]=root[n];n++;}if(n==400)return 0;
    const Wide *tail=(const Wide *)L"\\build\\PIP.Runtime.log";for(uint32_t i=0;tail[i];i++)path[n++]=tail[i];path[n]=0;
    logfile=CreateFileW(path,4,3,0,4,0x80,0);if(logfile==(Handle)(intptr_t)-1){logfile=0;return 0;}return 1;
}
static int host_valid(uint8_t *base){
    if(!base||*(uint16_t *)base!=0x5a4d)return 0;uint32_t pe=*(uint32_t *)(base+0x3c);
    return pe<0x100000&&*(uint32_t *)(base+pe)==0x4550&&*(uint16_t *)(base+pe+4)==0x8664&&*(uint32_t *)(base+pe+8)==0x6afa6a16&&*(uint32_t *)(base+pe+24+56)==8785920;
}
__declspec(dllexport) uint32_t pip_loader_start(void *arg){
    const BootArgs *a=arg;
    if(!a||a->size!=16||a->mode!=3||!a->root)return 1;
    image=GetModuleHandleW(0);if(!host_valid(image))return 2;
    if(installed){__atomic_store_n(&requested,1,__ATOMIC_RELEASE);return 0;}
    if(!open_log(a->root))return 3;
    units=image+0x6b1c80;buildings=image+0x6b8200;
    pip_collision_return=image+0x2290bb;
    log_text("PIP runtime: build 26692741; installing simulation preparation and native movement hooks.\r\n");
    if(!pip_install_hooks(image,(void *)&pip_dispatch_hook,(void *)&pip_move_bridge,(void **)&original_dispatch,&pip_move_return)){
        log_text("Hook installation rejected. No evacuation active.\r\n");return 7;
    }
    installed=1;log_text("Hooks installed. Evacuation begins on the next simulation tick. --disable releases steering immediately.\r\n");return 0;
}
__declspec(dllexport) uint32_t pip_runtime_disable(void *unused){(void)unused;__atomic_store_n(&requested,0,__ATOMIC_RELEASE);__atomic_store_n(&pip_runtime_stats.enabled,0,__ATOMIC_RELEASE);return 0;}
__declspec(dllexport) uint32_t pip_runtime_selftest(void){
    uint32_t r=nav_selftest();if(r)return r;
    bridge_test_branch=0;if(pip_bridge_selftest())return 1000;
    bridge_test_branch=1;r=pip_bridge_selftest();bridge_test_branch=0;return r?1001:0;
}
