#include "stride_scheduler_hierarchical.h"
#include <stdexcept>
#include <algorithm>
#include <functional>
#include <limits>
#include <cassert>

// -----------------------------------------------------------------------
// Construction helpers
// -----------------------------------------------------------------------

HierarchicalStrideScheduler::HierarchicalStrideScheduler(int max_clients)
{
    // Pre-allocate enough nodes for a complete binary tree of max_clients
    // leaves: a complete binary tree with n leaves has at most 2n-1 nodes.
    nodes_.reserve(2 * max_clients);
}

// -----------------------------------------------------------------------
// Internal: rebuild the tree whenever clients are added/removed.
//
// Strategy: collect all active (client_id >= 0) nodes' state, clear the
// tree, then rebuild a balanced binary tree from scratch using the saved
// state. This keeps the tree balanced and the indexing simple.
//
// For the simulator's client counts (≤ a few dozen to a few hundred)
// this O(n) rebuild on each add/remove is acceptable.
// -----------------------------------------------------------------------

void HierarchicalStrideScheduler::rebuild() {
    // --- 1. Collect current leaf state ---
    struct LeafState { int client_id; int tickets; long long pass; };
    std::vector<LeafState> leaves;
    leaves.reserve(num_clients_);
    for (const auto& kv : id_to_node_) {
        const HierNode& n = nodes_[kv.second];
        leaves.push_back({kv.first, n.tickets, n.pass});
    }

    // Sort by client_id for deterministic layout.
    std::sort(leaves.begin(), leaves.end(),
              [](const LeafState& a, const LeafState& b){ return a.client_id < b.client_id; });

    // --- 2. Rebuild nodes_ ---
    nodes_.clear();
    id_to_node_.clear();
    root_ = -1;

    if (leaves.empty()) return;

    // Build a complete binary tree with leaves.size() leaves.
    // We use a simple recursive build stored in a flat vector.
    // Node 0 will be the root.
    //
    // build(lo, hi) creates the subtree for leaves[lo..hi-1]
    // and returns the index of its root node in nodes_.

    std::function<int(int, int, int)> build = [&](int lo, int hi, int parent) -> int {
        int idx = static_cast<int>(nodes_.size());
        nodes_.emplace_back();  // placeholder
        HierNode& node = nodes_.back();
        node.parent = parent;

        if (hi - lo == 1) {
            // Leaf node
            node.client_id = leaves[lo].client_id;
            node.tickets   = leaves[lo].tickets;
            node.stride    = (node.tickets > 0) ? STRIDE1 / node.tickets : 0;
            node.pass      = leaves[lo].pass;
            node.left      = -1;
            node.right     = -1;
            id_to_node_[leaves[lo].client_id] = idx;
        } else {
            // Internal node: children cover [lo, mid) and [mid, hi)
            int mid = (lo + hi) / 2;

            // Build children first (they may realloc nodes_).
            // Build left child
            int left_idx  = build(lo, mid, idx);
            // After recursion, nodes_ may have been reallocated; re-get node ref by index
            int right_idx = build(mid, hi, idx);

            // Re-fetch reference after potential reallocation
            HierNode& n   = nodes_[idx];
            n.left         = left_idx;
            n.right        = right_idx;
            n.client_id    = -1;

            // Aggregate tickets/stride/pass from children
            n.tickets = nodes_[left_idx].tickets + nodes_[right_idx].tickets;
            n.stride  = (n.tickets > 0) ? STRIDE1 / n.tickets : 0;
            // Internal node pass = min(children passes)
            n.pass    = std::min(nodes_[left_idx].pass, nodes_[right_idx].pass);
        }
        return idx;
    };

    root_ = build(0, static_cast<int>(leaves.size()), -1);
}

// -----------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------

void HierarchicalStrideScheduler::add_client(int id, int tickets) {
    if (tickets <= 0)
        throw std::invalid_argument("Tickets must be positive");
    if (id_to_node_.count(id))
        throw std::invalid_argument("Client id already exists");

    // Add leaf with initial pass = stride (so it starts in proper order).
    // We'll store pass temporarily; rebuild sets it from this leaf state.
    // Use pass = 0 for a new client so it is selected promptly initially.
    id_to_node_[id] = -1;  // placeholder
    // We need a temporary node to hold state before rebuild.
    HierNode leaf;
    leaf.client_id = id;
    leaf.tickets   = tickets;
    leaf.stride    = STRIDE1 / tickets;
    leaf.pass      = leaf.stride;  // start at stride (like remain=stride in basic)
    leaf.left = leaf.right = leaf.parent = -1;

    nodes_.push_back(leaf);
    id_to_node_[id] = static_cast<int>(nodes_.size()) - 1;
    ++num_clients_;

    rebuild();
}

void HierarchicalStrideScheduler::remove_client(int id) {
    if (!id_to_node_.count(id))
        throw std::invalid_argument("Client id not found");
    // Remove from id_to_node_ before rebuild; rebuild skips missing ids.
    id_to_node_.erase(id);
    --num_clients_;
    rebuild();
}

void HierarchicalStrideScheduler::modify_client(int id, int new_tickets) {
    if (new_tickets <= 0)
        throw std::invalid_argument("Tickets must be positive");
    auto it = id_to_node_.find(id);
    if (it == id_to_node_.end())
        throw std::invalid_argument("Client id not found");

    HierNode& leaf = nodes_[it->second];
    long long old_stride = leaf.stride;
    leaf.tickets = new_tickets;
    leaf.stride  = STRIDE1 / new_tickets;

    // Scale pass similarly to remain-scaling in Figure 5:
    // The remaining portion of the stride is adjusted by stride'/stride.
    // Here we approximate by scaling pass relative to the root pass.
    if (root_ >= 0 && old_stride > 0) {
        long long global_pass = nodes_[root_].pass;
        long long remain = leaf.pass - global_pass;
        remain = (remain * leaf.stride) / old_stride;
        leaf.pass = global_pass + remain;
    }

    // Rebuild to rebalance and fix all ancestor aggregates.
    rebuild();
}

int HierarchicalStrideScheduler::allocate() {
    if (root_ < 0 || num_clients_ == 0)
        throw std::runtime_error("No clients to schedule");

    // Traverse root → leaf following the child with minimum pass (Figure 6).
    int n = root_;
    while (!is_leaf(n)) {
        int L = nodes_[n].left;
        int R = nodes_[n].right;
        // Choose child with smaller pass; ties broken by left child.
        if (nodes_[L].pass <= nodes_[R].pass)
            n = L;
        else
            n = R;
    }

    // n is the winning leaf.
    int winner_id = nodes_[n].client_id;

    // Advance pass for the winner and all its ancestors (Figure 6).
    // pass += stride * elapsed / quantum; elapsed = quantum = 1.
    nodes_[n].pass += nodes_[n].stride;

    // Update ancestor passes (each internal node's pass = min of children).
    int cur = nodes_[n].parent;
    while (cur >= 0) {
        int L = nodes_[cur].left;
        int R = nodes_[cur].right;
        long long new_pass = std::min(
            L >= 0 ? nodes_[L].pass : std::numeric_limits<long long>::max(),
            R >= 0 ? nodes_[R].pass : std::numeric_limits<long long>::max());
        nodes_[cur].pass = new_pass;
        cur = nodes_[cur].parent;
    }

    return winner_id;
}
