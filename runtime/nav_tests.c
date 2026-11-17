#include "nav.h"
#include "win.h"
#define CHECK(x,code) do{if(!(x)){nav_destroy(&n);return code;}}while(0)
static uint32_t cell(uint32_t x,uint32_t y){return y*11+x;}
static int portals(Nav *n,int first){
    PipPoint p[5]={nav_point(n,cell(2,3)),nav_point(n,cell(2,3)),nav_point(n,cell(3,3)),nav_point(n,cell(4,3)),nav_point(n,cell(4,3))};
    if(first&&!nav_connect(n,cell(2,3),cell(4,3),p,5))return 0;
    p[0]=nav_point(n,cell(6,3));p[1]=nav_point(n,cell(7,3));p[2]=nav_point(n,cell(8,3));
    return nav_connect(n,cell(6,3),cell(8,3),p,3);
}
static PipDecision step(Nav *n,PipPerson *p,PipState *s){
    uint32_t c=nav_cell(n,p->position);p->region_id=c<n->n?n->label[c]:0;
    uint64_t id=p->region_id?p->region_id:s->origin_region;PipDecision d={0};
    if(id&&id<=n->region_count)pip_step(&n->views[id].world,p,s,&d);
    return d;
}
static int walk(Nav *n,PipPerson *p,PipState *s,uint64_t goal){
    for(uint32_t i=0;i<500;i++){
        for(uint32_t r=1;r<=n->region_count;r++)n->views[r].world.sim_time=(double)i/30;
        PipDecision d=step(n,p,s);if(d.action==PIP_SHELTERED)return s->known_region==goal;
        if(d.action!=PIP_EVACUATE)return 0;
        PipPoint direction;uint32_t c=nav_cell(n,p->position);
        if(!nav_direction(n,c,(uint32_t)d.portal_id,s->origin_region,p->position,&direction))return 0;
        p->position.x+=direction.x*.25f;p->position.y+=direction.y*.25f;
        c=nav_cell(n,p->position);if(c>=n->n||!(n->flags[c]&NAV_WALK))return 0;
    }
    return 0;
}
uint32_t nav_selftest(void){
    Nav n={0};CHECK(nav_create(&n,11,7),1);
    for(uint32_t y=0;y<7;y++)for(uint32_t x=0;x<11;x++){
        if(x==3||x==7)continue;
        n.flags[cell(x,y)]=NAV_INSIDE|NAV_WALK|(x>3?NAV_CALM:0);
    }
    n.flags[cell(3,3)]=NAV_GATE|NAV_WALK|NAV_INSIDE;
    n.flags[cell(7,3)]=NAV_GATE|NAV_WALK|NAV_INSIDE|NAV_CALM;
    CHECK(nav_components(&n)&&n.region_count==3,2);
    CHECK(portals(&n,1)&&nav_finish(&n,1),3);
    CHECK(n.label[cell(2,3)]!=n.label[cell(4,3)],4);
    CHECK(n.distance[cell(0,0)]!=0xffffffffu&&n.portal[cell(0,0)]==1,5);
    CHECK(n.distance[cell(10,0)]==0xffffffffu,6);
    PipState state={0};PipPerson person={1,0,nav_point(&n,cell(0,0)),3,0};
    PipDecision d=step(&n,&person,&state);
    CHECK(d.action==PIP_EVACUATE&&d.target_region==2&&d.portal_id==1,7);
    CHECK(walk(&n,&person,&state,2),8);
    CHECK(state.known_region==2&&state.fallback_region==3&&!state.target_region,9);
    PipPoint old=person.position;d=step(&n,&person,&state);
    CHECK(d.action==PIP_SHELTERED&&person.position.x==old.x&&person.position.y==old.y,10);
    /* Breach the destination after arrival; the same person falls back again. */
    for(uint32_t y=0;y<7;y++)for(uint32_t x=4;x<=6;x++)n.flags[cell(x,y)]&=(uint8_t)~NAV_CALM;
    CHECK(nav_components(&n)&&portals(&n,1)&&nav_finish(&n,2),11);
    d=step(&n,&person,&state);CHECK(d.action==PIP_EVACUATE&&d.target_region==3,12);
    CHECK(walk(&n,&person,&state,3)&&state.known_region==3&&!state.fallback_region,13);
    /* No traversal through a second breached chamber to reach an intact third. */
    person.position=nav_point(&n,cell(0,0));pip_reset(&state);
    d=step(&n,&person,&state);CHECK(d.action==PIP_NO_ROUTE,14);
    /* Closing the only portal leaves the outer chamber on native behaviour. */
    for(uint32_t y=0;y<7;y++)for(uint32_t x=4;x<=6;x++)n.flags[cell(x,y)]|=NAV_CALM;
    CHECK(nav_components(&n)&&portals(&n,0)&&nav_finish(&n,3),15);
    pip_reset(&state);d=step(&n,&person,&state);CHECK(d.action==PIP_NO_ROUTE,16);
    /* A wall/water cell never belongs to the route field, even if inside. */
    CHECK(!n.label[cell(3,0)]&&n.distance[cell(3,0)]==0xffffffffu,17);
    /* Reopening restores a route on the next topology revision. */
    CHECK(nav_components(&n)&&portals(&n,1)&&nav_finish(&n,4),18);
    d=step(&n,&person,&state);CHECK(d.action==PIP_EVACUATE,19);
    person.flags|=PIP_PERSON_INFECTED;d=step(&n,&person,&state);
    CHECK(d.action==PIP_VANILLA&&!state.target_region,20);
    person.flags=1;d=step(&n,&person,&state);CHECK(d.action==PIP_VANILLA,21);
    nav_destroy(&n);return 0;
}
