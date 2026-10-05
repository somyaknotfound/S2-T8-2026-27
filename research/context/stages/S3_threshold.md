# Stage 3 — Threshold-carrying requests and bounded retries (result 3)

**Main session model:** Sonnet. The C++ work goes to `ndnsim-dev` (Opus). **Prerequisite:** S2 done (`research/results/stage2/analysis.md`, 24/24 within CI).

## Claim to establish
When a request carries the user's minimum similarity `s_min` and caches answer only if their best match meets it:
1. The heavy tail from S2 disappears. Nothing is ever rejected, so there are zero retries in a loss-free network.
2. Delay equals the paper's own Eq. (8) exactly (see `PROBLEM_STATEMENTS.md` §2.2(b)).
3. **α = 1 is delay-optimal.** If an acceptable item is cached, answering always beats forwarding; α only mattered because caches were blind.

We also add the cheaper fix that keeps caches blind: **exclusion on retry (V2)**. The retry carries the node IDs that served rejected answers, and those nodes forward instead. That bounds attempts at (caches on the path) + 1.

## Code changes (`build/scenario/`)
1. `extensions/sim-controller.hpp/.cpp`
   - Add `enum class Policy { Blind, Threshold, Exclusion }` and `SimConfig::policy`.
   - Add a `policy` column to the CSV.
   - `ShouldAnswer(nodeId, similarity, const RequestParams&)`:
     - Blind: `rand < alpha` (unchanged).
     - Threshold: `similarity >= sMin && rand < alpha` (α kept so we can *show* α=1 is optimal).
     - Exclusion: `!excluded(nodeId) && rand < alpha`.
   - Add an `interest_hops` CSV column: total hops travelled by all attempts of the request (upstream cost).
2. `extensions/sim-common.hpp`
   - `RequestParams { double sMin; std::vector<uint32_t> excluded; }`, encoded as text in the Interest's ApplicationParameters, e.g. `smin=0.95;excl=3,4`.
   - Helpers: `EncodeParams` / `ParseParams`.
   - `setApplicationParameters` appends a ParametersSha256Digest component, so the name grows to six components. `IsSimName` (size ≥ 5) still holds, and Data must keep using the **full** Interest name, which `MakeAnswerData(interest.getName(), …)` already does.
   - **Fallback if ApplicationParameters causes trouble:** put the parameters in a sixth name component instead. Record the choice in design_notes.
3. `extensions/sim-consumer.cpp`
   - Set the parameters on every attempt. On a rejected answer under Exclusion, add `answer.node` to `excluded`.
   - Accumulate `interest_hops += served_hop` per attempt. Use the hop of the serving node: position(served) − position(client).
4. `extensions/similarity-strategy.cpp` — parse the parameters from the Interest and pass them to `ShouldAnswer`.
5. `scenarios/tandem.cpp` — add `--policy=blind|threshold|exclusion`.
- **Done means:** a debug build with 0 warnings in our files, and `smoke`, plus `tandem --policy=blind` reproduces a S2 cell exactly (same seed gives identical rows).

## Analytical model (extend `build/retry_correlation_check.py`, new functions only)
- **Threshold, random placement:** P(served at i) = Π_{j<i}(1 − α·p_j^{≥s})·α·p_i^{≥s} for caches, and the origin takes the rest. Delay = Σ_i 2τ·i·P(i), with no retry term.
- **Threshold, persistent placement:** the same with `hit_j ∈ {0,1}` per configuration, averaged over configurations.
- **Exclusion, persistent:** Monte Carlo over configurations. Each attempt a non-excluded cache answers with probability α. A rejected node joins the excluded set. Repeat until accepted. At most m+1 attempts.

## Experiments
- **Grid:** policy {blind, threshold, exclusion} × α {0.1, 0.5, 0.9, 1.0} × s {0.9, 0.95, 1.0} × placement {per-request, static} × 5 runs.
- Requests per run: 1000; blind with α=0.9 and per-request placement: 200, as in S2.
- **Metrics:** mean ± CI, p95, p99, censored share, mean attempts, mean delivered similarity, `interest_hops` per request, and Jain's fairness index over per-client mean delay.
- **Gate (all three):**
  1. Threshold mode is within the CI of its closed form in every cell.
  2. Threshold has max attempts = 1 and max delay ≤ 2τ·(caches) + ε.
  3. Exclusion has max attempts ≤ caches + 1.
  - If a gate fails twice, stop and write it up in STATUS.md.

## Figures for the report
1. Mean and p99 delay vs s: blind vs exclusion vs threshold at α=0.9, per-request placement, log y.
2. Threshold-mode delay vs α, showing it decreases monotonically to α=1.
3. Upstream Interest hops per request for each policy (the bandwidth argument).

## Defence notes
- *"Isn't V1 trivial?"* The contribution is the analysis: the heavy-tail size (S2), V1 making Eq. (8) exact, and α becoming obsolete.
- *Privacy cost of `s_min`:* it reveals the user's tolerance (PS5 territory).
- *NDN removed Selectors in packet format v0.3:* parameters go in ApplicationParameters, which is the v0.3 way.
