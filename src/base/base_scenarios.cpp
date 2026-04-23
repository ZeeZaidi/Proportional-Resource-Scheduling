#include "base_scenarios.h"
#include "common.h"
#include "lottery_scheduler_list.h"
#include "lottery_scheduler_tree.h"
#include "stride_scheduler.h"
#include "stride_scheduler_hierarchical.h"
#include "event_simulator.h"
#include "metrics.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <numeric>
#include <cmath>
#include <random>
#include <functional>

// -----------------------------------------------------------------------
// Helper: run one scenario on a list of scheduler factories and print
//         the comparison report.
// -----------------------------------------------------------------------

using SchedulerFactory = std::function<std::unique_ptr<Scheduler>()>;

static void run_scenario(
    const std::string&                   title,
    const std::vector<SimClient>&        clients,
    int                                  num_quanta,
    const std::vector<DynamicEvent>&     events,
    const std::vector<SchedulerFactory>& factories,
    int                                  window_size = 50)
{
    std::string sep(80, '=');
    std::cout << "\n" << sep << "\n";
    std::cout << "  SCENARIO: " << title << "\n";
    std::cout << "  Clients : ";
    for (const auto& c : clients)
        std::cout << c.name << "(" << c.tickets << ") ";
    std::cout << "\n  Quanta  : " << num_quanta << "\n";
    std::cout << sep << "\n";

    std::vector<SimMetrics> all_metrics;
    EventSimulator sim;

    for (const auto& factory : factories) {
        auto sched = factory();
        SimResult result = sim.run(*sched, clients, num_quanta, events);
        all_metrics.push_back(compute_metrics(result, clients, window_size));
    }

    print_comparison(all_metrics);
}

// -----------------------------------------------------------------------
// Scenario 1: Static 2-client 7:3, 1000 quanta
// Replicates Figure 9(a,b) from stride paper.
// -----------------------------------------------------------------------
static void scenario_1() {
    std::vector<SimClient> clients = {
        {1, "A", 7},
        {2, "B", 3}
    };
    run_scenario(
        "Static 2-client 7:3 ratio — 1000 quanta (cf. Stride Fig.9a,b)",
        clients, 1000, {},
        {
            []{ return std::make_unique<LotteryListScheduler>(42); },
            []{ return std::make_unique<LotteryTreeScheduler>(16, 42); },
            []{ return std::make_unique<StrideScheduler>(); },
            []{ return std::make_unique<HierarchicalStrideScheduler>(); }
        });
}

// -----------------------------------------------------------------------
// Scenario 2: Static 2-client 19:1, 1000 quanta
// Replicates Figure 9(c,d) from stride paper.
// -----------------------------------------------------------------------
static void scenario_2() {
    std::vector<SimClient> clients = {
        {1, "A", 19},
        {2, "B", 1}
    };
    run_scenario(
        "Static 2-client 19:1 ratio — 1000 quanta (cf. Stride Fig.9c,d)",
        clients, 1000, {},
        {
            []{ return std::make_unique<LotteryListScheduler>(42); },
            []{ return std::make_unique<LotteryTreeScheduler>(16, 42); },
            []{ return std::make_unique<StrideScheduler>(); },
            []{ return std::make_unique<HierarchicalStrideScheduler>(); }
        });
}

// -----------------------------------------------------------------------
// Scenario 3: Static 3-client 3:2:1, 100 quanta
// Replicates Figure 8 from stride paper.
// -----------------------------------------------------------------------
static void scenario_3() {
    std::vector<SimClient> clients = {
        {1, "A", 3},
        {2, "B", 2},
        {3, "C", 1}
    };
    run_scenario(
        "Static 3-client 3:2:1 ratio — 100 quanta (cf. Stride Fig.8)",
        clients, 100, {},
        {
            []{ return std::make_unique<LotteryListScheduler>(42); },
            []{ return std::make_unique<StrideScheduler>(); },
            []{ return std::make_unique<HierarchicalStrideScheduler>(); }
        },
        10 /* window_size */);
}

