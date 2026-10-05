# Stage 2 results — simulated vs. expected delay

Mean ± 95% CI over runs (run-level t interval). Expected = capped analytical value (`model cap` for per-attempt, `persist cap` for per-request).

| α | s | placement | mean ± CI (ms) | expected (ms) | within CI | median | p99 | censored |
|---|---|---|---|---|---|---|---|---|
| 0.1 | 0.9 | per-attempt | 5.31 ± 0.13 | 5.28 | yes | 4.0 | 20.0 | 0.000 |
| 0.1 | 0.9 | per-request | 5.32 ± 0.21 | 5.33 | yes | 4.0 | 22.0 | 0.000 |
| 0.1 | 0.95 | per-attempt | 6.64 ± 0.21 | 6.51 | yes | 6.0 | 30.0 | 0.000 |
| 0.1 | 0.95 | per-request | 6.64 ± 0.19 | 6.65 | yes | 6.0 | 32.0 | 0.000 |
| 0.1 | 0.99 | per-attempt | 9.97 ± 0.18 | 9.96 | yes | 6.0 | 48.0 | 0.000 |
| 0.1 | 0.99 | per-request | 10.10 ± 0.39 | 10.05 | yes | 6.0 | 50.0 | 0.000 |
| 0.1 | 1.0 | per-attempt | 9.97 ± 0.18 | 9.96 | yes | 6.0 | 48.0 | 0.000 |
| 0.1 | 1.0 | per-request | 10.10 ± 0.39 | 10.05 | yes | 6.0 | 50.0 | 0.000 |
| 0.5 | 0.9 | per-attempt | 2.93 ± 0.11 | 2.99 | yes | 2.0 | 22.0 | 0.000 |
| 0.5 | 0.9 | per-request | 4.25 ± 0.48 | 4.22 | yes | 2.0 | 52.0 | 0.000 |
| 0.5 | 0.95 | per-attempt | 6.73 ± 0.32 | 7.00 | yes | 2.0 | 42.0 | 0.000 |
| 0.5 | 0.95 | per-request | 13.42 ± 1.19 | 13.91 | yes | 2.0 | 170.0 | 0.000 |
| 0.5 | 0.99 | per-attempt | 47.50 ± 2.60 | 45.97 | yes | 24.0 | 292.0 | 0.000 |
| 0.5 | 0.99 | per-request | 103.60 ± 6.07 | 97.77 | yes | 30.0 | 968.2 | 0.000 |
| 0.5 | 1.0 | per-attempt | 47.50 ± 2.60 | 45.97 | yes | 24.0 | 292.0 | 0.000 |
| 0.5 | 1.0 | per-request | 103.60 ± 6.07 | 97.77 | yes | 30.0 | 968.2 | 0.000 |
| 0.9 | 0.9 | per-attempt | 1.62 ± 0.18 | 1.72 | yes | 0.0 | 20.0 | 0.000 |
| 0.9 | 0.9 | per-request | 40.73 ± 17.24 | 54.02 | yes | 0.0 | 1284.3 | 0.000 |
| 0.9 | 0.95 | per-attempt | 6.68 ± 0.49 | 6.38 | yes | 0.0 | 50.0 | 0.000 |
| 0.9 | 0.95 | per-request | 896.07 ± 455.14 | 755.96 | yes | 0.0 | 29816.7 | 0.008 |
| 0.9 | 0.99 | per-attempt | 81.59 ± 3.57 | 80.62 | yes | 52.0 | 434.0 | 0.000 |
| 0.9 | 0.99 | per-request | 14521.56 ± 2069.77 | 13709.01 | yes | 969.0 | 51186.5 | 0.220 |
| 0.9 | 1.0 | per-attempt | 81.59 ± 3.57 | 80.62 | yes | 52.0 | 434.0 | 0.000 |
| 0.9 | 1.0 | per-request | 14521.56 ± 2069.77 | 13709.01 | yes | 969.0 | 51186.5 | 0.220 |

24 of 24 cells within the 95% CI of the expected value.
