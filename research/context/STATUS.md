# Status — read at the start of every session, update at the end

## Decided (5 Oct 2026)
- **Team T8:** Somyak Priyadarshi Mohanta (241CS257), Pranav Shaji (241CS243), Chris Tony (241CS222).
- **Topic:** *Threshold-Aware Similarity Caching for Named Data Networks: Bounding Retry Delay under Congestion using ns-3.*
  Spreadsheet title: *Threshold-Aware Similarity Caching with Bounded Retries for Named Data Networks.*
- **Tool:** ns-3 + ndnSIM; Python/MATLAB for analysis. The course only allows UnetStack, ns-3 or MATLAB; NeST was dropped on 5 Oct.
- **Scope:** PS1 (threshold-carrying Interests + bounded retry) and PS2 (congestion/loss-aware answer-or-forward). See `PROBLEM_STATEMENTS.md` §6 and `external_review_1_ns3.md`.
- **Deadlines (course manual):** proposal Thu 8 Oct 2026; progress report Thu 22 Oct; final report 2 Nov.
  - The manual says "Thursday" for 2 Nov, but 2 Nov 2026 is a Monday. Confirm with the guide.
  - Required report sections and line budgets: `COURSE_REQUIREMENTS.md`.
- **Proposal:** `Proposal_222_243_257.tex`/`.pdf` at the repo root. Rewritten on 7 Oct to the manual's one-paragraph format (background, challenges, existing work, problem statement, 3 objectives), one page, IEEE references (all verified).
- **Originality:** the guide has accepted the work so far. All code and documents stay original: own figures and text, everything cited, plagiarism < 10%.

## Key facts already established (don't re-derive)
- **Base paper Eq. (6)** is mis-printed: the figures use cumulative RTT. Our corrected model reproduces Fig. 5(a): 9.96 / 45.97 / 80.6 ms at s=1 for α = 0.1 / 0.5 / 0.9.
- **Eqs. (8) and (10)** appear to model a threshold-aware cache and a blind cache respectively.
- **With persistent cache contents**, the paper's model underestimates mean delay by ~2× at α=0.5 and 64–1700× at α=0.9. See `research/results/retry_correlation.txt` and `build/retry_correlation_check.py`.
- **The guide works on underwater acoustic sensor networks** (Nazareth & Chandavarkar, IJAHUC 2023; WPC 2024). An underwater (ns-3 UAN) scenario is a stretch goal only.
- **Corrections to external review #1:**
  - Its PS2 rule was backwards. Under upstream congestion a cache should answer locally *more*.
  - NDN name matching blocks substitute Data unless designed around.
- **Proposal wording:** it doesn't promise RED/CoDel, because NDN traffic probably bypasses ns-3 traffic control. Verify this in week 1.

## Plan (4 weeks)
| Week | Goal |
|---|---|
| 1 (6–12 Oct) | Docker `ubuntu:20.04` + ndnSIM build, `ndn-simple` runs; scenario template in `build/scenario`; tandem topology with exact caching; answer the three open design questions in CLAUDE.md, writing them into `design_notes.md` |
| 2 (13–19 Oct) | Similarity Content Store + `blind` mode + consumer retry; result 1 (reproduce paper) and result 2 (persistent-contents heavy tail) |
| 3 (20–26 Oct) | `threshold` mode (s_min in Interest + bounded retry), its closed-form model; result 3 |
| 4 (27 Oct–2 Nov) | Bandwidth limits + cross-traffic + Gilbert–Elliott loss, `delay-aware` mode; result 4; report |

## Stage status
| Stage | State | Evidence |
|---|---|---|
| S0 environment | **done** | ndnSIM 2.9 debug build installed, `ndn-simple` runs; template builds and runs `smoke-ndn-simple` (100 Data received) |
| S1 design | **done** | `design_notes.md`: lookup in our strategy; substitute Data under the Interest's own name; no RED/CoDel for NDN (verified: the bundled RED example aborts) |
| S2 reproduce | **done** | 120 runs, 104,000 requests: **24/24 cells within the 95% CI of the analytical value** (`research/results/stage2/analysis.md`, figure `submission/figures/stage2_delay.pdf`). Per-request placement at α=0.9, s=1: **14,522 ± 2,070 ms vs 80.6 ms** in the paper's model (180×, even with the 5000-attempt cap; 22% of requests censored) |
| S3 threshold | **done** | 360 + 960 runs, 1.27 M requests (`research/results/stage3/analysis.md`, `sweep.md`, figures `submission/figures/stage3_*.pdf`). All gates pass. See "S3 results" below |