// -----------------------------------------------------------------------
// Scenario 4: Dynamic ticket modification — [2,12]:3 ratio
// Replicates Figure 10 from stride paper: tickets updated every 2 quanta.
// -----------------------------------------------------------------------
static void scenario_4() {
    std::vector<SimClient> clients = {
        {1, "A", 2},
        {2, "B", 3}
    };

    std::mt19937 rng(99);
    std::uniform_int_distribution<int> ticket_dist(2, 12);
    std::vector<DynamicEvent> events;
    for (int q = 2; q <= 1000; q += 2) {
        DynamicEvent ev;
        ev.at_quantum  = q;
        ev.type        = DynamicEvent::MODIFY;
        ev.client_id   = 1;
        ev.new_tickets = ticket_dist(rng);
        events.push_back(ev);
    }

    run_scenario(
        "Dynamic ticket modification — A in [2,12], B fixed at 3, 1000 quanta (cf. Stride Fig.10)",
        clients, 1000, events,
        {
            []{ return std::make_unique<LotteryListScheduler>(42); },
            []{ return std::make_unique<StrideScheduler>(); }
        });
}

// -----------------------------------------------------------------------
// Scenario 5: Skewed 8-client 7:1:1:1:1:1:1:1
// Replicates Figure 13: basic stride vs hierarchical stride.
// -----------------------------------------------------------------------
static void scenario_5() {
    std::vector<SimClient> clients = {
        {1, "H",  7},
        {2, "L1", 1},
        {3, "L2", 1},
        {4, "L3", 1},
        {5, "L4", 1},
        {6, "L5", 1},
        {7, "L6", 1},
        {8, "L7", 1}
    };
    run_scenario(
        "Skewed 8-client 7:1:1:1:1:1:1:1 — 10000 quanta (cf. Stride Fig.13)",
        clients, 10000, {},
        {
            []{ return std::make_unique<StrideScheduler>(); },
            []{ return std::make_unique<HierarchicalStrideScheduler>(); }
        },
        500 /* window_size */);
}

// -----------------------------------------------------------------------
// Scenario 6: Dynamic client participation — join and leave mid-simulation
// Tests stride's remain mechanism and lottery's stateless handling.
// -----------------------------------------------------------------------
static void scenario_6() {
    std::vector<SimClient> clients = {
        {1, "A", 6},
        {2, "B", 4}
    };
    std::vector<DynamicEvent> events = {
        {200, DynamicEvent::JOIN,  3, 5, "C"},
        {400, DynamicEvent::LEAVE, 3, 0, "C"},
        {300, DynamicEvent::JOIN,  4, 2, "D"}
    };
    run_scenario(
        "Dynamic join/leave — A(6) B(4) always; C(5) joins q200 leaves q400; D(2) joins q300 — 500 quanta",
        clients, 500, events,
        {
            []{ return std::make_unique<LotteryListScheduler>(42); },
            []{ return std::make_unique<StrideScheduler>(); }
        });
}

// -----------------------------------------------------------------------
// Scenario 7: Lottery list vs tree equivalence and timing
// Verifies statistical equivalence and measures wall-clock time.
// -----------------------------------------------------------------------
static void scenario_7() {
    std::vector<SimClient> clients = {
        {1, "A", 5},
        {2, "B", 3},
        {3, "C", 2}
    };
    int N = 10000;

    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  SCENARIO: Lottery List vs Tree — equivalence and timing (" << N << " quanta)\n";
    std::cout << std::string(80, '=') << "\n";

    auto t0 = std::chrono::high_resolution_clock::now();
    LotteryListScheduler list_sched(42);
    EventSimulator sim;
    SimResult list_result = sim.run(list_sched, clients, N);
    auto t1 = std::chrono::high_resolution_clock::now();
    double list_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    auto t2 = std::chrono::high_resolution_clock::now();
    LotteryTreeScheduler tree_sched(16, 42);
    SimResult tree_result = sim.run(tree_sched, clients, N);
    auto t3 = std::chrono::high_resolution_clock::now();
    double tree_ms = std::chrono::duration<double, std::milli>(t3 - t2).count();

    SimMetrics list_m = compute_metrics(list_result, clients, 100);
    SimMetrics tree_m = compute_metrics(tree_result, clients, 100);

    print_comparison({list_m, tree_m});

    std::cout << "  Wall-clock time:\n";
    std::cout << "    Lottery (List): " << std::fixed << std::setprecision(3)
              << list_ms << " ms\n";
    std::cout << "    Lottery (Tree): " << std::fixed << std::setprecision(3)
              << tree_ms << " ms\n\n";

    bool all_close = true;
    for (size_t i = 0; i < list_m.clients.size(); ++i) {
        double diff = std::abs(list_m.clients[i].throughput_ratio -
                               tree_m.clients[i].throughput_ratio);
        if (diff > 0.10) { all_close = false; break; }
    }
    std::cout << "  Statistical equivalence (wins within 10%): "
              << (all_close ? "PASS" : "FAIL") << "\n";
}

