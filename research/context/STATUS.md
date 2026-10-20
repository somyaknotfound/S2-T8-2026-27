# Status — read at the start of every session, update at the end

## Decided (5 Oct 2026)
- **Team T8:** Somyak Priyadarshi Mohanta (241CS257), Pranav Shaji (241CS243), Chris Tony (241CS222).
- **Topic:** *Threshold-Aware Similarity Caching for Named Data Networks: Bounding Retry Delay under Congestion using ns-3.*
  Spreadsheet title: *Threshold-Aware Similarity Caching with Bounded Retries for Named Data Networks.*
- **Tool:** ns-3 + ndnSIM; Python/MATLAB for analysis. The course only allows UnetStack, ns-3 or MATLAB; NeST was dropped on 5 Oct.
- **MATLAB is part of the stack** (the team sheet lists "NS3/MATLAB/ndnSIM"). Somyak expects to need it later for plots and for simulating or replaying real-time data. Planned uses (1 and 2 written on 8 Oct, not run yet; 3 not started):
  1. Report figures redrawn from the committed summary CSVs (`research/results/stage*/summary.csv`, `stage4/nodes.csv`). They are plain LF CSVs, so `readtable` works directly.
  2. The closed-form models (S3 Eq. 8 threshold delay, exclusion recursion) as a MATLAB cross-check of `build/retry_correlation_check.py`.
  3. Time-series replay of the per-request and per-node traces (delay, D_up and s* as the run unfolds). Raw traces are gitignored, so regenerate them locally with the sweep scripts first.
  - **Workflow: MATLAB Online** (not installed on the Windows host; this is how Somyak used it last time).
    - Claude writes self-contained `.m` scripts in `build/matlab/`, each reading only CSVs that Somyak uploads to MATLAB Drive next to it. No repo paths, no toolboxes beyond base MATLAB unless he confirms access.
    - He runs them online and pastes back the output or figures.
    - Exported figures go to `submission/figures/` as before.
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
| S4 congestion/loss | **done** | 485 runs, 970 k requests (`research/results/stage4/analysis.md`, `sweep.md`, figures `submission/figures/stage4_*.pdf`). Gate passes at every load, loss and fixed-user point. See "S4 results" below |

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

## S4 results (8 Oct 2026; don't re-derive)
- **Scenario:** a 2 Mbps backhaul (v5–origin) with Poisson cross traffic at offered load 0–0.95, or Gilbert–Elliott loss of 1–10% on every link. Users are mixed (s ∈ {0.9, 0.95, 1.0}). 2000 requests per run, 500 of them warm-up. Design: `stages/S4_congestion_loss.md`; implementation: design_notes.
- **Gate passes everywhere.** Delay-aware beats the best fixed-α exclusion by a paired Δ whose CI excludes 0 at every point:
  - load: −2.8 ms at 0 load, rising to −7.0 ms at 0.95;
  - loss: −5.8 / −10.9 / −20.8 ms at 1 / 5 / 10%.
  - It recovers 43–50% of the gap to the oracle under load, and 28–50% under loss, without ever seeing s_min.
- **No fixed α is right everywhere:** the best fixed α is 0.1 up to 0.9 load and 1.0 at 0.95; under loss it shifts to 0.9 and then 0.7.
- **Fixed users (s = 0.95):** delay-aware ≈ oracle. Idle it recovers 100% of the gap (1.16 vs 1.17 ms); at 0.9 load, 97%.
- **Learned state:**
  - D_up rises with load at every cache.
  - s* falls with load (v1: 0.97 → 0.88).
  - At low load, caches near the origin are strictest (s* ≈ 1).
  - **At 0.95 load this reverses:** v5 has the lowest *mean* s* (0.77), because all of its forwards cross the congested backhaul (D_up 23.5 ms vs 11.6 ms at v1). The plan predicted only the first part; report both.
  - **That mean is a mixture, not a settled value (found 12 Oct; `stage4/convergence.md`).** s* flips between about 0.95 and 0 when D_up spikes: in 20% of v5's post-warm-up snapshots at 0.95 load, 5–8% at the other caches, at most 8% at 0.9 load, never below. Never quote 0.77 as "the threshold v5 learned".
