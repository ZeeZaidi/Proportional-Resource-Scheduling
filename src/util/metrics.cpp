#include "metrics.h"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <iostream>
#include <sstream>

// -----------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------

static double mean(const std::vector<int>& v) {
    if (v.empty()) return 0.0;
    return static_cast<double>(std::accumulate(v.begin(), v.end(), 0LL)) / v.size();
}

static double stddev(const std::vector<int>& v) {
    if (v.size() < 2) return 0.0;
    double m = mean(v);
    double sq_sum = 0.0;
    for (int x : v) {
        double d = x - m;
        sq_sum += d * d;
    }
    return std::sqrt(sq_sum / (v.size() - 1));  // sample std dev
}

// -----------------------------------------------------------------------
// compute_metrics
// -----------------------------------------------------------------------

SimMetrics compute_metrics(const SimResult&              result,
                           const std::vector<SimClient>& clients,
                           int                           window_size)
{
    SimMetrics m;
    m.scheduler_name = result.scheduler_name;
    m.total_quanta   = result.total_quanta;
    m.window_size    = window_size;

    // Build total_tickets from the initial clients list.
    // (For dynamic scenarios we use the initial total as the reference.)
    int total_tickets = 0;
    for (const auto& c : clients)
        total_tickets += c.tickets;

    // ---- Per-client metrics ----
    for (const auto& c : clients) {
        const auto& track_it = result.tracks.find(c.id);
        if (track_it == result.tracks.end()) continue;
        const ClientTrack& track = track_it->second;

        int n_active = track.last_active - track.first_active + 1;
        if (n_active <= 0) n_active = 1;

        double fraction = static_cast<double>(c.tickets) / total_tickets;
        double intended = n_active * fraction;

        ClientMetrics cm;
        cm.id      = c.id;
        cm.name    = c.name;
        cm.tickets = c.tickets;
        cm.n_active = n_active;
        cm.wins    = track.wins;
        cm.intended = intended;
        cm.absolute_error = std::abs(track.wins - intended);
        cm.throughput_ratio = static_cast<double>(track.wins) / n_active;
        cm.throughput_error = std::abs(cm.throughput_ratio - fraction);
        cm.mean_response_time = mean(track.inter_win_intervals);
        cm.jitter             = stddev(track.inter_win_intervals);
        cm.pairwise_rel_error = 0.0;  // filled below

        m.clients.push_back(cm);
    }

    // ---- Pairwise relative error ----
    // For each ordered pair (i, j), compute:
    //   T_ij = t_i + t_j
    //   specified_i = (wins_i + wins_j) * t_i / T_ij
    //   rel_error_ij = |wins_i - specified_i|
    // Store max over all pairs into each client's pairwise_rel_error.
    for (auto& ci : m.clients) {
        double max_rel = 0.0;
        for (const auto& cj : m.clients) {
            if (ci.id == cj.id) continue;
            // Find original ticket counts.
            int ti = ci.tickets, tj = cj.tickets;
            int T_ij = ti + tj;
            if (T_ij == 0) continue;
            double n_ij = ci.wins + cj.wins;
            double specified = n_ij * static_cast<double>(ti) / T_ij;
            double rel = std::abs(ci.wins - specified);
            max_rel = std::max(max_rel, rel);
        }
        ci.pairwise_rel_error = max_rel;
    }

    // ---- Convergence windows ----
    // For each window of window_size quanta, compute the mean absolute error
    // across all clients present in that window.
    int num_windows = (result.total_quanta + window_size - 1) / window_size;
    m.convergence_windows.resize(num_windows, 0.0);

    for (int w = 0; w < num_windows; ++w) {
        int q_start = w * window_size + 1;
        int q_end   = std::min((w + 1) * window_size, result.total_quanta);
        int span    = q_end - q_start + 1;

        // Count wins per client in this window.
        std::unordered_map<int, int> window_wins;
        for (const auto& rec : result.history) {
            if (rec.quantum >= q_start && rec.quantum <= q_end)
                window_wins[rec.winner_id]++;
        }

        // Mean absolute error in this window.
        double sum_err = 0.0;
        int    n_active_clients = 0;
        for (const auto& c : clients) {
            double fraction = static_cast<double>(c.tickets) / total_tickets;
            double intended_w = span * fraction;
            int    actual_w   = 0;
            auto it = window_wins.find(c.id);
            if (it != window_wins.end()) actual_w = it->second;
            sum_err += std::abs(actual_w - intended_w);
            ++n_active_clients;
        }
        m.convergence_windows[w] = (n_active_clients > 0) ? sum_err / n_active_clients : 0.0;
    }

    return m;
}

