#include "hetero_scenarios.h"
#include "core.h"
#include "common.h"
#include "hetero_scheduler.h"
#include "hetero_stride.h"
#include "homo_stride.h"
#include "hetero_lottery.h"
#include "hetero_simulator.h"
#include "hetero_metrics.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

// -----------------------------------------------------------------------
// Helper: run all three schedulers on the same scenario and compare.
// -----------------------------------------------------------------------

static void run_scenario(
    const std::string&                      title,
    const std::vector<SimClient>&           clients,
    int                                     num_quanta,
    const std::vector<HeteroDynamicEvent>&  events,
    std::vector<Core>                       cores,
    int                                     window_size = 50)
{
    int n_cores = static_cast<int>(cores.size());

    std::cout << "\n";
    std::cout << std::string(120, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << "  Cores: " << n_cores << " (";
    int n_p = 0, n_e = 0;
    for (const auto& c : cores) {
        if (c.type == CoreType::PCORE) ++n_p;
        else ++n_e;
    }
    std::cout << n_p << " P-cores @ S=" << cores[0].performance_scale
              << ", " << n_e << " E-cores @ S=" << cores[n_p].performance_scale
              << ")  Quanta: " << num_quanta << "\n";
    std::cout << std::string(120, '=') << "\n";

    HeteroEventSimulator sim;

    // 1. Compensated stride.
    {
        auto my_cores = cores;
        HeteroStrideScheduler sched(42);
        auto result = sim.run(sched, clients, my_cores, num_quanta, events);
        auto metrics = compute_hetero_metrics(result, clients, n_cores, window_size);
        print_hetero_report(metrics);
    }

    // 2. Uncompensated stride (baseline).
    {
        auto my_cores = cores;
        HomoStrideScheduler sched(42);
        auto result = sim.run(sched, clients, my_cores, num_quanta, events);
        auto metrics = compute_hetero_metrics(result, clients, n_cores, window_size);
        print_hetero_report(metrics);
    }

    // 3. Lottery.
    {
        auto my_cores = cores;
        HeteroLotteryScheduler sched(256, 42);
        auto result = sim.run(sched, clients, my_cores, num_quanta, events);
        auto metrics = compute_hetero_metrics(result, clients, n_cores, window_size);
        print_hetero_report(metrics);
    }
}

// =======================================================================
// H1: Proportional baseline — A(7):B(3), 1000 quanta
// =======================================================================

static void scenario_H1() {
    std::vector<SimClient> clients = {
        {1, "A", 7},
        {2, "B", 3}
    };
    run_scenario("H1 — Proportional baseline: A(7):B(3), 1000 quanta",
                 clients, 1000, {}, make_default_cores());
}

// =======================================================================
// H2: Multi-client — tickets {5,4,3,2,1}, 5000 quanta
// =======================================================================

static void scenario_H2() {
    std::vector<SimClient> clients = {
        {1, "A", 5},
        {2, "B", 4},
        {3, "C", 3},
        {4, "D", 2},
        {5, "E", 1}
    };
    run_scenario("H2 — Multi-client: {5,4,3,2,1}, 5000 quanta",
                 clients, 5000, {}, make_default_cores());
}

// =======================================================================
// H3: Dynamic join/leave — A(6) B(4) permanent; C(5) q200-400; D(2) q300+
// =======================================================================

static void scenario_H3() {
    std::vector<SimClient> clients = {
        {1, "A", 6},
        {2, "B", 4}
    };
    std::vector<HeteroDynamicEvent> events = {
        {200, HeteroDynamicEvent::JOIN,  3, 5, 1.0, 0, "C"},
        {300, HeteroDynamicEvent::JOIN,  4, 2, 1.0, 0, "D"},
        {401, HeteroDynamicEvent::LEAVE, 3, 0, 1.0, 0, ""},
    };
    run_scenario("H3 — Dynamic join/leave: A(6) B(4) + C(5) q200-400, D(2) q300+",
                 clients, 500, events, make_default_cores());
}

// =======================================================================
// H4: P-core thermal throttle — A(6):B(4), P-cores throttle at q300
// =======================================================================

static void scenario_H4() {
    std::vector<SimClient> clients = {
        {1, "A", 6},
        {2, "B", 4}
    };
    std::vector<HeteroDynamicEvent> events = {
        {300, HeteroDynamicEvent::SCALE_CHANGE, 0, 0, 1.4, 0, ""},
        {300, HeteroDynamicEvent::SCALE_CHANGE, 0, 0, 1.4, 1, ""},
        {300, HeteroDynamicEvent::SCALE_CHANGE, 0, 0, 1.4, 2, ""},
        {300, HeteroDynamicEvent::SCALE_CHANGE, 0, 0, 1.4, 3, ""},
    };
    run_scenario("H4 — P-core throttle: A(6):B(4), P-cores S=2.0->1.4 at q300",
                 clients, 600, events, make_default_cores());
}

// =======================================================================
// Public entry point
// =======================================================================

void run_hetero_scenarios() {
    std::cout << "\n" << std::string(120, '*') << "\n";
    std::cout << "  HETEROGENEOUS MULTICORE SCHEDULING SIMULATOR\n";
    std::cout << "  Core-Aware Currency Scaling for Stride/Lottery Schedulers\n";
    std::cout << "  4 P-cores (S=2.0) + 4 E-cores (S=1.0) — Intel Alder Lake layout\n";
    std::cout << std::string(120, '*') << "\n";

    scenario_H1();
    scenario_H2();
    scenario_H3();
    scenario_H4();

    std::cout << "\n" << std::string(120, '*') << "\n";
    std::cout << "  Heterogeneous scenarios complete.\n";
    std::cout << std::string(120, '*') << "\n";
}
