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

## Defence notes
- *Convergence of coupled learners:* time-scale separation (EWMA on 1/8 vs decayed counts); show convergence plots.
- *Censored feedback:* sensitivity to window W.
- *Identity for linking retries:* the `(client, request)` fields act as a per-request nonce, not a persistent user ID.
