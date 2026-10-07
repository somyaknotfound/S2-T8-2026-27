# Stage 5 — Report and defence material

**Main session model:** Sonnet. Use `citation-checker` (Haiku), then `report-writer` (Opus). **Prerequisite:** S3 done; S4 done or written up honestly as partial.

## Files
- **Report:** `submission/main.tex`. Its layout is cloned from last semester; the title page already says T8, ns-3 and the final title.
- **Figures:** `submission/figures/*.pdf`, produced by `build/analysis/*.py`. Never edit figures by hand.
- **Proposal (already submitted format):** `Proposal_222_243_257.tex`. Its 6 references are verified.
- **Compile:** Overleaf (upload `submission/`) or `tectonic -X compile main.tex`. On Windows, Tectonic was downloaded to the session scratchpad and may be gone — re-download from GitHub releases if needed.

## Section map (main.tex → source of truth)
**The structure is fixed by the course manual** (`research/context/COURSE_REQUIREMENTS.md`):
- Progress report, due 22 Oct: Abstract, Introduction, Literature review, Proposed approach.
- Final report, due 2 Nov: adds Implementation, Results and analysis, and Conclusions.
- Page limits and line counts are in that file.

| Section | Source |
|---|---|
| Abstract | the updated `Proposal_222_243_257.tex` paragraph |
| 1 Introduction: background / challenges / existing approaches / problem statement / objectives / organisation (`\ref`, no hard-coded numbers) | `Proposal_222_243_257.tex`, `PROBLEM_STATEMENTS.md` §1 |
| 2 Literature review: chronological, one subsection per approach with pros/cons and our own figures; last subsection compares them | `PROBLEM_STATEMENTS.md` §3 (verified entries only) + `external_review_1_ns3.md` §2; base-paper critique from §2.2 (Eq. 6 typo; Eq. 8 vs 10; independent retries / Jensen) |
| 3 Proposed approach: diagram or algorithm plus justification | `stages/S3`, `stages/S4` designs; S3 closed forms |
| 4 Implementation (final): class diagram + pseudocode | `design_notes.md` (Q1–Q3, encapsulation, strategy, the scheduler patch); `build/scenario/extensions/` |
| 5 Results and analysis (final): parameters, scenarios, metrics, graphs, **comparison with the state of the art** | `research/results/stage2/`, `stage3/`, `stage4/` + figures |
| 6 Conclusions and future work (≤ 250 words) | No AQM for NDN; debug-build-only runs; synthetic similarity; tandem topology |

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
