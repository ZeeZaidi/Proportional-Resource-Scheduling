# Stride & Lottery Scheduling Simulator

A discrete-event simulator in C++ that implements and compares lottery scheduling (Waldspurger & Weihl, OSDI 1994) and stride scheduling (Waldspurger & Weihl, MIT-LCS-TM-528, 1995). Four scheduler variants are provided: O(n) list-based lottery, O(log n) segment-tree lottery, dynamic stride, and hierarchical stride. Ten test scenarios reproduce figures from the original papers and measure proportional accuracy, latency jitter, throughput, and convergence rate across both static and dynamic allocation settings.

## Build and Run

```bash
make                          # compile with g++ -std=c++17 -O2
make run                      # build and run, output to stdout
make run FILE=results.txt     # build and run, write output to file
make clean                    # remove build artifacts
```

## Source Files

| File | Description |
|------|-------------|
| `src/common.h` | Abstract `Scheduler` base class; defines `SimClient` and `STRIDE1 = 1<<20`. |
| `src/lottery_scheduler_list.h/.cpp` | O(n) linear-scan lottery scheduler (paper §4.2). |
| `src/lottery_scheduler_tree.h/.cpp` | O(log n) segment-tree lottery scheduler. |
| `src/stride_scheduler.h/.cpp` | Dynamic stride scheduler with join/leave/modify and remain-rescaling (paper Fig. 3–5). |
| `src/stride_scheduler_hierarchical.h/.cpp` | Hierarchical stride scheduler with O(log n_c) error bound (paper §4, Fig. 6–7). |
| `src/event_simulator.h/.cpp` | Discrete-event engine; processes JOIN/LEAVE/MODIFY events and records per-quantum winners. |
| `src/metrics.h/.cpp` | Computes absolute error, pairwise relative error, throughput ratio, jitter, and convergence windows. |
| `src/main.cpp` | Defines and runs all 10 test scenarios. |
| `Makefile` | Build rules for `scheduler_sim`. |
| `test_summary.txt` | Narrative analysis: scenario descriptions, assumption validation, and per-scenario results. |
| `results.txt` | Raw simulator output (regenerate with `make run FILE=results.txt`). |

## Test Scenarios

1. **Static 7:3, 1000 quanta** — Canonical two-client correctness baseline at a moderate ticket ratio. Validates proportional accuracy, jitter, and convergence window stability (stride paper Fig. 9a,b).

2. **Static 19:1, 1000 quanta** — Stress-tests a severely skewed ratio where the minority client wins only ~5% of quanta. Confirms lottery's geometric inter-win tail and stride's perfectly periodic service with σ→0 (stride paper Fig. 9c,d).

3. **Static 3:2:1, 100 quanta** — Tests three-client fairness at a short time horizon where lottery variance is largest. Exposes rank inversions — a lower-priority client temporarily beating a higher-priority one — that stride avoids (stride paper Fig. 8).

4. **Dynamic ticket modification [2,12]:3, 1000 quanta** — Client A's tickets are redrawn uniformly from [2,12] every 2 quanta; verifies stride's remain-rescaling correctly absorbs mid-stride changes without accumulating error (stride paper Fig. 10).

5. **Skewed 8-client H(7):L1–L7(1), 10000 quanta** — Reproduces the paper's hierarchical vs. basic stride comparison; hierarchical achieves half the pairwise relative error (0.50 vs. 1.50) by interleaving the dominant client more tightly (stride paper Fig. 13).

6. **Dynamic join/leave, 500 quanta** — Two permanent clients (A, B) are joined mid-run by C (q200–400) and D (q300+); exercises stride's remain mechanism that sets a new client's pass to `global_pass + remain` rather than 0, preventing monopolization on re-entry (lottery paper §3.1, stride paper §2.2).

7. **List vs. Tree lottery equivalence + timing, 10000 quanta** — Verifies that the O(n) list and O(log n) segment-tree implementations produce byte-for-byte identical allocations from the same RNG seed (lottery paper §4.2).

8. **Thread thrashing, 600 quanta** — Three churning clients cycle in and out every 20 quanta alongside two stable clients; reveals stride's structural instability under rapid pool changes, where stable clients won ~78/77 quanta versus lottery's ~227/214 out of 600.

9. **Multiprocess ticket inflation, 1000 quanta** — Process P1 spawns an extra thread at q501, unilaterally raising its aggregate share; both schedulers are susceptible to inflation, but stride's transient startup burst amplifies the shift beyond the ticket-proportional expectation (lottery paper §4.4).

10. **Lottery convergence rate (1/√n law), 200 trials × 5 n-values** — Averages absolute error over 200 seeds at n ∈ {100, 316, 1000, 3162, 10000}; confirms E[AbsErr]/√n ≈ 0.375 (constant across all n), matching the theoretical prediction √(p(1-p))·√(2/π) ≈ 0.366 from the half-normal approximation.
