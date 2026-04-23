#pragma once

#include "common.h"
#include <vector>
#include <unordered_map>

// -----------------------------------------------------------------------
// Discrete event simulator.
//
// The simulator drives a Scheduler through a sequence of quanta (discrete
// time steps).  At each quantum it:
//   1. Processes any dynamic events scheduled for that quantum.
//   2. Calls scheduler.allocate() to select the winning client.
//   3. Records the allocation and updates per-client tracking data.
//
// DynamicEvent types:
//   JOIN    — add a new client (id, tickets) at the given quantum
//   LEAVE   — remove a client at the given quantum
//   MODIFY  — change an existing client's ticket count
// -----------------------------------------------------------------------

struct DynamicEvent {
    enum Type { JOIN, LEAVE, MODIFY };
    int  at_quantum;
    Type type;
    int  client_id;
    int  new_tickets = 0;   // used by JOIN and MODIFY
    std::string client_name;  // used by JOIN for reporting
};

struct AllocationRecord {
    int quantum;
    int winner_id;
};

// Per-client tracking accumulated during simulation.
struct ClientTrack {
    int id;
    int tickets_at_end;     // ticket count at the last quantum the client was active
    int wins;               // total quanta awarded
    int first_active;       // first quantum the client was present
    int last_active;        // last quantum the client was present
    // Inter-win intervals: time (in quanta) between consecutive allocations.
    // Used to compute mean response time and jitter (std dev).
    std::vector<int> inter_win_intervals;
    int last_win_quantum;   // quantum of most recent win (-1 if never won)
};

struct SimResult {
    std::string                              scheduler_name;
    int                                      total_quanta;
    std::vector<AllocationRecord>            history;
    // Keyed by client id.
    std::unordered_map<int, ClientTrack>     tracks;
    // Snapshot of client names for reporting.
    std::unordered_map<int, std::string>     client_names;
};

class EventSimulator {
public:
    // Run the simulation.
    //
    // sched        — the scheduler to drive (must be freshly constructed/empty)
    // clients      — initial set of clients added before quantum 1
    // num_quanta   — total number of time steps to simulate
    // events       — dynamic events (join/leave/modify) at specific quanta;
    //                need not be sorted (the simulator sorts them internally)
    SimResult run(Scheduler&                        sched,
                  const std::vector<SimClient>&     clients,
                  int                               num_quanta,
                  const std::vector<DynamicEvent>&  events = {});
};
