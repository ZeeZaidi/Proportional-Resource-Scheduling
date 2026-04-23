#include "hetero_metrics.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <iostream>
#include <sstream>

// -----------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------

static double vec_mean(const std::vector<int>& v) {
    if (v.empty()) return 0.0;
    return static_cast<double>(std::accumulate(v.begin(), v.end(), 0LL))
           / static_cast<double>(v.size());
}

static double vec_stddev(const std::vector<int>& v) {
    if (v.size() < 2) return 0.0;
    double m = vec_mean(v);
    double sq_sum = 0.0;
    for (int x : v) {
        double d = x - m;
        sq_sum += d * d;
    }
    return std::sqrt(sq_sum / (v.size() - 1));  // sample std dev
}

static std::string dbl(double v, int prec = 3) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << v;
    return ss.str();
}

static void print_separator(int width = 120) {
    std::cout << std::string(width, '-') << "\n";
}

// -----------------------------------------------------------------------
// compute_hetero_metrics
// -----------------------------------------------------------------------

HeteroSimMetrics compute_hetero_metrics(
    const HeteroSimResult&        result,
    const std::vector<SimClient>&  clients,
    int                            n_cores,
    int                            window_size)
{
    HeteroSimMetrics m;
    m.scheduler_name      = result.scheduler_name;
    m.total_quanta        = result.total_quanta;
    m.total_work_dispensed = result.total_work_dispensed;
    m.window_size         = window_size;

    int total_tickets = 0;
    for (const auto& c : clients)
        total_tickets += c.tickets;

    // Per-client metrics.
    for (const auto& c : clients) {
        auto track_it = result.tracks.find(c.id);
        if (track_it == result.tracks.end()) continue;
        const auto& track = track_it->second;

        int n_active = track.last_active - track.first_active + 1;
        if (n_active <= 0) n_active = 1;

        double fraction = static_cast<double>(c.tickets) / total_tickets;

        // Temporal (quanta-based) metrics.
        // In multicore, each quantum dispatches n_cores allocations.
        double intended = n_active * n_cores * fraction;

        HeteroClientMetrics cm;
        cm.id                  = c.id;
        cm.name                = c.name;
        cm.tickets             = c.tickets;
        cm.n_active            = n_active;
        cm.wins                = track.wins;
        cm.intended            = intended;
        cm.absolute_error      = std::abs(track.wins - intended);
        cm.throughput_ratio     = static_cast<double>(track.wins) / (n_active * n_cores);
        cm.throughput_error     = std::abs(cm.throughput_ratio - fraction);
        cm.mean_response_time  = vec_mean(track.inter_win_intervals);
        cm.jitter              = vec_stddev(track.inter_win_intervals);

        // Work-unit metrics.
        cm.effective_work       = track.effective_work;
        cm.work_intended        = fraction * result.total_work_dispensed;
        cm.computational_error  = std::abs(cm.effective_work - cm.work_intended);
        cm.work_throughput_ratio = (result.total_work_dispensed > 0.0)
                                  ? cm.effective_work / result.total_work_dispensed
                                  : 0.0;

        m.clients.push_back(cm);
    }

    // Convergence windows — computed on work units per window.
    int num_windows = (result.total_quanta + window_size - 1) / window_size;
    m.convergence_windows.resize(num_windows, 0.0);

    for (int w = 0; w < num_windows; ++w) {
        int q_start = w * window_size + 1;
        int q_end   = std::min((w + 1) * window_size, result.total_quanta);

        // Sum work received per client in this window.
        std::unordered_map<int, double> window_work;
        double window_total_work = 0.0;
        for (const auto& rec : result.history) {
            if (rec.quantum >= q_start && rec.quantum <= q_end) {
                window_work[rec.winner_id] += rec.core_scale;
                window_total_work += rec.core_scale;
            }
        }

        // Mean computational error in this window.
        double sum_err = 0.0;
        int    n_clients = 0;
        for (const auto& c : clients) {
            double fraction = static_cast<double>(c.tickets) / total_tickets;
            double intended_w = fraction * window_total_work;
            double actual_w = 0.0;
            auto it = window_work.find(c.id);
            if (it != window_work.end()) actual_w = it->second;
            sum_err += std::abs(actual_w - intended_w);
            ++n_clients;
        }
        m.convergence_windows[w] = (n_clients > 0) ? sum_err / n_clients : 0.0;
    }

    return m;
}

