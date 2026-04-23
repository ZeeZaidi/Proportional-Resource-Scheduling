#pragma once

#include "hetero_scheduler.h"
#include "core.h"
#include "common.h"
#include <vector>
#include <unordered_map>
#include <string>

// -----------------------------------------------------------------------
// Heterogeneous multicore discrete event simulator.
//
// Adapted from event_simulator.h/.cpp. Key difference: each quantum
// dispatches N allocations (one per core) instead of 1. The simulator
// records which core each winner ran on, enabling work-unit accounting.
//
// DynamicEvent gains a SCALE_CHANGE type that modifies a core's
// performance_scale mid-simulation and calls sched.rescale_remains()
// to maintain the remain invariant for stride schedulers.
// -----------------------------------------------------------------------

struct HeteroDynamicEvent {
    enum Type { JOIN, LEAVE, MODIFY, SCALE_CHANGE };
    int    at_quantum;
    Type   type;
    int    client_id   = 0;
    int    new_tickets = 0;      // used by JOIN, MODIFY
    double new_scale   = 1.0;    // used by SCALE_CHANGE
    int    core_id     = 0;      // used by SCALE_CHANGE: which core to modify
    std::string client_name;     // used by JOIN
};

struct HeteroAllocationRecord {
    int    quantum;
    int    winner_id;
    int    core_id;
    double core_scale;   // S_C at time of allocation
};

struct HeteroClientTrack {
    int    id;
    int    tickets_at_end;
    int    wins              = 0;
    int    first_active      = 0;
    int    last_active       = 0;
    double effective_work    = 0.0;   // Σ core.performance_scale per win
    std::vector<int> inter_win_intervals;
    int    last_win_quantum  = -1;
};

struct HeteroSimResult {
    std::string                                   scheduler_name;
    int                                           total_quanta     = 0;
    double                                        total_work_dispensed = 0.0;
    std::vector<HeteroAllocationRecord>           history;
    std::unordered_map<int, HeteroClientTrack>    tracks;
    std::unordered_map<int, std::string>          client_names;
};

class HeteroEventSimulator {
public:
    HeteroSimResult run(HeteroScheduler&                       sched,
                        const std::vector<SimClient>&           clients,
                        std::vector<Core>&                      cores,
                        int                                     num_quanta,
                        const std::vector<HeteroDynamicEvent>&  events = {});
};
