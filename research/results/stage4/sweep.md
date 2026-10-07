# Stage 4 sweep: delay-aware caching under backhaul congestion and burst loss (result 4)

**Purpose:** show that no fixed α is right everywhere, and that the delay-aware rule tracks or beats the best fixed α at every load and loss point. The delay-aware rule answers iff (1 − F(s′))·(δ + RTT_h + D_up) < D_up, and it learns F and D_up online. The design is in `research/context/stages/S4_congestion_loss.md`, "Implementation design".

**How to run:**
- Driver: `build/experiments/stage4_sweep.sh` (10 runs in parallel); analysis: `build/analysis/stage4_summary.py`.
- Binary: `/root/scenario-build/tandem`, debug build, md5 `60cc365743318a8f5a592e3c41d16c93`.
- ndnSIM commit: `90d50396654dabad54b6979f2dc8fa929ade544c`, plus `build/env/ndn-cxx-scheduler-leak.patch` (library md5 `8a61262a7ddb7e5d15c531d654110242`).
- Wall time: 485 runs in 23 min 28 s (2026-10-07, 19:33:12–19:56:40 UTC). 0 failed.
- Before the sweep, five S2/S3 runs replayed byte-for-byte with this binary on their original columns.

**Fixed parameters:**
- **Topology:** tandem of 5 caches plus the origin, one client per cache. C = 100, B = 10, s(a,b) = 1 − |a−b|/99, τ = 1 ms, δ = 10 ms.
- **Links:** 1 Gbps, except the **backhaul v5–origin at 2 Mbps**. DropTail queues of 100 packets per link direction.
- **Requests:**
  - Interest lifetime 200 ms; attempt cap 5000.
  - Contents are drawn per request (`per-request`) and persist across the request's retries.
  - 2000 requests per run; **the first 500 are warm-up and excluded from every metric**, for every policy.
- **Users:** *mixed*, each request drawing its s uniformly from {0.9, 0.95, 1.0}, except the `fixed` scenario (s = 0.95).
- **Cross traffic:** `ConsumerCbr` with exponential gaps at v1, toward a `Producer` with a 1024 B payload at the origin. Each cross Data is 1075 B on the wire (calibrated), so offered load = rate × 1075 × 8 / 2 Mbps.
- **Loss:** Gilbert–Elliott on both directions of every link, p_bg = 0.3 (mean burst 3.3 packets), p_gb = L·0.3/(1−L). Checked against the closed forms with `ge-check` (2 M draws): loss 1.00 / 5.03 / 10.03%, mean burst 3.34–3.37.
- **Delay-aware learner:**
  - 20 similarity bins with an isotonic (non-decreasing) acceptance estimate F;
  - exploration only while the decision differs at F ± 2 standard errors;
  - acceptance window 1 s; SRTT gain 1/8; HopLimit 32.

**Grid:** 485 runs, 5 runs (seeds 1–5) per cell.

| Scenario | Points | Policies |
|---|---|---|
| `load` | cross rate 0, 70, 140, 186, 209, 221 /s = offered load 0, 0.3, 0.6, 0.8, 0.9, 0.95 | delay-aware; threshold α = 1 (oracle); blind α = 0.5; exclusion α ∈ {0.1, 0.3, 0.5, 0.7, 0.9, 1.0} |
| `loss` | mean loss 1, 5, 10% (0% reuses the `load` zero-load cells: same configuration and seeds) | same 9 |
| `fixed` | offered load 0 and 0.9, all users s = 0.95 | same minus blind |

Blind α = 0.9 is excluded under cross traffic. Its heavy tail (S2/S3: 14.5 s mean for s = 1) stretches a run to hours of simulated time, which cross traffic turns into tens of millions of packet events. Its idle behaviour is in S2/S3.

**Outputs** (`research/results/raw/stage4/<scenario>/`, gitignored):
- `req/*.csv`: one row per request.
- `run/*.csv`: network counters (utilisation, cross-traffic delay, drops).
- `nodes/*.csv`: delay-aware learned state every 100 requests. Trace rows carry no scenario columns, so the analysis takes the cross rate from the file name.
- `req.csv`, `run.csv`, `nodes.csv`: merged.

**Measured, not assumed** (all per-cell values in `analysis.md`):
- Link loss 1.0–1.1 / 4.9–5.1 / 9.7–10.3% for the 1 / 5 / 10% targets.
- Backhaul utilisation equals the offered load plus our own traffic (≤ 0.045).
- 0 queue drops in every run.
- Timeouts in the load sweep: ≤ 0.001 per request (the 0.95 point only).