// -----------------------------------------------------------------------
// Formatting helpers
// -----------------------------------------------------------------------

static std::string dbl(double v, int prec = 3) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(prec) << v;
    return ss.str();
}

static void print_separator(int width = 100) {
    std::cout << std::string(width, '-') << "\n";
}

// -----------------------------------------------------------------------
// print_report
// -----------------------------------------------------------------------

void print_report(const SimMetrics& m) {
    std::cout << "\n  Scheduler : " << m.scheduler_name << "\n";
    std::cout << "  Quanta    : " << m.total_quanta << "\n\n";

    // Header
    std::cout << std::left
              << std::setw(12) << "Client"
              << std::setw(8)  << "Tickets"
              << std::setw(10) << "Intended"
              << std::setw(8)  << "Actual"
              << std::setw(10) << "AbsErr"
              << std::setw(12) << "PairRelErr"
              << std::setw(12) << "Throughput"
              << std::setw(10) << "TpErr"
              << std::setw(10) << "MeanRT"
              << std::setw(10) << "Jitter"
              << "\n";
    std::cout << std::string(102, '-') << "\n";

    for (const auto& c : m.clients) {
        std::cout << std::left
                  << std::setw(12) << c.name
                  << std::setw(8)  << c.tickets
                  << std::setw(10) << dbl(c.intended, 1)
                  << std::setw(8)  << c.wins
                  << std::setw(10) << dbl(c.absolute_error, 2)
                  << std::setw(12) << dbl(c.pairwise_rel_error, 2)
                  << std::setw(12) << dbl(c.throughput_ratio, 4)
                  << std::setw(10) << dbl(c.throughput_error, 4)
                  << std::setw(10) << dbl(c.mean_response_time, 2)
                  << std::setw(10) << dbl(c.jitter, 2)
                  << "\n";
    }

    // Convergence summary (first few + last window).
    std::cout << "\n  Convergence (mean abs error per " << m.window_size << "-quantum window):\n  ";
    int show = std::min(static_cast<int>(m.convergence_windows.size()), 10);
    for (int i = 0; i < show; ++i)
        std::cout << dbl(m.convergence_windows[i], 3) << " ";
    if (static_cast<int>(m.convergence_windows.size()) > show)
        std::cout << "... " << dbl(m.convergence_windows.back(), 3);
    std::cout << "\n";
}

// -----------------------------------------------------------------------
// print_comparison
// -----------------------------------------------------------------------

void print_comparison(const std::vector<SimMetrics>& all) {
    if (all.empty()) return;

    std::cout << "\n";
    print_separator();
    std::cout << "  COMPARISON SUMMARY\n";
    print_separator();

    // Print each scheduler's report.
    for (const auto& m : all) {
        print_separator(60);
        print_report(m);
    }
    print_separator();

    // Cross-scheduler comparison table: pairwise relative error and jitter.
    std::cout << "\n  Metric comparison across schedulers:\n\n";
    std::cout << std::left
              << std::setw(26) << "Scheduler"
              << std::setw(16) << "MaxPairRelErr"
              << std::setw(14) << "MaxJitter"
              << std::setw(14) << "AvgTpErr"
              << "\n";
    std::cout << std::string(70, '-') << "\n";

    for (const auto& m : all) {
        double max_rel  = 0.0;
        double max_jit  = 0.0;
        double avg_tperr = 0.0;
        for (const auto& c : m.clients) {
            max_rel  = std::max(max_rel,  c.pairwise_rel_error);
            max_jit  = std::max(max_jit,  c.jitter);
            avg_tperr += c.throughput_error;
        }
        if (!m.clients.empty()) avg_tperr /= m.clients.size();

        std::cout << std::left
                  << std::setw(26) << m.scheduler_name
                  << std::setw(16) << dbl(max_rel, 3)
                  << std::setw(14) << dbl(max_jit, 3)
                  << std::setw(14) << dbl(avg_tperr, 4)
                  << "\n";
    }
    std::cout << "\n";
}