- **Convergence (12 Oct):**
  - Delay-aware delay has no significant drift after warm-up at any load (last third − first third, paired CI includes 0 at 6/6 loads).
  - Up to 0.9 load, no cache's s* moves more than 0.03 between requests 1000 and 2000.
  - At 0.95 load the network is not in steady state: the oracle's delay drifts +2.5 ± 2.5 ms within a run.
- **Known weakness (12 Oct):** bins below 0.90 are rarely observed, so the isotonic fit pools them with the [0.90, 0.95) bin. When D_up is high the cache therefore answers whatever it holds, including items no user accepts. Not fixed; a fix means re-running S4.
- **Externality:** cross-traffic delay differs by ≤ 5.5 ms between policies even at 0.95 load. Delay-aware is lowest there (50.9 ms vs 56.4 ms for the oracle, which forwards more).
- **Learning cost:** the learner's rejections per request rise with load, 0.13 → 0.50, because answering more is the point. The oracle has 0.

## Next action
1. **Progress report, due Thu 22 Oct** (`submission/main.tex`): **drafted on 8 Oct; the team reviews it.**
   - Contents: Abstract, Introduction (6 subsections, `\ref`-only organisation paragraph), Literature review (8 approaches 2008–2025 in date order, pros/cons, own TikZ timeline, comparison table), Proposed approach (TikZ system figure, the four rules with Eqs., Algorithm 1, justification).
   - Tectonic build: intro 2.00 pages, lit review 3.00, approach 2.00 (limits 2/3/2). No undefined refs; 19 references, all cited, in first-citation order.
   - Every number has a `%` source comment. All 19 references were verified against Crossref/arXiv on 8 Oct.
   - Stage 2 is reported as "all 18 distinct configurations", not 24/24: the 24 cells count s = 0.99 and s = 1.0 twice.
2. **MATLAB (MATLAB Online): all three scripts ran on 20 Oct.**
   - **How to run:** put the `stage2`, `stage3` and `stage4` result folders (only those, not `raw/`) next to the scripts in MATLAB Drive. `find_csv` in each script also accepts renamed copies (`stage2_summary.csv`, …).
   - **`delay_models`:** All checks PASS (`research/results/matlab/model_check.txt`).
     - The exact ratios are 2.13× and 1705.73×, as in Python.
     - Stage 3 expectations agree with Python to about 1e-11 relative.
     - ns-3 within its CI: 12/12 + 12/12 (Stage 2) and 24/24 blind, 24/24 threshold, 22/24 exclusion (Stage 3).
   - **`trace_replay`:** its s* < 0.5 table is identical to `stage4/convergence.md`.
   - **Figures:** `submission/figures/matlab/` holds all 9 MATLAB figures and `stage4_replay.gif`.
     - The final report uses `\graphicspath{{figures/matlab/}{figures/}}`, so the Python figures are now only a fallback.
     - Two figures were redrawn on 20 Oct after legend fixes. `stage3_threshold_alpha`'s legend had covered the α = 0.9 and 1 points; `stage4_convergence`'s had covered the best-fixed-α line.
3. **S5 (final report, 2 Nov):** **drafted on 8 Oct; the team reviews it.** Compile `submission/final.tex`.
   - It defines `\FinalReport` and inputs `main.tex`, whose final-only parts sit in `\ifdefined\FinalReport` blocks. `main.tex` alone still builds the progress report unchanged (2/3/2 pages).
   - Final-only additions:
     - §3.5 Baselines;
     - §4 Implementation: class diagram, packet-field table, Algorithms 2–3, engineering challenges;
     - §5 Results: parameters table, Stages 2–4 with the 8 figures, the Stage 3 and Stage 4 tables, and a state-of-the-art comparison;
     - §6 Conclusions (215 words, limit 250).
   - Measured lengths (12 Oct): intro 2.08, lit review 3.05, approach 2.22, implementation 3.73, results 6.13 pages; 23 pages in all; conclusions 228 words. All minimums met. No overfull boxes, no undefined references.
   - §5.4 now has a "Does the learned state settle?" paragraph and `stage4_convergence` (Fig. 8), from `stage4/convergence.md`.
   - Since 20 Oct the figures are the MATLAB versions (`submission/figures/matlab/`). Rebuilt on 20 Oct: 23 pages, no overfull boxes, no undefined references.
