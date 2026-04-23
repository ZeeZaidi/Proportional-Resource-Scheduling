#include "homo_stride.h"
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <limits>

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

HomoStrideScheduler::HomoStrideScheduler(unsigned seed)
    : rng_(seed) {}

// -----------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------

void HomoStrideScheduler::global_pass_update_time(int n_cores) {
    // Advance global_pass in time units: N cores × STRIDE1 / global_tickets.
    // Each core dispatches 1 quantum → total elapsed = n_cores quanta.
    if (global_tickets_ > 0)
        global_pass_ += (STRIDE1 * static_cast<long long>(n_cores)) / global_tickets_;
}

void HomoStrideScheduler::global_tickets_update(long long delta) {
    global_tickets_ += delta;
}

void HomoStrideScheduler::client_join(HomoStrideClient& c) {
    c.pass   = global_pass_ + c.remain;
    c.active = true;
    global_tickets_update(c.tickets);
}

void HomoStrideScheduler::client_leave(HomoStrideClient& c) {
    c.remain = c.pass - global_pass_;
    c.active = false;
    global_tickets_update(-c.tickets);
}

HomoStrideClient* HomoStrideScheduler::find(int id) {
    for (auto& c : clients_)
        if (c.id == id) return &c;
    return nullptr;
}

// -----------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------

void HomoStrideScheduler::add_client(int id, int tickets) {
    if (tickets <= 0)
        throw std::invalid_argument("Tickets must be positive");
    if (find(id))
        throw std::invalid_argument("Client id already exists");

    HomoStrideClient c;
    c.id      = id;
    c.tickets = tickets;
    c.stride  = STRIDE1 / tickets;
    c.pass    = 0;
    c.remain  = c.stride;
    c.active  = false;

    clients_.push_back(c);
    client_join(clients_.back());
}

void HomoStrideScheduler::remove_client(int id) {
    HomoStrideClient* cp = find(id);
    if (!cp)
        throw std::invalid_argument("Client id not found");
    if (cp->active)
        client_leave(*cp);
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(),
                        [id](const HomoStrideClient& c){ return c.id == id; }),
        clients_.end());
}

void HomoStrideScheduler::modify_client(int id, int new_tickets) {
    if (new_tickets <= 0)
        throw std::invalid_argument("Tickets must be positive");
    HomoStrideClient* cp = find(id);
    if (!cp)
        throw std::invalid_argument("Client id not found");

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
HomoStrideScheduler::allocate_multi(const std::vector<Core>& cores) {
    int n = static_cast<int>(cores.size());

    // Shuffle core order so P-core vs E-core assignment is random.
    std::vector<int> core_order(n);
    std::iota(core_order.begin(), core_order.end(), 0);
    std::shuffle(core_order.begin(), core_order.end(), rng_);

    std::vector<std::pair<int,int>> result;
    result.reserve(n);

    // N sequential min-pass picks — same client may win multiple cores.
    // Pass advances by stride only (no × S_C) — this is the uncompensated
    // baseline that ignores core heterogeneity.
    for (int i = 0; i < n; ++i) {
        HomoStrideClient* best = nullptr;
        for (auto& c : clients_) {
            if (c.active && (!best || c.pass < best->pass))
                best = &c;
        }
        if (!best)
            throw std::runtime_error("No active clients to schedule");

        const Core& core = cores[core_order[i]];
        best->pass += best->stride;   // time units, not work units
        result.push_back({best->id, core.id});
    }

    // global_pass in time units.
    global_pass_update_time(n);

    return result;
}
