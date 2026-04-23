#include "stride_scheduler.h"
#include <stdexcept>
#include <algorithm>
#include <limits>

StrideScheduler::StrideScheduler() {}

// -----------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------

void StrideScheduler::global_pass_update(long long elapsed) {
    if (global_tickets_ > 0) {
        // global_stride = STRIDE1 / global_tickets
        // global_pass  += global_stride * elapsed / quantum
        // With elapsed = quantum = 1 this simplifies to += STRIDE1 / global_tickets.
        global_pass_ += (STRIDE1 * elapsed) / (global_tickets_ * quantum_);
    }
}

void StrideScheduler::global_tickets_update(long long delta) {
    global_tickets_ += delta;
    // global_stride is recomputed on-the-fly from global_tickets_ wherever needed.
}

void StrideScheduler::client_join(StrideClient& c) {
    global_pass_update();
    c.pass   = global_pass_ + c.remain;
    c.active = true;
    global_tickets_update(c.tickets);
}

void StrideScheduler::client_leave(StrideClient& c) {
    global_pass_update();
    c.remain = c.pass - global_pass_;
    c.active = false;
    global_tickets_update(-c.tickets);
}

StrideClient* StrideScheduler::find(int id) {
    for (auto& c : clients_)
        if (c.id == id) return &c;
    return nullptr;
}

// -----------------------------------------------------------------------
// Public Scheduler interface
// -----------------------------------------------------------------------

void StrideScheduler::add_client(int id, int tickets) {
    if (tickets <= 0)
        throw std::invalid_argument("Tickets must be positive");
    if (find(id))
        throw std::invalid_argument("Client id already exists");

    StrideClient c;
    c.id      = id;
    c.tickets = tickets;
    c.stride  = STRIDE1 / tickets;
    c.pass    = 0;
    c.remain  = c.stride;   // whole stride remaining (client_init in Figure 3)
    c.active  = false;

    clients_.push_back(c);
    client_join(clients_.back());
}

void StrideScheduler::remove_client(int id) {
    StrideClient* cp = find(id);
    if (!cp)
        throw std::invalid_argument("Client id not found");
    if (cp->active)
        client_leave(*cp);
    clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
                                  [id](const StrideClient& c){ return c.id == id; }),
                   clients_.end());
}

void StrideScheduler::modify_client(int id, int new_tickets) {
    if (new_tickets <= 0)
        throw std::invalid_argument("Tickets must be positive");
    StrideClient* cp = find(id);
    if (!cp)
        throw std::invalid_argument("Client id not found");

    // Figure 5: leave → recompute stride → scale remain → join
    client_leave(*cp);

    long long old_stride = cp->stride;
    cp->tickets = new_tickets;
    cp->stride  = STRIDE1 / new_tickets;

    // Scale remaining passes: remain' = remain * stride' / stride
    if (old_stride > 0)
        cp->remain = (cp->remain * cp->stride) / old_stride;
    else
        cp->remain = cp->stride;

    client_join(*cp);
}

int StrideScheduler::allocate() {
    // Select the active client with the minimum pass value.
    StrideClient* winner = nullptr;
    long long min_pass = std::numeric_limits<long long>::max();

    for (auto& c : clients_) {
        if (c.active && c.pass < min_pass) {
            min_pass = c.pass;
            winner   = &c;
        }
    }

    if (!winner)
        throw std::runtime_error("No active clients to schedule");

    // Advance winner's pass by stride (elapsed = 1 quantum).
    winner->pass += winner->stride;

    return winner->id;
}