// -----------------------------------------------------------------------
// Scenario 8: Thread thrashing — high-frequency client churn
// -----------------------------------------------------------------------
static void scenario_8() {
    std::vector<SimClient> clients = {
        {1, "A", 4},
        {2, "B", 4}
    };

    std::vector<DynamicEvent> events;
    const int cycle = 20, active_for = 10, total_q = 600;
    struct ChurnSpec { int id; std::string name; int phase; };
    for (auto& ch : std::vector<ChurnSpec>{{10,"C1",0},{11,"C2",6},{12,"C3",13}}) {
        for (int k = 0; ch.phase + k * cycle + 1 <= total_q; ++k) {
            int jq = ch.phase + k * cycle + 1;
            int lq = jq + active_for;
            events.push_back({jq,  DynamicEvent::JOIN,  ch.id, 2, ch.name});
            if (lq <= total_q)
                events.push_back({lq, DynamicEvent::LEAVE, ch.id, 0, ch.name});
        }
    }

    run_scenario(
        "Thread thrashing — A(4) B(4) stable; C1/C2/C3(2 each) cycle every 20q — 600 quanta",
        clients, total_q, events,
        {
            []{ return std::make_unique<LotteryListScheduler>(42); },
            []{ return std::make_unique<StrideScheduler>(); }
        },
        20 /* window_size */);
}

// -----------------------------------------------------------------------
// Scenario 9: Multiprocess fairness / ticket inflation
// -----------------------------------------------------------------------
static void scenario_9() {
    std::vector<SimClient> clients = {
        {1, "T1a", 2}, {2, "T1b", 2}, {3, "T1c", 2}, {4, "T2", 6}
    };
    std::vector<DynamicEvent> events = {
        {501, DynamicEvent::JOIN, 5, 2, "T1d"}
    };
    const int N = 1000;

    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  SCENARIO: Multiprocess fairness / ticket inflation — " << N << " quanta\n";
    std::cout << "  P1 = T1a(2)+T1b(2)+T1c(2) [+T1d(2) at q501]   P2 = T2(6)\n";
    std::cout << std::string(80, '=') << "\n";

    EventSimulator sim;
    std::vector<std::pair<std::string, SimResult>> results;

    for (auto factory : {
        std::function<std::unique_ptr<Scheduler>()>([]{ return std::make_unique<LotteryListScheduler>(42); }),
        std::function<std::unique_ptr<Scheduler>()>([]{ return std::make_unique<StrideScheduler>(); })
    }) {
        auto sched = factory();
        std::string sname = sched->name();
        SimResult r = sim.run(*sched, clients, N, events);
        results.push_back({sname, r});
        print_report(compute_metrics(r, clients, 50));
    }

    std::cout << "\n  --- Aggregate process-level wins ---\n\n";
    std::cout << std::left
              << std::setw(28) << "Scheduler"
              << std::setw(14) << "Phase"
              << std::setw(12) << "P1 wins"
              << std::setw(12) << "P2 wins"
              << std::setw(12) << "P1 share"
              << std::setw(12) << "P2 share" << "\n";
    std::cout << std::string(90, '-') << "\n";

    auto pct = [](int w, int t) {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << (t ? 100.0*w/t : 0.0) << "%";
        return ss.str();
    };

    for (size_t ri = 0; ri < results.size(); ++ri) {
        const std::string& sname = results[ri].first;
        const SimResult&   r     = results[ri].second;

        auto wins = [&r](int id, int qs, int qe) {
            int n = 0;
            for (const auto& rec : r.history)
                if (rec.winner_id == id && rec.quantum >= qs && rec.quantum <= qe) ++n;
            return n;
        };

        int p1a = wins(1,1,500)+wins(2,1,500)+wins(3,1,500), p2a = wins(4,1,500);
        int p1b = wins(1,501,N)+wins(2,501,N)+wins(3,501,N)+wins(5,501,N), p2b = wins(4,501,N);

        std::cout << std::left << std::setw(28) << sname
                  << std::setw(14) << "A (q1-500)"
                  << std::setw(12) << p1a << std::setw(12) << p2a
                  << std::setw(12) << pct(p1a,p1a+p2a) << std::setw(12) << pct(p2a,p1a+p2a) << "\n";
        std::cout << std::setw(28) << "" << std::setw(14) << "B (q501-1000)"
                  << std::setw(12) << p1b << std::setw(12) << p2b
                  << std::setw(12) << pct(p1b,p1b+p2b) << std::setw(12) << pct(p2b,p1b+p2b) << "\n\n";
    }
    std::cout << "  Expected Phase A: P1 ~50.0%  P2 ~50.0%\n";
    std::cout << "  Expected Phase B: P1 ~57.1%  P2 ~42.9%  (ticket inflation: 8 vs 6)\n\n";
}

