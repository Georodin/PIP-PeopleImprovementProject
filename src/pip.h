#ifndef PIP_H
#define PIP_H
#include <stdint.h>

#ifdef _WIN32
#define PIP_API __declspec(dllexport)
#define PIP_CALL __cdecl
#else
#define PIP_API __attribute__((visibility("default")))
#define PIP_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif

/* This is PIP's proposed host ABI, NOT an API supplied by Zombies Aren't Real.
 * All structures use normal 8-byte packing. Call only on the simulation thread.
 * IDs must be stable within a session; zero means none. Reset state on load. */
enum { PIP_ABI_VERSION = 1 };
enum { PIP_REGION_ACTIVE = 1, PIP_REGION_BREACHED = 2 };
enum { PIP_PORTAL_OPEN = 1, PIP_PORTAL_A_TO_B = 2, PIP_PORTAL_B_TO_A = 4 };
enum { PIP_PERSON_ALIVE = 1, PIP_PERSON_CIVILIAN = 2, PIP_PERSON_INFECTED = 4 };
enum { PIP_VANILLA = 0, PIP_SHELTERED = 1, PIP_EVACUATE = 2, PIP_NO_ROUTE = 3 };
enum { PIP_OK = 0, PIP_INVALID_INPUT = 1, PIP_ABI_MISMATCH = 2 };

typedef struct PipPoint { float x, y; } PipPoint;
typedef struct PipRegion { uint64_t id; uint32_t flags, reserved; } PipRegion;
typedef struct PipPortal {
    uint64_t id, region_a, region_b;
    PipPoint inside_a, inside_b;
    uint32_t flags;
    float threat_cost; /* non-negative path-equivalent penalty, supplied by host */
} PipPortal;
typedef struct PipPerson {
    uint64_t id, region_id;
    PipPoint position;
    uint32_t flags, reserved;
} PipPerson;
typedef struct PipState {
    uint64_t known_region, informed_revision, fallback_portal, fallback_region;
    uint64_t origin_region, target_region, target_portal;
    double next_replan_time;
} PipState;
typedef struct PipDecision {
    uint32_t action, reserved;
    uint64_t target_region, portal_id;
    PipPoint waypoint;
} PipDecision;

/* Return 1 only if the HOST finds a complete legal walking path through this
 * portal to waypoint inside target_region. Return 0 for closed/one-way walls,
 * disconnected ground, water, buildings, or a route through another breached
 * region. Write finite non-negative walking length to out_distance.
 * This callback must NOT mutate person/world or re-enter pip_step. */
typedef int32_t (PIP_CALL *PipQueryPath)(
    void *context, const PipPerson *person, uint64_t portal_id,
    uint64_t target_region, PipPoint waypoint, float *out_distance);

typedef struct PipWorld {
    uint32_t abi_version, reserved;
    uint64_t revision; /* increment on breach, portal, or region changes */
    double sim_time;
    const PipRegion *regions;
    uint32_t region_count, portal_count;
    const PipPortal *portals;
    PipQueryPath query_path;
    void *context;
    float retry_seconds, reserved2; /* 0.5 seconds recommended; >0 required */
} PipWorld;

PIP_API uint32_t PIP_CALL pip_abi_version(void);
PIP_API void PIP_CALL pip_reset(PipState *state);
/* Never moves a person or changes the world. On invalid input, state is unchanged
 * and the result is VANILLA. Host must release any previous override then. */
PIP_API uint32_t PIP_CALL pip_step(const PipWorld *world, const PipPerson *person,
                                 PipState *state, PipDecision *decision);
#ifdef __cplusplus
}
#endif
#endif