4. **Defence pack:** `research/context/defence_pack.md` holds the headline numbers and the 10 hardest viva questions with answers.
5. **Explainer:** `research/context/project_explainer.html` (open it in a browser) covers the project for the progress assessment: problem, KPIs, how it differs from the base paper, the simulation set-up, every result and the limits. The same page is a private artifact (https://claude.ai/artifact/KTCDzSxAodub4wgCHniGSd), but it opens only for the claude.ai account that published it, not the NITK one.
6. **Open decisions:**
   - **Learner fix:** stop pooling unobserved low-similarity bins (design_notes, "Known weakness"). It means re-running S4 and updating every S4 number in both reports, so not before the 22 Oct progress assessment.
   - **Team number:** the submitted proposal's header says S-2-T-7; everything else says T8. Confirm which is right.
   - **Progress report, due 22 Oct:** compile `main.tex` and upload it to Overleaf or submit it.
- Report note: for C = 100, s = 0.99 and s = 1.0 give identical results (similarity step = 1/99). Show s = 1.0 only, or say so.
- Report note: S4 runs one topology and a synthetic catalogue. Name these limitations in the conclusions.

## Log
- 2026-10-05 — repo created; proposal, template, research context and agents pushed.
- 2026-10-05 — moved to the Windows host (Docker Desktop, container `ndnsim`). S0 and S1 done; S2 code written and first validation matches the model.
- 2026-10-05 — S2 sweep done: 24/24 cells match the analysis. Results 1 and 2 reproduced at packet level.
- 2026-10-05 — S3 code: threshold and exclusion policies, `--policy`, `interest_hops`, exact model (`--stage3`). Blind replays S2 exactly.
- 2026-10-07 — Diagnosed and patched the ndn-cxx scheduler event leak; the patch is proven output-neutral.
- 2026-10-07 — S3 sweep and 25-run follow-up done; all gates pass.
- 2026-10-07 — Proposal rewritten to the manual's format; roll numbers recorded; course requirements saved.
- 2026-10-07 — S3 committed (`29b075a`).
- 2026-10-07 — S4 built: Gilbert–Elliott model, backhaul, cross traffic, mixed users, and the delay-aware policy.
  - Found that ndnSIM's NFD never calls `afterReceiveData`.
  - Added isotonic F and confidence-based exploration.
  - Sweep: 485 runs, all gates pass.
- 2026-10-08 — Progress report drafted in `submission/main.tex` within the page limits, with 19 verified references.
- 2026-10-08 — MATLAB scripts written (`build/matlab/`); they are waiting for a MATLAB Online run.
- 2026-10-08 — Committed `8edbc4f`. Final report drafted (`submission/final.tex`) and defence pack written.
- 2026-10-09 — Proposal defence. The submitted proposal is `submission/proposal_257_222_243.pdf`; it is kept out of git because the repo is public and the PDF has phone numbers.
- 2026-10-12 — Pushed `244ae85`. Trace analysis added (`build/analysis/stage4_traces.py`): convergence checked, the bimodal s* at high load found, and the final report, defence pack and design notes corrected.
- 2026-10-13 — Pushed `f166190` (the Stage 4 trace analysis).
- 2026-10-20 — Pushed `40e4ffe`: the submitted proposal with contact details redacted. Personal files in the folder (grade cards, roster CSV, the unredacted proposal) are excluded through `.git/info/exclude`.
- 2026-10-20 — All three MATLAB scripts ran in MATLAB Online. Model check: all PASS (`results/matlab/model_check.txt`). The scripts now find the result folders by themselves (`find_csv`). The final report switched to the 9 MATLAB figures; two of them were redrawn after legend fixes. Wrote the standalone project explainer.
