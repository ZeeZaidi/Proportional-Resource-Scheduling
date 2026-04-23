#include "hetero_stride.h"
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <limits>
#include <cmath>

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

HeteroStrideScheduler::HeteroStrideScheduler(unsigned seed)
    : rng_(seed) {}

// -----------------------------------------------------------------------
// Internal helpers — adapted from stride_scheduler.cpp
// -----------------------------------------------------------------------

void HeteroStrideScheduler::global_pass_update(long long total_work_int) {
    // Advance global_pass by the amount of work dispensed this quantum,
    // expressed in work units: (STRIDE1 × total_work_int) / (S_DENOM × global_tickets).
    // Overflow analysis: max total_work_int = 8 cores × 2000 = 16000.
    // STRIDE1 × 16000 = 2^20 × 16000 ≈ 1.68×10^10 per quantum.
    // Over 5000 quanta: ≈ 8.4×10^13 << long long max 9.2×10^18. Safe.
    if (global_tickets_ > 0)
        global_pass_ += (STRIDE1 * total_work_int) / (S_DENOM * global_tickets_);
}

void HeteroStrideScheduler::global_tickets_update(long long delta) {
    global_tickets_ += delta;
}

void HeteroStrideScheduler::client_join(HeteroStrideClient& c) {
    global_pass_update(0);   // no work dispensed on join, just sync
    c.pass   = global_pass_ + c.remain;
    c.active = true;
    global_tickets_update(c.tickets);
}

void HeteroStrideScheduler::client_leave(HeteroStrideClient& c) {
    global_pass_update(0);
    c.remain = c.pass - global_pass_;
    c.active = false;
    global_tickets_update(-c.tickets);
}

HeteroStrideClient* HeteroStrideScheduler::find(int id) {
    for (auto& c : clients_)
        if (c.id == id) return &c;
    return nullptr;
}

// -----------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------

void HeteroStrideScheduler::add_client(int id, int tickets) {
    if (tickets <= 0)
        throw std::invalid_argument("Tickets must be positive");
    if (find(id))
        throw std::invalid_argument("Client id already exists");

    HeteroStrideClient c;
    c.id      = id;
    c.tickets = tickets;
    c.stride  = STRIDE1 / tickets;
    c.pass    = 0;
    c.remain  = c.stride;   // full stride remaining (client_init, Figure 3)
    c.active  = false;

    clients_.push_back(c);
    client_join(clients_.back());
}

void HeteroStrideScheduler::remove_client(int id) {
    HeteroStrideClient* cp = find(id);
    if (!cp)
        throw std::invalid_argument("Client id not found");
    if (cp->active)
        client_leave(*cp);
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
                        [id](const HeteroStrideClient& c){ return c.id == id; }),
        clients_.end());
}

void HeteroStrideScheduler::modify_client(int id, int new_tickets) {
    if (new_tickets <= 0)
        throw std::invalid_argument("Tickets must be positive");
    HeteroStrideClient* cp = find(id);
    if (!cp)
        throw std::invalid_argument("Client id not found");

    // Figure 5: leave → recompute stride → scale remain → join
    client_leave(*cp);

    long long old_stride = cp->stride;
    cp->tickets = new_tickets;
    cp->stride  = STRIDE1 / new_tickets;

    if (old_stride > 0)
        cp->remain = (cp->remain * cp->stride) / old_stride;
    else
        cp->remain = cp->stride;

    client_join(*cp);
}

std::vector<std::pair<int,int>>
HeteroStrideScheduler::allocate_multi(const std::vector<Core>& cores) {
    int n = static_cast<int>(cores.size());

    // Shuffle core order so that P-core vs E-core assignment is random.
    std::vector<int> core_order(n);
    std::iota(core_order.begin(), core_order.end(), 0);
    std::shuffle(core_order.begin(), core_order.end(), rng_);

    std::vector<std::pair<int,int>> result;
    result.reserve(n);
    long long total_work_int = 0;

    // N sequential min-pass picks.  A client may win multiple cores in
    // one quantum — its pass advances after each win, so higher-ticket
    // clients (lower stride) naturally accumulate more cores.
    for (int i = 0; i < n; ++i) {
        // Find the active client with the smallest pass value.
        HeteroStrideClient* best = nullptr;
        for (auto& c : clients_) {
            if (c.active && (!best || c.pass < best->pass))
                best = &c;
        }
        if (!best)
            throw std::runtime_error("No active clients to schedule");

        const Core& core = cores[core_order[i]];
        long long sc = core.scale_int();

        // Advance winner's pass by stride × S_C (work-unit compensation).
        best->pass += (best->stride * sc) / S_DENOM;
        result.push_back({best->id, core.id});
        total_work_int += sc;
    }

    // Advance global_pass by total work dispensed.
    global_pass_update(total_work_int);

    return result;
}

void HeteroStrideScheduler::rescale_remains(double ratio) {
    // Called on SCALE_CHANGE to maintain the remain invariant.
    // All inactive clients have remain calibrated to the old work rate;
    // rescale to the new rate so credit is preserved proportionally.
    for (auto& c : clients_)
        if (!c.active)
            c.remain = static_cast<long long>(std::round(c.remain * ratio));
}
