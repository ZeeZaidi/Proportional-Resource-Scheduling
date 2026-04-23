#pragma once

#include "common.h"
#include <random>
#include <vector>
#include <unordered_map>

// -----------------------------------------------------------------------
// Tree-based O(log n) lottery scheduler.
//
// Implements the partial-ticket-sum tree described in Section 4.2 of
// Waldspurger & Weihl (OSDI 1994): "a more efficient implementation is
// to use a tree of partial ticket sums, with clients at the leaves."
//
// Data structure: a 1-indexed binary segment tree stored in a flat array.
//   - Leaves hold individual client ticket counts.
//   - Internal nodes hold the sum of tickets in their subtree.
//   - The root holds total_tickets.
//
// allocate():
//   Draw random in [0, root.sum). Traverse root → leaf: at each internal
//   node, go left if draw < left.sum, else subtract left.sum and go right.
//   O(log n).
//
// add/remove/modify: update leaf value and propagate sums upward. O(log n).
//
// Capacity is fixed at construction time (next power of two ≥ max_clients).
// Clients are mapped to leaf slots via a hash map.
// -----------------------------------------------------------------------

class LotteryTreeScheduler : public Scheduler {
public:
    // max_clients: upper bound on number of simultaneous clients.
    explicit LotteryTreeScheduler(int max_clients = 256, unsigned seed = 42);

    void add_client(int id, int tickets) override;
    void remove_client(int id) override;
    void modify_client(int id, int new_tickets) override;
    int  allocate() override;
    std::string name() const override { return "Lottery (Tree)"; }

    int total_tickets() const;
    int num_clients()   const { return num_clients_; }

private:
    int  capacity_;      // number of leaf slots (power of two)
    int  num_clients_;
    std::vector<int>  tree_;   // size = 2 * capacity_; 1-indexed
    std::mt19937      rng_;

    // Maps client id → leaf index in [capacity_, 2*capacity_).
    std::unordered_map<int, int> id_to_leaf_;
    // Maps leaf index → client id (-1 = empty).
    std::vector<int> leaf_to_id_;

    // Next free leaf slot (linear probe through leaf_to_id_).
    int next_free_leaf() const;

    // Update leaf at position pos with new value; propagate sums upward.
    void update(int pos, int value);

    // Walk tree from root following the draw value; return leaf index.
    int find_winner(int draw) const;
};
