#include "nav.h"
#include "win.h"
#define NONE 0xffffffffu
static float square(float x){return x*x;}
static float norm(PipPoint p){return __builtin_sqrtf(square(p.x)+square(p.y));}
void nav_destroy(Nav *n){
    release(n->flags);release(n->label);release(n->distance);release(n->portal);release(n->queue);
    release(n->regions);release(n->portals);release(n->views);release(n->view_regions);release(n->view_portals);
    bytes_zero(n,sizeof(*n));
}
int nav_create(Nav *n,uint32_t w,uint32_t h){
    nav_destroy(n);
    if(!w||!h||(uint64_t)w*h>NAV_MAX_CELLS)return 0;
    n->w=w;n->h=h;n->n=w*h;n->cell_size=2;
    n->flags=alloc(n->n);n->label=alloc((size_t)n->n*4);n->distance=alloc((size_t)n->n*4);
    n->portal=alloc((size_t)n->n*4);n->queue=alloc((size_t)n->n*4);
    n->regions=alloc((NAV_MAX_REGIONS+1)*sizeof(PipRegion));
    n->portals=alloc(NAV_MAX_PORTALS*sizeof(NavPortal));n->views=alloc((NAV_MAX_REGIONS+1)*sizeof(NavView));
    if(!n->flags||!n->label||!n->distance||!n->portal||!n->queue||!n->regions||!n->portals||!n->views){nav_destroy(n);return 0;}
    return 1;
}
static uint32_t adjacent(const Nav *n,uint32_t c,uint32_t d){
    if(d==0)return c%n->w?c-1:NONE;
    if(d==1)return c%n->w+1<n->w?c+1:NONE;
    if(d==2)return c>=n->w?c-n->w:NONE;
    return c+n->w<n->n?c+n->w:NONE;
}
int nav_components(Nav *n){
    n->region_count=0;n->portal_count=0;
    bytes_zero(n->label,(size_t)n->n*4);
    for(uint32_t c=0;c<n->n;c++){
        uint8_t f=n->flags[c];
        if(n->label[c]||(f&(NAV_INSIDE|NAV_WALK|NAV_GATE))!=(NAV_INSIDE|NAV_WALK))continue;
        if(n->region_count==NAV_MAX_REGIONS)return 0;
        uint32_t id=++n->region_count,head=0,tail=1;n->queue[0]=c;n->label[c]=id;
        n->regions[id]=(PipRegion){id,(f&NAV_CALM)?PIP_REGION_ACTIVE:PIP_REGION_ACTIVE|PIP_REGION_BREACHED,0};
        while(head<tail){uint32_t at=n->queue[head++];for(uint32_t d=0;d<4;d++){
            uint32_t next=adjacent(n,at,d);
            if(next==NONE||n->label[next]||n->flags[next]!=f)continue;
            n->label[next]=id;n->queue[tail++]=next;
        }}
    }
    return 1;
}
PipPoint nav_point(const Nav *n,uint32_t c){return (PipPoint){((float)n->x0+(float)(c%n->w)+.5f)*n->cell_size,((float)n->y0+(float)(c/n->w)+.5f)*n->cell_size};}
uint32_t nav_cell(const Nav *n,PipPoint p){
    if(!(p.x>=0&&p.y>=0&&p.x<1e8f&&p.y<1e8f)||!n->n)return NONE;
    int64_t x=(int64_t)(p.x/n->cell_size)-n->x0,y=(int64_t)(p.y/n->cell_size)-n->y0;
    if(x<0||y<0||x>=n->w||y>=n->h)return NONE;
    return (uint32_t)y*n->w+(uint32_t)x;
}
int nav_connect(Nav *n,uint32_t a,uint32_t b,const PipPoint *path,uint32_t count){
    if(a>=n->n||b>=n->n||!n->label[a]||!n->label[b]||n->label[a]==n->label[b]||!count||count>NAV_PATH_POINTS||n->portal_count==NAV_MAX_PORTALS)return 0;
    NavPortal *p=&n->portals[n->portal_count++];bytes_zero(p,sizeof(*p));
    p->policy=(PipPortal){n->portal_count,n->label[a],n->label[b],nav_point(n,a),nav_point(n,b),7,0};
    p->cell_a=a;p->cell_b=b;p->path_count=count;
    for(uint32_t i=0;i<count;i++)p->path[i]=path[i];
    return 1;
}
int nav_finish(Nav *n,uint64_t revision){
    n->revision=revision;
    for(uint32_t i=1;i<=n->region_count;i++)n->views[i].degree=0;
    for(uint32_t i=0;i<n->portal_count;i++){n->views[n->portals[i].policy.region_a].degree++;n->views[n->portals[i].policy.region_b].degree++;}
    release(n->view_regions);release(n->view_portals);
    n->view_regions=alloc(((size_t)n->region_count+2*n->portal_count)*sizeof(PipRegion));
    n->view_portals=alloc((size_t)(2*n->portal_count+1)*sizeof(PipPortal));
    if(!n->view_regions||!n->view_portals)return 0;
    uint32_t ro=0,po=0;
    for(uint32_t id=1;id<=n->region_count;id++){
        NavView *v=&n->views[id];v->offset=0;
        v->world=(PipWorld){1,0,revision,0,n->view_regions+ro,v->degree+1,v->degree,n->view_portals+po,nav_query,n,.5f,0};
        n->view_regions[ro]=n->regions[id];ro+=v->degree+1;po+=v->degree;
    }
    for(uint32_t i=0;i<n->portal_count;i++){
        PipPortal p=n->portals[i].policy;
        uint32_t ids[2]={(uint32_t)p.region_a,(uint32_t)p.region_b};
        for(uint32_t j=0;j<2;j++){
            NavView *v=&n->views[ids[j]];uint32_t k=v->offset++;
            ((PipPortal *)v->world.portals)[k]=p;
            ((PipRegion *)v->world.regions)[k+1]=n->regions[ids[1-j]];
        }
    }
    for(uint32_t c=0;c<n->n;c++){n->distance[c]=NONE;n->portal[c]=0;}
    uint32_t head=0,tail=0;
    for(uint32_t i=0;i<n->portal_count;i++){
        NavPortal *p=&n->portals[i];uint32_t ends[2]={p->cell_a,p->cell_b};
        for(uint32_t j=0;j<2;j++){
            uint32_t a=ends[j],b=ends[1-j];
            if((n->regions[n->label[a]].flags&PIP_REGION_BREACHED)&&!(n->regions[n->label[b]].flags&PIP_REGION_BREACHED)&&n->distance[a]==NONE){
                n->distance[a]=0;n->portal[a]=i+1;n->queue[tail++]=a;
            }
        }
    }
    while(head<tail){uint32_t c=n->queue[head++];for(uint32_t d=0;d<4;d++){
        uint32_t next=adjacent(n,c,d);
        if(next==NONE||n->label[next]!=n->label[c]||n->distance[next]!=NONE)continue;
        n->distance[next]=n->distance[c]+1;n->portal[next]=n->portal[c];n->queue[tail++]=next;
    }}
    return 1;
}
int32_t nav_query(void *context,const PipPerson *p,uint64_t portal,uint64_t target,PipPoint waypoint,float *distance){
    (void)target;(void)waypoint;
    const Nav *n=context;uint32_t c=nav_cell(n,p->position);
    if(c>=n->n||portal==0||portal>n->portal_count)return 0;
    if(n->portal[c]==portal&&n->distance[c]!=NONE){*distance=(float)n->distance[c]*n->cell_size+norm((PipPoint){n->portals[portal-1].policy.inside_a.x-n->portals[portal-1].policy.inside_b.x,n->portals[portal-1].policy.inside_a.y-n->portals[portal-1].policy.inside_b.y});return 1;}
    /* A person already in the doorway retains the validated connection. */
    if(!n->label[c]&&(n->flags[c]&NAV_GATE)){
        const NavPortal *q=&n->portals[portal-1];
        PipPoint a=q->policy.inside_a,b=q->policy.inside_b;
        float ab=norm((PipPoint){a.x-b.x,a.y-b.y});
        if(norm((PipPoint){p->position.x-a.x,p->position.y-a.y})<ab+2*n->cell_size&&norm((PipPoint){p->position.x-b.x,p->position.y-b.y})<ab+2*n->cell_size){*distance=ab;return 1;}
    }
    return 0;
}
int nav_direction(const Nav *n,uint32_t c,uint32_t portal,uint64_t origin,PipPoint at,PipPoint *out){
    if(c>=n->n||!portal||portal>n->portal_count)return 0;
    const NavPortal *p=&n->portals[portal-1];
    if(origin!=p->policy.region_a&&origin!=p->policy.region_b)return 0;
    PipPoint goal;
    if(n->label[c]==origin&&n->distance[c]>0&&n->distance[c]!=NONE){
        /* Follow cardinal descent. Never cut a diagonal through a wall corner. */
        uint32_t best=c;float score=1e30f;
        for(uint32_t d=0;d<4;d++){uint32_t next=adjacent(n,c,d);if(next==NONE||n->label[next]!=origin||n->portal[next]!=portal||n->distance[next]>=n->distance[c])continue;
            PipPoint q=nav_point(n,next);float s=square(q.x-at.x)+square(q.y-at.y);
            if(s<score){best=next;score=s;}
        }
        if(best==c)return 0;
        goal=nav_point(n,best);
    }else{
        /* Choose the nearest point on the native gate path and advance along it. */
        uint32_t nearest=0;float best=1e30f;
        for(uint32_t i=0;i<p->path_count;i++){float d=square(p->path[i].x-at.x)+square(p->path[i].y-at.y);if(d<best){best=d;nearest=i;}}
        int forward=origin==p->policy.region_a;
        while(best<square(n->cell_size*.65f)){
            if(forward&&nearest+1<p->path_count)nearest++;
            else if(!forward&&nearest)nearest--;
            else break;
            best=square(p->path[nearest].x-at.x)+square(p->path[nearest].y-at.y);
        }
        goal=p->path[nearest];
    }
    PipPoint v={goal.x-at.x,goal.y-at.y};float length=norm(v);
    if(!(length>.01f&&length<1e6f))return 0;
    *out=(PipPoint){v.x/length,v.y/length};return 1;
}
