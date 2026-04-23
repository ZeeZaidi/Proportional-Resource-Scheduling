#include "lottery_scheduler_tree.h"
#include <stdexcept>
#include <algorithm>
#include <cassert>

// Round up to the next power of two >= n.
static int next_pow2(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

LotteryTreeScheduler::LotteryTreeScheduler(int max_clients, unsigned seed)
    : capacity_(next_pow2(std::max(max_clients, 1))),
      num_clients_(0),
      tree_(2 * capacity_, 0),
      rng_(seed),
      leaf_to_id_(capacity_, -1) {}

void LotteryTreeScheduler::update(int pos, int value) {
    // pos is 1-indexed position in tree_ (leaf positions are [capacity_, 2*capacity_)).
    tree_[pos] = value;
    for (int p = pos >> 1; p >= 1; p >>= 1)
        tree_[p] = tree_[2 * p] + tree_[2 * p + 1];
}

int LotteryTreeScheduler::next_free_leaf() const {
    for (int i = 0; i < capacity_; ++i)
        if (leaf_to_id_[i] < 0)
            return capacity_ + i;  // 1-indexed tree position
    return -1;
}

int LotteryTreeScheduler::find_winner(int draw) const {
    // Traverse from root (index 1) downward.
    int node = 1;
    while (node < capacity_) {
        int left = 2 * node;
        if (draw < tree_[left]) {
            node = left;
        } else {
            draw -= tree_[left];
            node = left + 1;
        }
    }
    return node;  // leaf position (1-indexed)
}

int LotteryTreeScheduler::total_tickets() const {
    return tree_[1];  // root holds total
}

void LotteryTreeScheduler::add_client(int id, int tickets) {
    if (tickets <= 0)
        throw std::invalid_argument("Tickets must be positive");
    if (id_to_leaf_.count(id))
        throw std::invalid_argument("Client id already exists");
    if (num_clients_ >= capacity_)
        throw std::runtime_error("Scheduler capacity exceeded");

    int pos = next_free_leaf();
    int leaf_idx = pos - capacity_;  // 0-indexed slot
    id_to_leaf_[id]       = pos;
    leaf_to_id_[leaf_idx] = id;
    ++num_clients_;
    update(pos, tickets);
}

void LotteryTreeScheduler::remove_client(int id) {
    auto it = id_to_leaf_.find(id);
    if (it == id_to_leaf_.end())
        throw std::invalid_argument("Client id not found");
    int pos = it->second;
    int leaf_idx = pos - capacity_;
    leaf_to_id_[leaf_idx] = -1;
    id_to_leaf_.erase(it);
    --num_clients_;
    update(pos, 0);
}

void LotteryTreeScheduler::modify_client(int id, int new_tickets) {
    if (new_tickets <= 0)
        throw std::invalid_argument("Tickets must be positive");
    auto it = id_to_leaf_.find(id);
    if (it == id_to_leaf_.end())
        throw std::invalid_argument("Client id not found");
    update(it->second, new_tickets);
}

int LotteryTreeScheduler::allocate() {
    if (num_clients_ == 0)
        throw std::runtime_error("No clients to schedule");

    int total = tree_[1];
    std::uniform_int_distribution<int> dist(0, total - 1);
    int draw = dist(rng_);

    int leaf_pos = find_winner(draw);
    int leaf_idx = leaf_pos - capacity_;
    return leaf_to_id_[leaf_idx];
}
