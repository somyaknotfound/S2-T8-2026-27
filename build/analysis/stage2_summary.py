"""Stage 2: compare simulated similar-content delivery delay with the analytical expectations.

Run from the repo root after build/experiments/stage2_sweep.sh:
    python build/analysis/stage2_summary.py
Writes research/results/stage2/{summary.csv,analysis.md} and submission/figures/stage2_delay.{pdf,png}.
"""
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

RAW = Path("research/results/raw/stage2/all.csv")
EXPECTED = Path("research/results/retry_correlation.txt")
OUT = Path("research/results/stage2")
FIG = Path("submission/figures")
T95 = {2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571, 6: 2.447, 9: 2.262}  # two-sided t, by dof
COLORS = {0.1: "tab:blue", 0.5: "tab:orange", 0.9: "tab:red"}


def load_expected():
    """(alpha, s) -> {'model_cap': .., 'persist_cap': ..} from retry_correlation.txt."""
    expected = {}
    for line in EXPECTED.read_text().splitlines()[1:]:
        cols = line.replace("|", " ").split()
        alpha, s = float(cols[0]), float(cols[1])
        expected[(alpha, s)] = {"model_cap": float(cols[8]), "persist_cap": float(cols[9])}
    return expected


def load_runs():
    cells = defaultdict(lambda: defaultdict(list))
    with RAW.open(encoding="utf-8") as f:
        for row in csv.DictReader(f):
            key = (float(row["alpha"]), float(row["threshold"]), row["placement"])
            cells[key][int(row["run"])].append(row)
    return cells


def summarise(cells, expected):
    summary = []
    for (alpha, s, placement), runs in sorted(cells.items()):
        run_means = np.array([np.mean([float(r["delay_ms"]) for r in rows])
                              for rows in runs.values()])
        rows = [r for rows in runs.values() for r in rows]
        delays = np.array([float(r["delay_ms"]) for r in rows])
        k = len(run_means)
        half = T95[k - 1] * run_means.std(ddof=1) / np.sqrt(k) if k > 1 else float("nan")
        mean = run_means.mean()
        ref = expected[(alpha, s)]["model_cap" if placement == "per-attempt" else "persist_cap"]
        summary.append({
            "alpha": alpha, "threshold": s, "placement": placement, "runs": k,
            "requests": len(rows), "mean_ms": mean, "ci95_ms": half,
            "median_ms": np.median(delays), "p95_ms": np.percentile(delays, 95),
            "p99_ms": np.percentile(delays, 99),
            "mean_attempts": np.mean([int(r["attempts"]) for r in rows]),
            "censored_share": np.mean([int(r["censored"]) for r in rows]),
            "expected_ms": ref, "within_ci": abs(mean - ref) <= half,
        })
    return summary


def write_tables(summary):
    OUT.mkdir(parents=True, exist_ok=True)
    with (OUT / "summary.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(summary[0]))
        writer.writeheader()
        writer.writerows(summary)

    lines = ["# Stage 2 results — simulated vs. expected delay", "",
             "Mean ± 95% CI over runs (run-level t interval). Expected = capped analytical value "
             "(`model cap` for per-attempt, `persist cap` for per-request).", "",
             "| α | s | placement | mean ± CI (ms) | expected (ms) | within CI | median | p99 | "
             "censored |", "|---|---|---|---|---|---|---|---|---|"]
    for r in summary:
        lines.append(
            f"| {r['alpha']} | {r['threshold']} | {r['placement']} | "
            f"{r['mean_ms']:.2f} ± {r['ci95_ms']:.2f} | {r['expected_ms']:.2f} | "
            f"{'yes' if r['within_ci'] else '**no**'} | {r['median_ms']:.1f} | "
            f"{r['p99_ms']:.1f} | {r['censored_share']:.3f} |")
    hits = sum(r["within_ci"] for r in summary)
    lines += ["", f"{hits} of {len(summary)} cells within the 95% CI of the expected value."]
    (OUT / "analysis.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))


def plot(summary):
    FIG.mkdir(parents=True, exist_ok=True)
    fig, axes = plt.subplots(1, 2, figsize=(10, 4), sharey=True)
    titles = {"per-attempt": "Contents redrawn every attempt (paper's model)",
              "per-request": "Contents persist across retries (real caches)"}
    for ax, placement in zip(axes, ["per-attempt", "per-request"]):
        for alpha in sorted(COLORS):
            rows = sorted((r for r in summary
                           if r["placement"] == placement and r["alpha"] == alpha),
                          key=lambda r: r["threshold"])
            x = [r["threshold"] for r in rows]
            ax.errorbar(x, [r["mean_ms"] for r in rows], yerr=[r["ci95_ms"] for r in rows],
                        fmt="o", color=COLORS[alpha], capsize=3, label=f"α = {alpha} (ns-3)")
            ax.plot(x, [r["expected_ms"] for r in rows], "--", color=COLORS[alpha], alpha=0.7)
        ax.set_yscale("log")
        ax.set_xlabel("Required similarity s")
        ax.set_title(titles[placement], fontsize=10)
        ax.grid(True, which="both", alpha=0.3)
    axes[0].set_ylabel("Mean similar-content delivery delay (ms)")
    axes[0].legend(fontsize=8, title="dashed = analytical", title_fontsize=8)
    fig.tight_layout()
    for ext in ("pdf", "png"):
        fig.savefig(FIG / f"stage2_delay.{ext}", dpi=200)


def main():
    summary = summarise(load_runs(), load_expected())
    write_tables(summary)
    plot(summary)


if __name__ == "__main__":
    main()
