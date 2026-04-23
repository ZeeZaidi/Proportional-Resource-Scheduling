#pragma once

#include "hetero_scheduler.h"
#include <vector>
#include <random>

// -----------------------------------------------------------------------
// Heterogeneous stride scheduler (work-unit compensated).
//
// Adapted from stride_scheduler.h/.cpp (Waldspurger 1995, Figures 1,3,4,5).
// Key change: pass advances by stride × S_C (core performance scale) instead
// of stride alone. global_pass tracks work units, not time units.
//
// Formal error bound: work-unit error ≤ S_max × 1 quantum (original bound).
// For S_max=2.0, error ≤ 2 work units per client per round.
//
// The remain invariant (pass - global_pass = credit) is maintained in
// work-unit space. rescale_remains() adjusts inactive clients' remain
// values when SCALE_CHANGE events modify the global work rate.
// -----------------------------------------------------------------------

struct HeteroStrideClient {
    int       id;
    int       tickets;
    long long stride;
    long long pass;
    long long remain;
    bool      active;
};

class HeteroStrideScheduler : public HeteroScheduler {
public:
    explicit HeteroStrideScheduler(unsigned seed = 42);

    void add_client(int id, int tickets) override;
    void remove_client(int id) override;
    void modify_client(int id, int new_tickets) override;

    std::vector<std::pair<int,int>>
        allocate_multi(const std::vector<Core>& cores) override;

    void rescale_remains(double ratio) override;

    std::string name() const override { return "Stride (Compensated)"; }

private:
    std::vector<HeteroStrideClient> clients_;
    long long global_tickets_ = 0;
    long long global_pass_    = 0;
    std::mt19937 rng_;

    static constexpr long long S_DENOM = 1000;

    void global_pass_update(long long total_work_int);
    void global_tickets_update(long long delta);
    void client_join(HeteroStrideClient& c);
    void client_leave(HeteroStrideClient& c);
    HeteroStrideClient* find(int id);
};
