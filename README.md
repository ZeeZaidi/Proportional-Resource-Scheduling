# Stride & Lottery Scheduling Simulator

A discrete-event simulator in C++ that compares lottery scheduling (Waldspurger & Weihl, OSDI 1994) and stride scheduling (Waldspurger & Weihl, MIT-LCS-TM-528, 1995). See report directory for details.

## Build and Run

```bash
make                          # compile with g++ -std=c++17 -O2
make run                      # build and run, output to stdout
make run FILE=results.txt     # build and run, write output to file
make clean                    # remove build artifacts
```
