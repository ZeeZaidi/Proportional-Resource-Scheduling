#pragma once

#include "common.h"
#include <random>
#include <vector>

// -----------------------------------------------------------------------
// List-based O(n) lottery scheduler.
//
// Faithfully implements the list-based lottery described in Section 4.2
// and Figure 1 of Waldspurger & Weihl (OSDI 1994).
//
// allocate():
//   1. Generate a random ticket number in [0, total_tickets).
//   2. Walk the client list, accumulating a running sum.
//   3. The client whose running sum first reaches/exceeds the draw is
//      the winner.
//
// Dynamic changes:
//   Lottery is stateless between rounds, so add/remove/modify only update
//   the ticket counts — no carry-over bookkeeping is needed.
// -----------------------------------------------------------------------

struct LotteryListClient {
    int id;
    int tickets;
};

class LotteryListScheduler : public Scheduler {
public:
    explicit LotteryListScheduler(unsigned seed = 42);

    void add_client(int id, int tickets) override;
    void remove_client(int id) override;
    void modify_client(int id, int new_tickets) override;
    int  allocate() override;
    std::string name() const override { return "Lottery (List)"; }

    int total_tickets() const { return total_tickets_; }
    int num_clients()   const { return static_cast<int>(clients_.size()); }

private:
    std::vector<LotteryListClient> clients_;
    int          total_tickets_ = 0;
    std::mt19937 rng_;

    // Returns iterator to the client with given id, or clients_.end().
    std::vector<LotteryListClient>::iterator find(int id);
};
