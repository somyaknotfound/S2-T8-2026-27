# Threshold-Aware Similarity Caching for Named Data Networks

CS301 Computer Networks mini-project, NITK Surathkal (2026-27) — Team T8.
Guide: Prof. B. R. Chandavarkar.

Somyak Priyadarshi Mohanta (241CS257), Pranav Shaji, Chris Tony

Base paper: R. Nakamura and N. Kamiyama, "Analysis of Similarity Caching on General Cache Networks,"
IEEE Access, vol. 12, pp. 163338-163348, 2024.

## Layout

| Folder | Contents |
|---|---|
| `build/` | All code: ndnSIM scenarios (`build/scenario/`), experiment drivers, analysis scripts |
| `submission/` | LaTeX for submission (`main.tex` = progress report) |
| `research/documents/` | Base paper, last semester's report and proposal |
| `research/context/` | Problem statements, reviews, design notes, `STATUS.md` (current state) |
| `research/results/` | Experiment outputs |
| `Proposal_255_257.tex` | Project proposal |

## Getting started

ndnSIM needs Ubuntu 20.04, so it runs in Docker:

```bash
git clone https://github.com/somyaknotfound/S2-T8-2026-27.git && cd S2-T8-2026-27
docker run -dit --name ndnsim -v "$PWD":/work -w /work ubuntu:20.04 bash
claude        # Claude Code picks up .claude/CLAUDE.md and the agents in .claude/agents/
```

Then follow `research/context/STATUS.md`.
