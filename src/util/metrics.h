#pragma once

#include "event_simulator.h"
#include <string>
#include <vector>

// -----------------------------------------------------------------------
// Metrics definitions (from the papers):
//
//   intended_i      = n_a * t_i / T          (proportional entitlement)
//   absolute_error  = |wins_i - intended_i|
//
//   pairwise_rel_error (for the subsystem of clients i, j):
//     T_ij = t_i + t_j;  n_ij = wins_i + wins_j
//     specified_i = n_ij * t_i / T_ij
//     rel_error_ij = |wins_i - specified_i|   (in quanta)
//   We report the maximum pairwise relative error over all pairs.
//
//   throughput_ratio = wins_i / n_a           (observed fraction)
//   throughput_error = |throughput_ratio - t_i/T|
//
//   mean_response_time = mean(inter_win_intervals)
//   jitter             = std_dev(inter_win_intervals)
//
//   convergence_windows: for window w of size W covering quanta
//     [(w-1)*W+1, w*W], compute the mean absolute error over clients
//     in that window.  Reported as a vector indexed by window.
// -----------------------------------------------------------------------

struct ClientMetrics {
    int         id;
    std::string name;
    int         tickets;
    int         n_active;           // quanta the client was present
    int         wins;
    double      intended;           // n_active * t_i / T
    double      absolute_error;     // |wins - intended|
    double      pairwise_rel_error; // max pairwise relative error vs any other client
    double      throughput_ratio;   // wins / n_active
    double      throughput_error;   // |throughput_ratio - t_i/T|
    double      mean_response_time; // mean inter-win interval (quanta)
    double      jitter;             // std dev of inter-win intervals
};

struct SimMetrics {
    std::string                scheduler_name;
    int                        total_quanta;
    std::vector<ClientMetrics> clients;
    std::vector<double>        convergence_windows; // mean abs error per window
    int                        window_size;
};

// Compute all metrics from a SimResult.
// total_tickets: sum of all client ticket allocations (used for proportions).
// window_size:   number of quanta per convergence window.
SimMetrics compute_metrics(const SimResult&              result,
                           const std::vector<SimClient>& clients,
                           int                           window_size = 50);

// Print a formatted per-scheduler report to stdout.
void print_report(const SimMetrics& m);

// Print a side-by-side comparison of multiple schedulers.
void print_comparison(const std::vector<SimMetrics>& all);
