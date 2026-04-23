#pragma once

#include "hetero_scheduler.h"
#include <vector>
#include <random>

// -----------------------------------------------------------------------
// Uncompensated (baseline) stride scheduler for heterogeneous cores.
//
// Identical to HeteroStrideScheduler except:
//   - pass advances by stride only (no × S_C)
//   - global_pass tracks time units (N quanta), not work units
//   - rescale_remains() is a no-op
//
// The simulator still records effective_work_received per win using the
// actual core's performance_scale, so computational_error is measurable.
// This lets us isolate the effect of S_C compensation by comparison.
// -----------------------------------------------------------------------

struct HomoStrideClient {
    int       id;
    int       tickets;
    long long stride;
    long long pass;
    long long remain;
    bool      active;
};

class HomoStrideScheduler : public HeteroScheduler {
public:
    explicit HomoStrideScheduler(unsigned seed = 42);

    void add_client(int id, int tickets) override;
    void remove_client(int id) override;
    void modify_client(int id, int new_tickets) override;

    std::vector<std::pair<int,int>>
        allocate_multi(const std::vector<Core>& cores) override;

    // No-op: uncompensated scheduler does not track work units.
    void rescale_remains(double /*ratio*/) override {}

    std::string name() const override { return "Stride (Uncompensated)"; }

private:
    std::vector<HomoStrideClient> clients_;
    long long global_tickets_ = 0;
    long long global_pass_    = 0;
    std::mt19937 rng_;

    void global_pass_update_time(int n_cores);
    void global_tickets_update(long long delta);
    void client_join(HomoStrideClient& c);
    void client_leave(HomoStrideClient& c);
    HomoStrideClient* find(int id);
};
