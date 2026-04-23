#pragma once

#include "common.h"
#include <vector>
#include <unordered_map>

// -----------------------------------------------------------------------
// Hierarchical stride scheduler.
//
// Implements Section 4 of Waldspurger & Weihl (MIT-LCS-TM-528, 1995),
// Figures 6 and 7.
//
// Data structure: a complete binary tree stored in a flat array.
//   - Leaves correspond to individual clients.
//   - Internal nodes summarize their subtree (aggregate tickets, stride, pass).
//   - The root acts as the "global pass" reference.
//
// allocate() (Figure 6):
//   Traverse root → leaf, at each internal node choose the child with
//   the smaller pass value. After use, update pass for all ancestors
//   following the leaf-to-root path.
//
// node_modify() (Figure 7):
//   Adjust a leaf's tickets/stride; scale remain; propagate the delta up
//   the tree to update every ancestor's tickets/stride/pass.
//
// Absolute error bound: O(log n_c) vs O(n_c) for basic stride.
// (Paper, Section 5.4: hierarchical σ is more than 2× smaller than basic.)
// -----------------------------------------------------------------------

struct HierNode {
    int       tickets = 0;
    long long stride  = 0;
    long long pass    = 0;
    int       parent  = -1;
    int       left    = -1;
    int       right   = -1;
    int       client_id = -1;  // >=0 only for leaf nodes
};

class HierarchicalStrideScheduler : public Scheduler {
public:
    // max_clients: upper bound on simultaneous clients.
    explicit HierarchicalStrideScheduler(int max_clients = 256);

    void add_client(int id, int tickets) override;
    void remove_client(int id) override;
    void modify_client(int id, int new_tickets) override;
    int  allocate() override;
    std::string name() const override { return "Stride (Hierarchical)"; }

    int num_clients() const { return num_clients_; }

private:
    std::vector<HierNode> nodes_;   // flat array of tree nodes
    int root_      = -1;
    int num_clients_ = 0;

    // Client id → leaf node index in nodes_.
    std::unordered_map<int, int> id_to_node_;

    // ---- Tree construction helpers ----

    // Build a balanced binary tree for 'n' leaves.
    // Returns the index of the root.
    void rebuild();

    // ---- Allocation helpers ----

    // Is node i a leaf?
    bool is_leaf(int i) const {
        return nodes_[i].left == -1 && nodes_[i].right == -1;
    }

    // Update pass for all ancestors of node i after use (Figure 6).
    // elapsed = 1 quantum in the discrete simulator.
    void update_ancestors(int i, long long elapsed = 1);

    // ---- Modification helpers ----

    // Propagate a ticket delta from node i up to (but not including) the root.
    // Adjusts tickets, stride, and pass at every ancestor (Figure 7).
    void propagate_modify(int i, int delta, int old_stride_leaf);
};
