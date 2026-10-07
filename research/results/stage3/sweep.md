# Stage 3 sweep: threshold-carrying requests and exclusion on retry (result 3)

**Purpose:**
- Result 3: when the request carries s_min (`threshold`), the heavy tail of Stage 2 disappears, delay equals the paper's Eq. (8) exactly, and α = 1 is delay-optimal.
- The cheaper fix that keeps caches blind (`exclusion`: a retry names the caches that served rejected answers, and they forward) bounds a request at caches + 1 attempts.
- `blind` is the base paper, rerun on the same grid as the reference.

**How to run:**
- Driver: `build/experiments/stage3_sweep.sh`, with 10 runs in parallel.
- Binary: `/root/scenario-build/tandem`, the debug build. The optimized build segfaults when our strategy answers; see `research/context/design_notes.md`.
- Binary md5: `0430bfc507dd7f6599d709e23efa7bfe`.
- ndnSIM commit: `90d50396654dabad54b6979f2dc8fa929ade544c` (2.9, NFD 22.02), plus `build/env/ndn-cxx-scheduler-leak.patch` (debug library md5 `8a61262a7ddb7e5d15c531d654110242`).
  - The patch only removes an event leak that made long runs quadratically slow. It changes no output: 216 runs are byte-identical before and after (design_notes).
- Wall time: 360 runs in 18 min 24 s (2026-10-07, 18:06:58–18:25:22 UTC).
  - A first attempt on the unpatched library ran its 10 heaviest jobs for over 50 min without finishing. It was stopped and its partial output deleted.

**Follow-up (gate 1 re-test):**
- `build/experiments/stage3_followup.sh` adds runs 6–25 (1000 requests each) to every threshold and exclusion cell: 960 runs, written to `research/results/raw/stage3_followup/`.
- Why: the 5-run first pass had 23 of 24 threshold cells within their 95% CI, which is expected by chance (see `analysis.md`).
- Every gated cell is re-tested, not only the one that missed. Re-testing only the miss would bias the outcome toward passing.
- The fresh seeds also give `static` placement 25 independent cache configurations per cell instead of 5.

**Fixed parameters (as in Stage 2):**
- Topology: tandem of 5 caches plus the origin, one client per cache.
- Links: 1 ms one-way, 1 Gbps.
- Catalogue C = 100 items; cache size B = 10.
- Synthetic similarity s(a,b) = 1 − |a−b|/99.
- δ = 10 ms; Interest lifetime 2 s; attempt cap 5000 (requests hitting it are marked `censored`).
- Each request picks its client and its item uniformly at random. Requests run one at a time.

**Grid:** 360 runs = 3 policies × 4 α values × 3 thresholds × 2 placements × 5 runs.

| Parameter | Values |
|---|---|
| policy | `blind` (paper), `threshold` (s_min in ApplicationParameters), `exclusion` (rejected caches named in ApplicationParameters) |
| α | 0.1, 0.5, 0.9, 1.0 |
| threshold s | 0.9, 0.95, 1.0 (s = 0.99 dropped: for C = 100 it equals s = 1.0) |
| placement | `per-request` (contents redrawn per request, fixed across its retries); `static` (drawn once per run) |
| runs | 1–5 (`RngSeedManager` run number); threshold and exclusion also get 6–25 in the follow-up |
| requests per run | 1000, except `blind` with α ≥ 0.9: 200 per run (1000 per cell) |

Two notes on the grid:
- **The 200-request cap is wider than the plan's.** The plan capped only blind α = 0.9 per-request. Blind α = 1.0 and blind static at α ≥ 0.9 are as heavy or heavier: at α = 1, s = 1 about 90% of requests run to the 5000-attempt cap. At 1000 requests the sweep would take about 1.5 h. Blind cells are not gated.
- **Static placement is paired across cells.** For a given run number, `static` draws the same five cache contents in every policy, α and s cell: the contents are drawn first from the controller's stream, whose seed depends only on the run number. Policy comparisons on `static` are therefore paired.

**Outputs:**
- Per-run CSVs and the merged `all.csv` go in `research/results/raw/stage3/` (gitignored).
- The summary, gates and checks go in this folder (`analysis.md`, `summary.csv`). Figures go in `submission/figures/stage3_*.pdf`.

**Compare against:** `research/results/stage3/model.txt` (`python build/retry_correlation_check.py --stage3`). These are exact expectations with contents persisting across retries, and they apply to both placements:
- `blind` → "blind cap" (same 5000-attempt cap as the simulator).
- `threshold` → "threshold". This is the closed form (Eq. 8 with q_i = α·p_i); "thr persist" is the same expectation computed over configurations.
- `exclusion` → "exclusion".

**Model validation, independent of the simulator:**
- For blind, the exact enumeration matches S2's 800-configuration Monte Carlo within its noise.
- A protocol-level Monte Carlo with real B-subsets and coins (60,000 requests per cell) matches the threshold and exclusion expectations in all 24 cells. Worst |z| = 2.14.
