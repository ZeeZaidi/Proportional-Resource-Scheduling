#pragma once

#include "common.h"
#include <vector>

// -----------------------------------------------------------------------
// Dynamic stride scheduler.
//
// Implements the full dynamic stride scheduling algorithm described in
// Figures 1, 3, 4, and 5 of Waldspurger & Weihl (MIT-LCS-TM-528, 1995).
//
// Per-client state (Figure 3):
//   tickets  — ticket allocation
//   stride   — stride1 / tickets  (inversely proportional to tickets)
//   pass     — virtual time of next selection
//   remain   — remaining portion of current stride when client is inactive
//   active   — whether the client is in the run queue
//
// Global state (Figure 3):
//   global_tickets — sum of all active client tickets
//   global_pass    — "current" virtual time for the scheduler
//   (global_stride is recomputed as stride1 / global_tickets on demand)
//
// Operations:
//   client_join   — reactivate client; pass = global_pass + remain
//   client_leave  — deactivate client; remain = pass - global_pass
//   client_modify — leave → rescale remain by stride'/stride → join (Figure 5)
//   allocate      — select min-pass active client; advance pass by stride
//                   (nonuniform-quanta form: pass += stride * elapsed / quantum)
//                   In the event simulator, elapsed = 1 quantum always.
// -----------------------------------------------------------------------

struct StrideClient {
    int       id;
    int       tickets;
    long long stride;
    long long pass;
    long long remain;
    bool      active;
};

class StrideScheduler : public Scheduler {
public:
    StrideScheduler();

    void add_client(int id, int tickets) override;   // init + join
    void remove_client(int id) override;              // leave + erase
    void modify_client(int id, int new_tickets) override;
    int  allocate() override;
    std::string name() const override { return "Stride (Dynamic)"; }

    int num_clients() const { return static_cast<int>(clients_.size()); }

private:
    std::vector<StrideClient> clients_;
    long long global_tickets_ = 0;
    long long global_pass_    = 0;
    int       quantum_        = 1;   // time-slice size (1 in discrete sim)

    // Advance global_pass by global_stride * elapsed / quantum.
    // In the discrete simulator elapsed is always 1, so this advances
    // global_pass by stride1 / global_tickets each call.
    void global_pass_update(long long elapsed = 1);

    // Update global_tickets by delta and recompute global_stride.
    void global_tickets_update(long long delta);

    void client_join(StrideClient& c);
    void client_leave(StrideClient& c);

    // Returns pointer to client with given id, or nullptr.
    StrideClient* find(int id);
};
