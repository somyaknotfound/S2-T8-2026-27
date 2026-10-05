# Stage 5 — Report and defence material

**Main session model:** Sonnet. Use `citation-checker` (Haiku), then `report-writer` (Opus). **Prerequisite:** S3 done; S4 done or written up honestly as partial.

## Files
- **Report:** `submission/main.tex`. Its layout is cloned from last semester; the title page already says T8, ns-3 and the final title.
- **Figures:** `submission/figures/*.pdf`, produced by `build/analysis/*.py`. Never edit figures by hand.
- **Proposal (already submitted format):** `Proposal_255_257.tex`. Its 6 references are verified.
- **Compile:** Overleaf (upload `submission/`) or `tectonic -X compile main.tex`. On Windows, Tectonic was downloaded to the session scratchpad and may be gone — re-download from GitHub releases if needed.

## Section map (main.tex → source of truth)
| Section | Source |
|---|---|
| 1 Introduction, problem statement, objectives | `Proposal_255_257.tex`, `PROBLEM_STATEMENTS.md` §1 |
| 2 Literature review | `PROBLEM_STATEMENTS.md` §3 (verified entries only) + `external_review_1_ns3.md` §2 |
| 2.x Critical analysis of the base paper | `PROBLEM_STATEMENTS.md` §2.2 (Eq. 6 typo; Eq. 8 vs 10; independent retries / Jensen) |
| 3 Methodology | `design_notes.md` (Q1–Q3, encapsulation, strategy) + `stages/S3`, `stages/S4` designs |
| 3.x Engineering challenges | design_notes: template fixes, the RED example crashing, the optimized-build segfault investigation |
| 4 Results | `research/results/stage2/analysis.md`, `stage3/`, `stage4/` + figures |
| 5 Conclusion / limitations | No AQM for NDN; debug-build-only runs; synthetic similarity; tandem topology |

## Rules
- Every number in the report must trace back to a file in `research/results/`.
- Cite only references marked VERIFIED, by `citation-checker` or in `external_review_1_ns3.md` §2.
- Write in the team's plain voice, matching last semester's report and the proposal. Use Pros/Cons boxes in the literature review, as last semester.
- Say what was not done: RED/CoDel are impossible for NDN in ndnSIM 2.9 (verified); the underwater (UAN) scenario is future work unless it was done.

## Defence pack (one page, last step)
- The headline numbers:
  - S2: 24/24 cells within CI. At α=0.9, real caches give 14.5 s vs 80.6 ms in the model.
  - S3 and S4 results.
- The top 10 hard questions with short answers. Collect them from `PROBLEM_STATEMENTS.md` §6, `stages/S3` and `stages/S4`.
