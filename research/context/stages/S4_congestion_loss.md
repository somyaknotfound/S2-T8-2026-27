# Stage 4 — Congestion, burst loss and the delay-aware rule (result 4)

**Main session model:** Sonnet. C++ goes to `ndnsim-dev` (Opus); analysis to `results-analyst` (Opus). **Prerequisite:** S3 done.

## Claim to establish
Under congestion and burst loss on the upstream path, the best amount of approximation depends on load and on position. No fixed α is right everywhere.

A **delay-aware** rule derived from the paper's own delay equation adapts automatically:
- answer iff `(1 − F(s'))·(δ + D_retry) < D_up`;
- equivalently, `s' ≥ F⁻¹(1 − D_up/(δ + D_retry))`;
- it tracks the best fixed α across loads.

**Direction check:** congestion upstream ⇒ D_up ↑ ⇒ threshold ↓ ⇒ caches answer locally **more**. (External review #1 had this backwards.)

Report an honest verdict even if the result is negative.

## What NDN in ndnSIM can and cannot do (design_notes Q3)
- **No RED/CoDel:** NDN bypasses ns-3 traffic control. Congestion means a point-to-point DropTail queue: `p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", "<N>p")` with a limited `DataRate`.
- **Loss:** a device-level `ReceiveErrorModel` works for NDN (`annotated-topology-reader.cpp:398-429`).

## Code changes (`build/scenario/`)
1. **`extensions/gilbert-elliott-error-model.{hpp,cpp}`** — `class GilbertElliottErrorModel : public ns3::ErrorModel`.
   - Attributes: `PGoodToBad`, `PBadToGood`, `LossGood` (default 0), `LossBad` (default 1).
   - `DoCorrupt` advances the 2-state Markov chain once per packet.
   - Reuse last semester's parameters (`research/documents/report(last sem).pdf`: p_bg = 0.30, mean burst ≈ 3.3 packets, with p_gb set to hit the target average loss).
2. **`scenarios/tandem.cpp`**:
   - `--bottleneckRate`, `--queuePackets` for the cache-to-cache links.
   - `--crossRate`: background NDN traffic, a `ndn::ConsumerCbr` on prefix `/cross` at v1 with a producer at the origin. It shares the upstream links but never touches our caches' logic.
   - `--lossPgb --lossPbg`: a GE model on chosen links.
   - `--interestLifetimeMs`: 2000 ms is far too long once loss exists; use about 100 ms.
3. **Delay-aware policy** in `sim-controller` and `similarity-strategy`:
   - **D_up per node:** an EWMA (Jacobson/Karels SRTT, α = 1/8) of forwarded-Interest RTTs. Record the send time when the strategy forwards a `/sim` Interest (key = full name). In `afterReceiveData` (default `sendDataToAll` behaviour preserved), sample now − sendTime.
   - **F per node:** acceptance per similarity bin (e.g. 20 bins over [0,1]).
     - When the node answers `(client, request)` with similarity s', remember `(s', time)`.
     - A later attempt of the same `(client, request)` reaching this node within window W counts as a rejection for that bin.
     - No retry within W counts as an acceptance.
     - Use decayed counts; ε-greedy exploration (ε = 0.05) so the low bins get samples.
   - **D_retry ≈ δ + D_up** (on a retry, this node forwards).
   - **Exact hit** (s' = 1): always answer (F(1) = 1).
4. **CSV:** add `policy`, `bottleneck_rate`, `cross_rate`, `loss_pgb`, `loss_pbg` columns.

## Experiments
- **Load sweep:** `crossRate` set for upstream utilisation {0, 0.3, 0.6, 0.8, 0.9}, no loss.
- **Loss sweep:** GE average loss {0, 1, 5, 10}%, no cross traffic.
- **Policies:** blind α ∈ {0.1, 0.3, 0.5, 0.7, 0.9}; threshold (oracle s_min); delay-aware.
- **Users:** a fixed s per run, plus one mixed-β run (users draw s from {0.9, 0.95, 1.0}).
- **Metrics:** mean ± CI and p95 delay, delivered similarity, censored share, cross-traffic throughput and RTT (the externality), and queue drops (`ndn::L2RateTracer` / `L3RateTracer`).
- **Gate:** the delay-aware rule is within the CI of, or better than, the best fixed α at each load point, or there's a written explanation of where it isn't and why.

## Figures for the report
1. Delay vs upstream utilisation: each fixed α against delay-aware.
2. The learned threshold s* per node position vs load (it should fall with load and be stricter near the origin).
3. Delay vs GE loss rate.
4. Cross-traffic throughput: similarity caching relieves the bottleneck.

## Implementation design (8 Oct 2026; supersedes the sketch above where they differ)

Written after S3, from the NFD sources and S3 results. Where this section and the sketch above disagree, this section wins.

### Corrections to the sketch
1. **The rejection penalty counts δ once.** Answering costs RTT_h now, plus, with probability 1−F(s'), a rejection: δ + RTT_h (the retry climbs back to this node) + D_up. Forwarding costs RTT_h + D_up. So:

   **answer iff (1 − F(s'))·(δ + RTT_h + D_up) < D_up**

   The sketch's "D_retry ≈ δ + D_up" added δ twice. The direction check still holds: D_up ↑ ⇒ answer more.
2. **The baseline is exclusion with a fixed α, not blind with a fixed α.** Blind caching has the S2 heavy tail, so anything with exclusion-on-retry beats it trivially.
   - The fair comparison has the same information (caches never see s) and the same retry mechanism (the `excl` list). Only the answer rule differs: fixed coin vs. cost rule.
   - Blind (the paper) stays as a reference point. Threshold (s_min in the Interest, α = 1) is the oracle lower bound: it never causes a rejection.
3. **The load sweep needs mixed users.** With one fixed s per run, the learned F is a 0/1 step and the rule ignores D_up.
   - Load only matters where 0 < F < 1, i.e. when users differ. Main runs: each request draws s from {0.9, 0.95, 1.0}.
   - With 20 bins, the bin edges 0.90 / 0.95 align with those thresholds, so F ≈ 1/3 on [0.90, 0.95), 2/3 on [0.95, 1.0), and 1 at s' = 1.
   - Fixed-s runs are kept as a sanity check: delay-aware should then match the oracle.

### Mechanisms (all NDN-native, no simulator shortcuts in the decision)
- **Retry feedback reuses S3's `excl` list.**
  - A delay-aware cache that answered (client, request) with s' remembers the bin of s'.
  - If a later attempt of that request names the cache in `excl`, that's a rejection, and the cache forwards (bounded attempts, as in exclusion).
  - If no retry names the cache within window W (attribute, default 1 s), that's an acceptance.
  - A retry that does *not* name the cache means its answer was lost, not rejected. No count is recorded, and the cache may answer again.
- **F per node:** 20 bins of accept/reject counts with a Laplace prior (an optional decay attribute is there for a drifting user population; default 1, because every experiment here has a fixed one). s' = 1 always answers. **Exploration stops once the decision is settled:** a bin answers anyway only while the rule gives different answers at F ± 2 standard errors. A fixed budget of 30 samples per bin was tried first: it kept exploring bins whose decision was already obvious (e.g. [0.90, 0.95) for s = 0.95 with an idle upstream), and rejections were still 6.6% of requests after 1,750. Permanent ε-greedy (the sketch's 0.05) would add a constant cost of wrong answers and keep delay-aware from ever matching the oracle.
- **D_up per node:** Jacobson/Karels SRTT (gain 1/8) over forwarded /sim Interests.
  - Samples come from `afterReceiveData`: now − forward time.
  - An Interest that expires unsatisfied, seen via the forwarder's `beforeExpirePendingInterest` signal, contributes now − forward time, i.e. the lifetime the client lost. This makes D_up see loss, not only queueing.
- **RTT_h:** consumers set HopLimit H0 = 32, and NFD decrements it at every node, so h = H0 − 1 − HopLimit seen.
  - The serving cache echoes the HopLimit it saw in the Data payload; this field is added only when the Interest has a HopLimit, so S2/S3 packets stay byte-identical.
  - A forwarding node therefore knows how many hops up its answer came from, and keeps an EWMA of per-hop RTT. RTT_h = h × per-hop RTT.
- **Learned threshold s\*** = the smallest s' the rule would answer. It's logged per node every N requests to `<out>.nodes.csv`, for the convergence and position figures.

### Scenario and measurement
- **Links:** all five tandem links get `--bottleneckRate` (default 10 Mbps) and `--queuePackets` (DropTail).
- **Cross traffic:** `--crossRate` (Interests/s) drives `ConsumerCbr` (exponential gaps) at v1 toward a `Producer` at the origin. With `--crossPayload` = 1024 B, its Data loads every link downstream; our Data queues behind it.
  - Utilisation u = rate × Data bytes × 8 / link rate, with Data bytes measured, not assumed.
- **Loss:** `--lossPgb`, `--lossPbg` set a Gilbert–Elliott model (LossGood = 0, LossBad = 1) on both directions of every tandem link. With p_bg = 0.30 (mean burst 3.3), p_gb = L·p_bg/(1−L) gives average loss L.
- **Interest lifetime:** `--interestLifetimeMs`, chosen from calibration at well above the p99 RTT under the heaviest load.
- **Per-run summary** (`<out>.run.csv`): cross-traffic Data received and mean delay, queue drops, error-model drops, simulated time.
- **Warm-up:** 2000 requests per run; the first 500 are excluded from the metrics, for every policy alike.

### Build order (each step verified before the next)
1. **Infrastructure:** GE model, flags, cross traffic, per-request s (`--thresholds`), summary file. **Check:** with default flags, S3 cells replay byte-identically on the S3 columns.
2. **Calibration:** utilisation and RTT vs `crossRate`; measured GE loss vs target. Fix the lifetime and queue size from this.
3. **Delay-aware policy.** **Check:** with fixed s and no load it learns F as a step at s, its s\* converges to s, and its delay is within CI of the oracle.
4. **Sweeps:**
   - load: u ∈ {0, 0.3, 0.6, 0.8, 0.9};
   - loss: L ∈ {0, 1, 5, 10}%;
   - policies: exclusion α ∈ {0.1, 0.3, 0.5, 0.7, 0.9, 1.0}, delay-aware, threshold oracle, blind α ∈ {0.5, 0.9};
   - mixed users; 5 runs each.
5. **Analysis and gate:** delay-aware within the CI of, or better than, the best fixed-α exclusion at every load and loss point, or a written explanation.

## Results (8 Oct 2026)
Done. The gate passes at every load, loss and fixed-user point. Numbers, verdicts and the learned state are in `research/results/stage4/analysis.md`; provenance is in `sweep.md`; figures are `submission/figures/stage4_{load,loss,learned,cross}.pdf`. The summary is in STATUS.md, "S4 results".

## Defence notes
- *Convergence of coupled learners:* time-scale separation (EWMA on 1/8 vs decayed counts); show convergence plots.
- *Censored feedback:* sensitivity to window W.
- *Identity for linking retries:* the `(client, request)` fields act as a per-request nonce, not a persistent user ID.