## S3 results (7 Oct 2026; don't re-derive)
- **Policies.**
  - `threshold`: the Interest carries s_min in ApplicationParameters, and a cache answers only if its best match ≥ s_min.
  - `exclusion`: caches stay blind; a retry names the caches that served rejected answers, and they forward.
  - `blind` replays S2 byte-for-byte: three S2 runs, 2,200 rows identical.
- **Headline** (α=0.9, s=1, contents persist):

  | policy | mean (ms) | p99 (ms) | max attempts | upstream hops | Jain |
  |---|---|---|---|---|---|
  | blind | 14,522 | 51,187 | 5,000 | 158 | 0.48 |
  | exclusion | 32.9 | 80 | 6 | 5.0 | 0.80 |
  | threshold | 4.84 | 10 | 1 | 2.4 | 0.85 |

- **Gate 1** (threshold mean within the CI of the closed form):
  - The 5-run first pass had 23/24 cells inside. The miss was α=0.5, s=1, per-request, t=−3.78. That cell's 5 run means happened to cluster, which shrank its CI; with 24 cells, one miss is likely.
  - Re-test with 25 runs for **every** cell (`stage3_followup.sh`): **24/24 cells, max |t| = 1.61**, t mean +0.05.
- **Gate 2:** threshold mode always takes 1 attempt and at most 10.0077 ms. **Gate 3:** exclusion takes at most 6 attempts, and the per-request bound (caches on the path + 1) holds.
- **α = 1 is delay-optimal** in threshold mode: at s=1, 5.93 → 5.27 → 4.84 → 4.72 ms for α = 0.1 → 1. Under blind caching, α=1 is the worst choice: 89% of requests are censored at s=1.
- **Exact model.** Threshold delay = Eq. (8) with q_i = α·p_i. Because the delay depends only on i.i.d. per-cache "holds an acceptable item" flags, every expectation is exact by enumeration, not Monte Carlo.
  - It agrees with a protocol-level Monte Carlo (worst |z| 2.14 over 24 cells) and with S2's sampled values.
- **Accounting identity** on all 1.27 M requests: delay = 2τ·hops + δ·(attempts−1) + 0.7–1.7 µs/hop of serialisation. Timeouts: 0.
- **Exclusion static cells lean** (mean t +0.84; 22/24 exclusion cells within CI). This is shared cache configurations, not model error: the per-request cells show +0.07. Exclusion isn't gated; mention it in the report.
- **ndnSIM bug found and patched:** the ndn-cxx scheduler leaked ns-3 events, so runs slowed quadratically. The patch changes no output (design_notes, `build/env/ndn-cxx-scheduler-leak.patch`).

## Next action
1. **Progress report, due Thu 22 Oct** (`submission/main.tex`): Abstract, Introduction, Literature review, Proposed approach, following `COURSE_REQUIREMENTS.md` and `stages/S5_report.md`. The team writes it; Claude reviews and checks every number against `research/results/`.
2. **S4:** congestion, Gilbert–Elliott loss and the delay-aware rule. Follow `research/context/stages/S4_congestion_loss.md`.
   - `interest_hops` counts answered attempts only; timed-out attempts need handling once loss exists (design_notes).
   - Commands and pitfalls: `RUNBOOK.md`.
- Report note: for C = 100, s = 0.99 and s = 1.0 give identical results (similarity step = 1/99). Show s = 1.0 only, or say so.

## Log
- 2026-10-05 — repo created; proposal, template, research context and agents pushed.
- 2026-10-05 — moved to the Windows host (Docker Desktop, container `ndnsim`). S0 and S1 done; S2 code written and first validation matches the model.
- 2026-10-05 — S2 sweep done: 24/24 cells match the analysis. Results 1 and 2 reproduced at packet level.
- 2026-10-05 — S3 code: threshold and exclusion policies, `--policy`, `interest_hops`, exact model (`--stage3`). Blind replays S2 exactly.
- 2026-10-07 — Diagnosed and patched the ndn-cxx scheduler event leak; the patch is proven output-neutral.
- 2026-10-07 — S3 sweep and 25-run follow-up done; all gates pass.
- 2026-10-07 — Proposal rewritten to the manual's format; roll numbers recorded; course requirements saved.
