# Course requirements — CS301 mini-project and CS302 lab (from the course manual, 2026-27)

Source: the manual's text, pasted by Somyak on 7 Oct 2026. Team T8: Somyak Priyadarshi Mohanta (241CS257), Pranav Shaji (241CS243), Chris Tony (241CS222). Tool listed for T8: NS3/MATLAB/ndnSIM. Guide: Prof. B. R. Chandavarkar.

## Deadlines and weights (CS301)
| Report | Weight | Due |
|---|---|---|
| Proposal | 5% | **Thu 8 Oct 2026** |
| Progress | 5% | **Thu 22 Oct 2026** |
| Final | 10% | **Mon 2 Nov 2026** (the manual prints "Thursday") |

Teams are 3 students from the same section. All reports are written in Overleaf.

## Proposal (`Proposal_222_243_257.tex`)
One paragraph covering, in order:
- background (4–5 lines);
- challenges and issues (4–5 lines);
- existing approaches and their issues (4–5 lines);
- our problem statement (2 lines);
- 2–3 objectives.

References must be in a proper format; we use IEEE.

## Progress report (`submission/main.tex`)
1. **Abstract:** the updated proposal.
2. **Introduction** (max. 2 pages). One paragraph or subsection each:
   - background (15–20 lines);
   - challenges and issues (15–20 lines);
   - existing approaches and their issues (10–15 lines);
   - problem statement (5 lines);
   - 2–3 objectives.
   - Every paragraph except the problem statement and objectives needs references.
   - The **last paragraph is the organisation of the rest of the report.** Use `\ref`; never hard-code section numbers.
3. **Literature review** (max. 3 pages):
   - Existing work in chronological order, one subsection per approach, with its pros and cons.
   - Our own flowcharts, figures and algorithms only: never copy figures, results or algorithms from papers.
   - The last subsection compares the approaches.
4. **Proposed approach** (max. 2 pages): our model for the gaps found, with a neat diagram or algorithm and the justification.

## Final report
Abstract; Introduction (≥ 2 pages); Literature review (≥ 3 pages); Proposed approach (≥ 2 pages). The first four are the updated progress-report content. Then:
- **Implementation** (≥ 2 pages): a class diagram and pseudocode.
- **Results and analysis** (≥ 2 pages):
  - simulation parameters, scenarios and metrics;
  - numerical results and graphs;
  - **a comparison with the state of the art**.
- **Conclusions and future work** (≤ 250 words): purpose, then achievements.

## Integrity rules (verbatim intent)
- Plagiarism must be < 10% for every report.
- "Using AI to generate any of the reports/code results in 0 marks for the mini-project and also for all the labs after the mid-semester exams."

## CS302 lab (relevant to this project)
- Lab plan:
  - 5/6 Oct: tool demo (UnetStack, NS3, MATLAB).
  - 12/13 Oct: UnetStack exercise.
  - 19 Oct: MATLAB exercise.
  - 26/27 Oct: NS3 exercise.
  - 2/3 Nov: end-sem.
- Option 1 applies: the mini-project is implemented in ns-3, so it also counts as the lab's problem-solving activity.
- Remaining lab evaluation: quiz 5%, basic exercises 10%, problem-solving code progress 5%.
- **Lab end-sem** (3 h, 40 marks):
  - Q1 is one of Packet Tracer, socket programming or NeST.
  - Q2 is one of UnetStack, NS3 or MATLAB.
  - Marking: write-up/concept 5+5, neat commented code 5+5, execution 8+8, viva 4.
