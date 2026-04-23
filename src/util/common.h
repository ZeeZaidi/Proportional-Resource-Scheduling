#pragma once

#include <string>
#include <vector>
#include <stdexcept>

// -----------------------------------------------------------------------
// Stride constant (Appendix A of stride paper).
// stride1 = 1<<20; stride = stride1 / tickets.
// Using long long throughout to avoid overflow: with stride1=2^20 and
// worst-case tickets=1, ~2^44 allocations fit before overflow.
// -----------------------------------------------------------------------
static constexpr long long STRIDE1 = 1LL << 20;

// -----------------------------------------------------------------------
// SimClient: descriptor used by the simulator layer.
// -----------------------------------------------------------------------
struct SimClient {
    int         id;
    std::string name;
    int         tickets;
};

// -----------------------------------------------------------------------
// Scheduler: abstract base class shared by all four implementations.
// -----------------------------------------------------------------------
class Scheduler {
public:
    // Add a new client with the given ticket allocation.
    virtual void add_client(int id, int tickets) = 0;

    // Remove a client that was previously added.
    virtual void remove_client(int id) = 0;

    // Change an existing client's ticket allocation dynamically.
    virtual void modify_client(int id, int new_tickets) = 0;

    // Run one allocation quantum; returns the winning client's id.
    virtual int allocate() = 0;

    // Human-readable name for reporting.
    virtual std::string name() const = 0;

    virtual ~Scheduler() = default;
};
