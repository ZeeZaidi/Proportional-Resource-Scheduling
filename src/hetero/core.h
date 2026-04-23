#pragma once

#include <vector>
#include <cmath>

// -----------------------------------------------------------------------
// Core types for heterogeneous multicore simulation.
//
// Models Intel Alder Lake-style big.LITTLE layout:
//   P-cores: performance_scale = 2.0  (HFI capability ~255)
//   E-cores: performance_scale = 1.0  (HFI capability ~128)
//
// performance_scale is mutable to support mid-simulation throttle events
// (SCALE_CHANGE). All scheduler and metrics code reads it live each quantum.
// -----------------------------------------------------------------------

enum class CoreType { PCORE, ECORE };

struct Core {
    int      id;
    CoreType type;
    double   performance_scale;   // S_C: 2.0 for P, 1.0 for E (mutable)

    // Integer fixed-point representation for pass arithmetic.
    // Denominator is 1000: S=2.0 → 2000, S=1.0 → 1000, S=1.4 → 1400.
    // Supports S_C to 3 decimal places with no overflow impact.
    long long scale_int() const {
        return static_cast<long long>(std::round(performance_scale * 1000.0));
    }
};

// Factory: 4 P-cores (ids 0-3, S=2.0) + 4 E-cores (ids 4-7, S=1.0).
inline std::vector<Core> make_default_cores() {
    std::vector<Core> cores;
    cores.reserve(8);
    for (int i = 0; i < 4; ++i)
        cores.push_back({i, CoreType::PCORE, 2.0});
    for (int i = 4; i < 8; ++i)
        cores.push_back({i, CoreType::ECORE, 1.0});
    return cores;
}

// Sum of performance_scale across all cores.
inline double total_scale(const std::vector<Core>& cores) {
    double sum = 0.0;
    for (const auto& c : cores) sum += c.performance_scale;
    return sum;
}