// -----------------------------------------------------------------------
// Scenario 10: Lottery convergence rate — 1/√n error decay
// -----------------------------------------------------------------------
static void scenario_10() {
    const std::vector<int> ns = {100, 316, 1000, 3162, 10000};
    const int R = 200;
    const double p_A = 0.7, p_B = 0.3;

    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  SCENARIO: Lottery convergence rate — 1/sqrt(n) error decay\n";
    std::cout << "  Clients: A(7):B(3), " << R << " independent trials per n\n";
    std::cout << std::string(80, '=') << "\n\n";

    std::cout << std::left
              << std::setw(8)  << "n"
              << std::setw(14) << "E[AbsErr_A]"
              << std::setw(14) << "Theory_sA"
              << std::setw(18) << "E[AbsErr]/sqrt(n)"
              << std::setw(14) << "E[TpErr_A]"
              << std::setw(14) << "Theory_1/srt"
              << "\n";
    std::cout << std::string(82, '-') << "\n";

    EventSimulator sim;
    for (int n : ns) {
        std::vector<SimClient> clients = {{1, "A", 7}, {2, "B", 3}};
        double sum_abs = 0.0, sum_tp = 0.0;

        for (int r = 0; r < R; ++r) {
            LotteryListScheduler sched(1000 + r);
            SimResult result = sim.run(sched, clients, n);
            SimMetrics m = compute_metrics(result, clients, n);
            for (const auto& cm : m.clients) {
                if (cm.id == 1) {
                    sum_abs += cm.absolute_error;
                    sum_tp  += cm.throughput_error;
                }
            }
        }

        double mean_abs     = sum_abs / R;
        double mean_tp      = sum_tp  / R;
        double sqrtn        = std::sqrt(static_cast<double>(n));
        double theory_sigma = sqrtn * std::sqrt(p_A * p_B);
        double theory_tp    = std::sqrt(p_A * p_B) / sqrtn;

        std::cout << std::left << std::fixed << std::setprecision(3)
                  << std::setw(8)  << n
                  << std::setw(14) << mean_abs
                  << std::setw(14) << theory_sigma
                  << std::setw(18) << (mean_abs / sqrtn)
                  << std::setw(14) << mean_tp
                  << std::setw(14) << theory_tp
                  << "\n";
    }

    const double pi = std::acos(-1.0);
    std::cout << "\n  Expected E[AbsErr]/sqrt(n) ≈ sqrt(p*(1-p)) * sqrt(2/pi) = "
              << std::fixed << std::setprecision(3)
              << (std::sqrt(p_A * p_B) * std::sqrt(2.0 / pi))
              << "  (constant across all n)\n";
    std::cout << "  Confirmed if column 4 (E[AbsErr]/sqrt(n)) is approximately constant.\n\n";
}

// -----------------------------------------------------------------------
// Public entry point
// -----------------------------------------------------------------------

void run_base_scenarios() {
    std::cout << "\n";
    std::cout << "############################################################\n";
    std::cout << "#  Stride vs Lottery Scheduling — Event Simulator          #\n";
    std::cout << "#  Based on Waldspurger & Weihl (1994, 1995)               #\n";
    std::cout << "############################################################\n";

    scenario_1();
    scenario_2();
    scenario_3();
    scenario_4();
    scenario_5();
    scenario_6();
    scenario_7();
    scenario_8();
    scenario_9();
    scenario_10();

    std::cout << "\nBase scenarios complete.\n";
}