// -----------------------------------------------------------------------
// print_hetero_report
// -----------------------------------------------------------------------

void print_hetero_report(const HeteroSimMetrics& m) {
    std::cout << "\n  Scheduler      : " << m.scheduler_name << "\n";
    std::cout << "  Quanta         : " << m.total_quanta << "\n";
    std::cout << "  Total work     : " << dbl(m.total_work_dispensed, 1) << "\n\n";

    // Header.
    std::cout << std::left
              << std::setw(10) << "Client"
              << std::setw(6)  << "Tkts"
              << std::setw(8)  << "Wins"
              << std::setw(10) << "AbsErr"
              << std::setw(8)  << "Jitter"
              << std::setw(10) << "EffWork"
              << std::setw(12) << "WorkIntend"
              << std::setw(10) << "CompErr"
              << std::setw(10) << "WorkTpR"
              << "\n";
    std::cout << std::string(84, '-') << "\n";

    for (const auto& c : m.clients) {
        std::cout << std::left
                  << std::setw(10) << c.name
                  << std::setw(6)  << c.tickets
                  << std::setw(8)  << c.wins
                  << std::setw(10) << dbl(c.absolute_error, 2)
                  << std::setw(8)  << dbl(c.jitter, 2)
                  << std::setw(10) << dbl(c.effective_work, 1)
                  << std::setw(12) << dbl(c.work_intended, 1)
                  << std::setw(10) << dbl(c.computational_error, 2)
                  << std::setw(10) << dbl(c.work_throughput_ratio, 4)
                  << "\n";
    }

    // Convergence summary.
    std::cout << "\n  Convergence (mean comp. error per "
              << m.window_size << "-quantum window):\n  ";
    int show = std::min(static_cast<int>(m.convergence_windows.size()), 10);
    for (int i = 0; i < show; ++i)
        std::cout << dbl(m.convergence_windows[i], 3) << " ";
    if (static_cast<int>(m.convergence_windows.size()) > show)
        std::cout << "... " << dbl(m.convergence_windows.back(), 3);
    std::cout << "\n";
}

// -----------------------------------------------------------------------
// print_hetero_comparison
// -----------------------------------------------------------------------

void print_hetero_comparison(const std::vector<HeteroSimMetrics>& all) {
    if (all.empty()) return;

    std::cout << "\n";
    print_separator();
    std::cout << "  COMPARISON SUMMARY\n";
    print_separator();

    for (const auto& m : all) {
        print_separator(80);
        print_hetero_report(m);
    }
    print_separator();

    // Cross-scheduler table.
    std::cout << "\n  Metric comparison across schedulers:\n\n";
    std::cout << std::left
              << std::setw(28) << "Scheduler"
              << std::setw(12) << "MaxAbsErr"
              << std::setw(12) << "MaxCompErr"
              << std::setw(12) << "MaxJitter"
              << std::setw(14) << "AvgWorkTpErr"
              << "\n";
    std::cout << std::string(78, '-') << "\n";

    for (const auto& m : all) {
        double max_abs  = 0.0;
        double max_comp = 0.0;
        double max_jit  = 0.0;
        double avg_wtp  = 0.0;
        for (const auto& c : m.clients) {
            max_abs  = std::max(max_abs,  c.absolute_error);
            max_comp = std::max(max_comp, c.computational_error);
            max_jit  = std::max(max_jit,  c.jitter);
            avg_wtp += std::abs(c.work_throughput_ratio
                        - static_cast<double>(c.tickets)
                          / std::accumulate(
                              m.clients.begin(), m.clients.end(), 0,
                              [](int s, const HeteroClientMetrics& x){ return s + x.tickets; }));
        }
        if (!m.clients.empty()) avg_wtp /= m.clients.size();

        std::cout << std::left
                  << std::setw(28) << m.scheduler_name
                  << std::setw(12) << dbl(max_abs, 2)
                  << std::setw(12) << dbl(max_comp, 2)
                  << std::setw(12) << dbl(max_jit, 2)
                  << std::setw(14) << dbl(avg_wtp, 4)
                  << "\n";
    }
    std::cout << "\n";
}
