#pragma once

#include "core.h"
#include "common.h"       // SimClient, STRIDE1 — via -Isrc
#include <vector>
#include <utility>
#include <string>

// -----------------------------------------------------------------------
// Abstract base class for heterogeneous multicore schedulers.
//
// Replaces the homogeneous Scheduler::allocate() with allocate_multi(),
// which selects one winner per core per quantum. Each (winner_id, core_id)
// pair lets the scheduler (and simulator) know which core each winner ran
// on, enabling work-unit accounting.
//
// rescale_remains() is called by the simulator when a SCALE_CHANGE event
// fires to maintain the remain invariant for stride schedulers. Lottery
// schedulers inherit the empty default (no state to adjust).
// -----------------------------------------------------------------------

class HeteroScheduler {
public:
    virtual void add_client(int id, int tickets)        = 0;
    virtual void remove_client(int id)                  = 0;
    virtual void modify_client(int id, int new_tickets) = 0;

    // Select cores.size() distinct winners and assign them to cores.
    // Returns vector of (winner_id, core_id) pairs, length = cores.size().
    virtual std::vector<std::pair<int,int>>
        allocate_multi(const std::vector<Core>& cores)  = 0;

    // Rescale all inactive clients' remain values when the global work rate
    // changes (e.g. due to SCALE_CHANGE). ratio = new_total_scale / old_total_scale.
    // Default is no-op; stride schedulers override.
    virtual void rescale_remains(double /*ratio*/) {}

    virtual std::string name() const = 0;
    virtual ~HeteroScheduler() = default;
};
