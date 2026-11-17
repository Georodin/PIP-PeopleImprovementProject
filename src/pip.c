#include "pip.h"

/* Clang/MSVC floating-point marker; this freestanding DLL needs no CRT. */
#ifdef _WIN32
int _fltused = 0;
#endif
_Static_assert(sizeof(PipWorld) == 72, "PIP requires the x64 ABI");
_Static_assert(sizeof(PipState) == 64, "PIP state ABI");
_Static_assert(sizeof(PipDecision) == 32, "PIP decision ABI");

static int finite_float(float x) { return x == x && x <= 3.402823466e38F && x >= -3.402823466e38F; }
static int finite_double(double x) { return x == x && x <= 1.7976931348623157e308 && x >= -1.7976931348623157e308; }
static int point_valid(PipPoint p) { return finite_float(p.x) && finite_float(p.y); }
static const PipRegion *region(const PipWorld *w, uint64_t id) {
    if (!id) return 0;
    for (uint32_t i = 0; i < w->region_count; ++i)
        if (w->regions[i].id == id) return &w->regions[i];
    return 0;
}
static int safe(const PipRegion *r) {
    return r && (r->flags & PIP_REGION_ACTIVE) && !(r->flags & PIP_REGION_BREACHED);
}
static int destination(const PipWorld *w, const PipPortal *p, uint64_t origin,
                       uint64_t *target, PipPoint *point) {
    if (!p->id || !(p->flags & PIP_PORTAL_OPEN) || !finite_float(p->threat_cost) || p->threat_cost < 0) return 0;
    if (p->region_a == origin && (p->flags & PIP_PORTAL_A_TO_B)) {
        *target = p->region_b; *point = p->inside_b;
    } else if (p->region_b == origin && (p->flags & PIP_PORTAL_B_TO_A)) {
        *target = p->region_a; *point = p->inside_a;
    } else return 0;
    return *target != origin && point_valid(*point) && safe(region(w, *target));
}
static void clear_route(PipState *s) {
    s->origin_region = 0; s->target_region = 0; s->target_portal = 0;
    s->next_replan_time = 0;
}
static void learn(const PipWorld *w, const PipPerson *person, PipState *s) {
    /* Learn a contingency without moving or pathfinding for a sheltered person.
     * Actual reachability is checked at breach time from the person's position. */
    s->known_region = person->region_id;
    s->informed_revision = w->revision;
    s->fallback_portal = 0; s->fallback_region = 0;
    float best_threat = 3.402823466e38F;
    for (uint32_t i = 0; i < w->portal_count; ++i) {
        const PipPortal *p = &w->portals[i];
        uint64_t target; PipPoint point;
        if (!destination(w, p, person->region_id, &target, &point)) continue;
        if (p->threat_cost < best_threat ||
            (p->threat_cost == best_threat && (!s->fallback_portal || p->id < s->fallback_portal))) {
            best_threat = p->threat_cost; s->fallback_portal = p->id; s->fallback_region = target;
        }
    }
}
static void output(PipDecision *d, uint32_t action, uint64_t target, uint64_t portal, PipPoint point) {
    d->action = action; d->reserved = 0;
    d->target_region = target; d->portal_id = portal; d->waypoint = point;
}
PIP_API uint32_t PIP_CALL pip_abi_version(void) { return PIP_ABI_VERSION; }
PIP_API void PIP_CALL pip_reset(PipState *s) {
    if (!s) return;
    s->known_region = 0; s->informed_revision = 0;
    s->fallback_portal = 0; s->fallback_region = 0;
    clear_route(s);
}
PIP_API uint32_t PIP_CALL pip_step(const PipWorld *w, const PipPerson *person,
                                 PipState *s, PipDecision *d) {
    if (!d) return PIP_INVALID_INPUT;
    PipPoint zero = {0, 0};
    output(d, PIP_VANILLA, 0, 0, zero);
    if (!w || !person || !s) return PIP_INVALID_INPUT;
    if (w->abi_version != PIP_ABI_VERSION) return PIP_ABI_MISMATCH;
    if ((w->region_count && !w->regions) || (w->portal_count && !w->portals) ||
        !person->id || !point_valid(person->position) || !finite_double(w->sim_time) || w->sim_time < 0 ||
        !finite_float(w->retry_seconds) || w->retry_seconds <= 0 || w->retry_seconds > 60)
        return PIP_INVALID_INPUT;
    if ((person->flags & (PIP_PERSON_ALIVE | PIP_PERSON_CIVILIAN)) !=
        (PIP_PERSON_ALIVE | PIP_PERSON_CIVILIAN) || (person->flags & PIP_PERSON_INFECTED)) {
        pip_reset(s); return PIP_OK;
    }
    const PipRegion *current = region(w, person->region_id);
    if (safe(current)) {
        /* Physical region membership is the arrival signal, never proximity to
         * a wall or an invented teleport. Overlap regions are independent. */
        if (s->known_region != person->region_id || s->informed_revision != w->revision)
            learn(w, person, s);
        clear_route(s);
        output(d, PIP_SHELTERED, 0, 0, zero);
        return PIP_OK;
    }
    if (current && (current->flags & PIP_REGION_ACTIVE) && (current->flags & PIP_REGION_BREACHED)) {
        if (s->origin_region != current->id) {
            clear_route(s); s->origin_region = current->id;
        }
    } else if (person->region_id || !s->origin_region) {
        /* Missing/deleted regions are not evidence of a breach. Outside people
         * are left to vanilla unless already travelling out of a breached zone. */
        pip_reset(s); return PIP_OK;
    }
    if (!region(w, s->origin_region)) { pip_reset(s); return PIP_OK; }

    const PipPortal *active = 0;
    PipPoint active_point = zero;
    for (uint32_t i = 0; i < w->portal_count; ++i) {
        const PipPortal *p = &w->portals[i];
        uint64_t target; PipPoint point;
        if (p->id == s->target_portal && destination(w, p, s->origin_region, &target, &point) && target == s->target_region) {
            active = p; active_point = point; break;
        }
    }
    if (active && s->informed_revision == w->revision && w->sim_time < s->next_replan_time) {
        output(d, PIP_EVACUATE, s->target_region, active->id, active_point);
        return PIP_OK;
    }
    /* Check real paths only on breach, topology change, and bounded retries.
     * A blocked attempt is retried even when no topology event was delivered. */
    if (!s->target_portal && s->informed_revision == w->revision && w->sim_time < s->next_replan_time) {
        output(d, PIP_NO_ROUTE, 0, 0, zero); return PIP_OK;
    }
    float best = 3.402823466e38F;
    uint64_t best_portal = 0, best_target = 0;
    PipPoint best_point = zero;
    if (w->query_path) for (uint32_t i = 0; i < w->portal_count; ++i) {
        const PipPortal *p = &w->portals[i];
        uint64_t target; PipPoint point;
        if (!destination(w, p, s->origin_region, &target, &point)) continue;
        float length = -1;
        if (w->query_path(w->context, person, p->id, target, point, &length) != 1 ||
            !finite_float(length) || length < 0) continue;
        float score = length + p->threat_cost;
        if (!finite_float(score)) continue;
        if (score < best || (score == best && (!best_portal || p->id < best_portal))) {
            best = score; best_portal = p->id; best_target = target; best_point = point;
        }
    }
    s->informed_revision = w->revision;
    s->next_replan_time = w->sim_time + w->retry_seconds;
    s->target_portal = best_portal; s->target_region = best_target;
    output(d, best_portal ? PIP_EVACUATE : PIP_NO_ROUTE, best_target, best_portal, best_point);
    return PIP_OK;
}

