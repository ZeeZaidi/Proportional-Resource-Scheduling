#include "hetero_lottery.h"
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include <cassert>

// Round up to next power of two >= n.
static int next_pow2(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

HeteroLotteryScheduler::HeteroLotteryScheduler(int max_clients, unsigned seed)
    : capacity_(next_pow2(std::max(max_clients, 1))),
      num_clients_(0),
      tree_(2 * capacity_, 0),
      rng_(seed),
      leaf_to_id_(capacity_, -1) {}

// -----------------------------------------------------------------------
// Tree operations — identical to lottery_scheduler_tree.cpp
// -----------------------------------------------------------------------

void HeteroLotteryScheduler::update(int pos, int value) {
    tree_[pos] = value;
    for (int p = pos >> 1; p >= 1; p >>= 1)
        tree_[p] = tree_[2 * p] + tree_[2 * p + 1];
}

int HeteroLotteryScheduler::next_free_leaf() const {
    for (int i = 0; i < capacity_; ++i)
        if (leaf_to_id_[i] < 0)
            return capacity_ + i;
    return -1;
}

int HeteroLotteryScheduler::find_winner(int draw) const {
    int node = 1;
    while (node < capacity_) {
        int left = 2 * node;
        if (draw < tree_[left])
            node = left;
        else {
            draw -= tree_[left];
            node = left + 1;
        }
    }
    return node;
}

int HeteroLotteryScheduler::total_tickets() const {
    return tree_[1];
}

// -----------------------------------------------------------------------
// Client management — identical to lottery_scheduler_tree.cpp
// -----------------------------------------------------------------------

void HeteroLotteryScheduler::add_client(int id, int tickets) {
    if (tickets <= 0)
        throw std::invalid_argument("Tickets must be positive");
    if (id_to_leaf_.count(id))
        throw std::invalid_argument("Client id already exists");
    if (num_clients_ >= capacity_)
        throw std::runtime_error("Scheduler capacity exceeded");

    int pos = next_free_leaf();
    int leaf_idx = pos - capacity_;
    id_to_leaf_[id]       = pos;
    leaf_to_id_[leaf_idx] = id;
    ++num_clients_;
    update(pos, tickets);
}

void HeteroLotteryScheduler::remove_client(int id) {
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

void HeteroLotteryScheduler::modify_client(int id, int new_tickets) {
    if (new_tickets <= 0)
        throw std::invalid_argument("Tickets must be positive");
    auto it = id_to_leaf_.find(id);
    if (it == id_to_leaf_.end())
        throw std::invalid_argument("Client id not found");
    update(it->second, new_tickets);
}

// -----------------------------------------------------------------------
// allocate_multi — N independent draws with replacement
// -----------------------------------------------------------------------

std::vector<std::pair<int,int>>
HeteroLotteryScheduler::allocate_multi(const std::vector<Core>& cores) {
    int n = static_cast<int>(cores.size());

    if (num_clients_ == 0)
        throw std::runtime_error("No clients to schedule");

    int total = tree_[1];
    if (total <= 0)
        throw std::runtime_error("No tickets in pool");

    // Shuffle core order so P-core vs E-core assignment is random.
    std::vector<int> core_order(n);
    std::iota(core_order.begin(), core_order.end(), 0);
    std::shuffle(core_order.begin(), core_order.end(), rng_);

    // N independent draws — the same client can win multiple cores,
    // with probability proportional to its ticket share per draw.
    std::uniform_int_distribution<int> dist(0, total - 1);
    std::vector<std::pair<int,int>> result;
    result.reserve(n);

    for (int i = 0; i < n; ++i) {
        int draw = dist(rng_);
        int leaf = find_winner(draw);
        int leaf_idx = leaf - capacity_;
        int id = leaf_to_id_[leaf_idx];
        result.push_back({id, cores[core_order[i]].id});
    }

    return result;
}
