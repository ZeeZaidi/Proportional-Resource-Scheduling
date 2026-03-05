#include "lottery_scheduler_list.h"
#include <stdexcept>
#include <algorithm>

LotteryListScheduler::LotteryListScheduler(unsigned seed)
    : rng_(seed) {}

void LotteryListScheduler::add_client(int id, int tickets) {
    if (tickets <= 0)
        throw std::invalid_argument("Tickets must be positive");
    if (find(id) != clients_.end())
        throw std::invalid_argument("Client id already exists");
    clients_.push_back({id, tickets});
    total_tickets_ += tickets;
}

void LotteryListScheduler::remove_client(int id) {
    auto it = find(id);
    if (it == clients_.end())
        throw std::invalid_argument("Client id not found");
    total_tickets_ -= it->tickets;
    clients_.erase(it);
}

void LotteryListScheduler::modify_client(int id, int new_tickets) {
    if (new_tickets <= 0)
        throw std::invalid_argument("Tickets must be positive");
    auto it = find(id);
    if (it == clients_.end())
        throw std::invalid_argument("Client id not found");
    total_tickets_ += (new_tickets - it->tickets);
    it->tickets = new_tickets;
}

int LotteryListScheduler::allocate() {
    if (clients_.empty())
        throw std::runtime_error("No clients to schedule");

    // Draw a winning ticket number in [0, total_tickets).
    std::uniform_int_distribution<int> dist(0, total_tickets_ - 1);
    int draw = dist(rng_);

    // Linear scan: accumulate running sum until we reach the draw.
    int running = 0;
    for (auto& c : clients_) {
        running += c.tickets;
        if (running > draw)
            return c.id;
    }

    // Should never reach here with a valid total_tickets_.
    return clients_.back().id;
}

std::vector<LotteryListClient>::iterator LotteryListScheduler::find(int id) {
    return std::find_if(clients_.begin(), clients_.end(),
                        [id](const LotteryListClient& c) { return c.id == id; });
}
