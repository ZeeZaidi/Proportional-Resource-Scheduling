#pragma once

#include "hetero_scheduler.h"
#include <random>
#include <vector>
#include <unordered_map>

// -----------------------------------------------------------------------
// Tree-based O(log n) lottery scheduler for heterogeneous multicore.
//
// Adapted from lottery_scheduler_tree.h/.cpp (Waldspurger 1994, §4.2).
// allocate_multi() draws N winners without replacement using Fisher-Yates
// on the segment tree: draw → zero leaf → repeat → restore all leaves.
//
// Without-replacement draws are hypergeometric: same expected wins as
// independent binomial draws (E[w_i] = N × t_i/T), but lower variance.
// This is a property to highlight, not a flaw.
//
// Lottery is stateless — rescale_remains() is a no-op (default from base).
// Work tracking is handled entirely by the simulator/metrics layer.
//
// Requires T (total tickets) ≥ N (number of cores) to guarantee N distinct
// winners per quantum. An assertion enforces this in allocate_multi().
// -----------------------------------------------------------------------

class HeteroLotteryScheduler : public HeteroScheduler {
public:
    explicit HeteroLotteryScheduler(int max_clients = 256, unsigned seed = 42);

    void add_client(int id, int tickets) override;
    void remove_client(int id) override;
    void modify_client(int id, int new_tickets) override;

    std::vector<std::pair<int,int>>
        allocate_multi(const std::vector<Core>& cores) override;

    std::string name() const override { return "Lottery (Tree)"; }

    int total_tickets() const;
    int num_clients()   const { return num_clients_; }

private:
    int  capacity_;       // leaf count (power of two)
    int  num_clients_;
    std::vector<int>  tree_;    // size = 2 * capacity_; 1-indexed
    std::mt19937      rng_;

    std::unordered_map<int, int> id_to_leaf_;   // client id → leaf position
    std::vector<int> leaf_to_id_;               // leaf slot → client id (-1 = empty)

    int  next_free_leaf() const;
    void update(int pos, int value);
    int  find_winner(int draw) const;
};
