# Stage 2 sweep: reproduce the base paper (results 1 and 2)

**Purpose:**
- Result 1: blind similarity caching with per-attempt placement should match the paper's corrected model.
- Result 2: with contents persisting across a request's retries, delay shows the heavy tail predicted by `build/retry_correlation_check.py`.

**How to run:**
- Driver: `build/experiments/stage2_sweep.sh`, with 10 runs in parallel.
- Binary: `/root/scenario-build/tandem`, the debug build. The optimized build segfaults when our strategy answers; see `research/context/design_notes.md`.
- ndnSIM commit: `90d50396654dabad54b6979f2dc8fa929ade544c` (2.9, NFD 22.02).

**Fixed parameters:**
- Topology: tandem of 5 caches plus the origin, one client per cache.
- Links: 1 ms one-way, 1 Gbps (transmission delay is negligible, as in the paper's infinite-bandwidth assumption).
- Catalogue C = 100 items; cache size B = 10.
- Synthetic similarity s(a,b) = 1 − |a−b|/99.
- δ = 10 ms; Interest lifetime 2 s; attempt cap 5000 (requests hitting it are marked `censored`).
- Each request picks its client and its item uniformly at random. Requests run one at a time.

**Grid:** 120 runs = 3 α values × 4 thresholds × 2 placements × 5 runs.

| Parameter | Values |
|---|---|
| α | 0.1, 0.5, 0.9 |
| threshold s | 0.9, 0.95, 0.99, 1.0 (for C = 100, s = 0.99 and s = 1.0 both mean exact item only) |
| placement | `per-attempt` (the model's independence assumption); `per-request` (contents fixed for the whole retry chain) |
| runs | 1–5 (`RngSeedManager` run number) |
| requests per run | 1000, except α = 0.9 with `per-request`: 200 per run (1000 per cell) |

**Outputs:**
- Per-run CSVs and the merged `all.csv` go in `research/results/raw/stage2/` (gitignored, about 12 MB).
- The summary and comparison with the model go in this folder.

**Compare against:** `research/results/retry_correlation.txt`.
- `per-attempt` → the "model cap" column.
- `per-request` → the "persist cap" column.
- The capped columns use the same 5000-attempt cap as the simulator.
