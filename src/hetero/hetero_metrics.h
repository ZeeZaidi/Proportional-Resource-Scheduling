#pragma once

#include "hetero_simulator.h"
#include "common.h"
#include <string>
#include <vector>

// -----------------------------------------------------------------------
// Extended metrics for heterogeneous multicore scheduling.
//
// All original temporal metrics (wins, AbsErr, jitter, MeanRT) are kept
// for direct comparison. New work-unit metrics are added:
//
//   effective_work_received  = Σ S_C for each quantum won by client i
//   work_intended            = (t_i/T) × total_work_dispensed
//   computational_error      = |effective_work_received - work_intended|
//   work_throughput_ratio    = effective_work_received / total_work_dispensed
//
// The comparison between temporal AbsErr and computational_error is the
// central result of the heterogeneous extension.
// -----------------------------------------------------------------------

struct HeteroClientMetrics {
    int         id;
    std::string name;
    int         tickets;
    int         n_active;

    // Temporal metrics (quanta-based).
    int         wins;
    double      intended;            // n_active * n_cores * t_i/T
    double      absolute_error;
    double      throughput_ratio;
    double      throughput_error;
    double      mean_response_time;
    double      jitter;

    // Work-unit metrics.
    double      effective_work;      // Σ S_C per win
    double      work_intended;       // (t_i/T) × total_work_dispensed
    double      computational_error; // |effective_work - work_intended|
    double      work_throughput_ratio;
};

struct HeteroSimMetrics {
    std::string                       scheduler_name;
    int                               total_quanta;
    double                            total_work_dispensed;
    std::vector<HeteroClientMetrics>  clients;
    std::vector<double>               convergence_windows;
    int                               window_size;
};

// Compute all metrics from a HeteroSimResult.
HeteroSimMetrics compute_hetero_metrics(
    const HeteroSimResult&        result,
    const std::vector<SimClient>&  clients,
    int                            n_cores,
    int                            window_size = 50);

// Print a formatted report.
void print_hetero_report(const HeteroSimMetrics& m);

// Print a side-by-side comparison of multiple schedulers.
void print_hetero_comparison(const std::vector<HeteroSimMetrics>& all);
