# Status — read at the start of every session, update at the end

## Decided (5 Oct 2026)
- **Team T8:** Somyak Priyadarshi Mohanta (241CS257), Pranav Shaji, Chris Tony. Pranav's and Chris's roll numbers aren't recorded yet.
- **Topic:** *Threshold-Aware Similarity Caching for Named Data Networks: Bounding Retry Delay under Congestion using ns-3.*
  Spreadsheet title: *Threshold-Aware Similarity Caching with Bounded Retries for Named Data Networks.*
- **Tool:** ns-3 + ndnSIM; Python/MATLAB for analysis. The course only allows UnetStack, ns-3 or MATLAB; NeST was dropped on 5 Oct.
- **Scope:** PS1 (threshold-carrying Interests + bounded retry) and PS2 (congestion/loss-aware answer-or-forward). See `PROBLEM_STATEMENTS.md` §6 and `external_review_1_ns3.md`.
- **Deadline:** about one month from 5 Oct 2026.
- **Proposal:** `Proposal_255_257.tex`/`.pdf` at the repo root, in last semester's format. Its references are verified.

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

## Next action
**Start S3 by following `research/context/stages/S3_threshold.md`** (code changes, model, grid, gate). Commands and pitfalls: `research/context/RUNBOOK.md`. Later stages: `stages/S4_congestion_loss.md`, then `stages/S5_report.md`.
- S3, threshold mode:
  - The Interest carries s_min (ApplicationParameters).
  - A cache answers only if its best match ≥ s_min; the consumer's retries are bounded with an exclusion list.
  - Add the closed-form threshold-mode delay to `build/retry_correlation_check.py`.
  - Expected: the heavy tail disappears and α no longer matters.
- Report note: for C = 100, s = 0.99 and s = 1.0 give identical results (similarity step = 1/99). Show s = 1.0 only, or say so.

## Log
- 2026-10-05 — repo created; proposal, template, research context and agents pushed.
- 2026-10-05 — moved to the Windows host (Docker Desktop, container `ndnsim`). S0 and S1 done; S2 code written and first validation matches the model.
- 2026-10-05 — S2 sweep done: 24/24 cells match the analysis. Results 1 and 2 reproduced at packet level.
