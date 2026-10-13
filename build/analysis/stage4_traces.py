"""Stage 4 over time: how the delay-aware caches learn as a run unfolds (load sweep).

Run from the repo root after build/experiments/stage4_sweep.sh:
    python build/analysis/stage4_traces.py
Writes research/results/stage4/{trace_delay.csv,trace_nodes.csv,convergence.md} and
submission/figures/stage4_convergence.{pdf,png}. The two CSVs are what build/matlab/trace_replay.m
reads; the raw traces are too large to upload.
"""
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from stage4_summary import (BOTTLENECK_BPS, CROSS_DATA_BYTES, EXCLUSION, FIG, OUT, RAW,  # noqa: E402
                            WARMUP, ci, label, point, read, save)

WINDOW = 100  # requests per point; also the spacing of the per-cache trace rows
REQUESTS = 2000
PHASES = [(501, 1000), (1001, 1500), (1501, 2000)]  # thirds of the measured part of a run


def window_means():
    """(load, policy) -> run -> window index -> [mean delay, rejections per request]."""
    acc = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))
    for row in read(RAW / "load" / "req.csv"):
        window = (int(row["request"]) - 1) // WINDOW
        rejected = int(row["attempts"]) - 1 - int(row["timeouts"])
        acc[(point("load", row), label(row))][int(row["run"])][window].append(
            (float(row["delay_ms"]), rejected))
    return {cell: {run: {w: np.mean(v, axis=0) for w, v in windows.items()}
                   for run, windows in runs.items()} for cell, runs in acc.items()}


def node_traces():
    """(load, request, position) -> per-run [upstream delay, learned threshold]."""
    acc = defaultdict(list)
    for path in sorted((RAW / "load" / "nodes").glob("delay-aware_x*_r*.csv")):
        rate = float(path.stem.split("_x")[1].split("_r")[0])
        x = round(rate * CROSS_DATA_BYTES * 8 / BOTTLENECK_BPS, 2)
        for row in read(path):
            acc[(x, int(row["request"]), int(row["position"]))].append(
                (float(row["upstream_ms"]), float(row["threshold_own"])))
    return acc


def phase_mean(windows, phase):
    return np.mean([windows[w][0] for w in range((phase[0] - 1) // WINDOW, phase[1] // WINDOW)])


def best_fixed(delays, x):
    """The fixed-alpha exclusion policy with the lowest mean delay after warm-up at load x."""
    after = range(WARMUP // WINDOW, REQUESTS // WINDOW)
    return min(EXCLUSION, key=lambda p: np.mean([[runs[w][0] for w in after]
                                                 for runs in delays[(x, p)].values()]))


def write_traces(delays, nodes):
    """Per-run rows, so one run can be replayed as it unfolded."""
    with (OUT / "trace_delay.csv").open("w", newline="", encoding="utf-8") as f:
        f.write("offered_load,policy,run,request,mean_ms,rejections_per_req\n")
        for (x, policy), runs in sorted(delays.items()):
            for run in sorted(runs):
                for w in range(REQUESTS // WINDOW):
                    mean, rejections = runs[run][w]
                    f.write(f"{x},{policy},{run},{(w + 1) * WINDOW},{mean:.4f},{rejections:.4f}\n")
    with (OUT / "trace_nodes.csv").open("w", newline="", encoding="utf-8") as f:
        f.write("offered_load,run,request,position,upstream_ms,threshold_own\n")
        for (x, request, position), values in sorted(nodes.items()):
            # node_traces reads the per-run files in run order
            for run, (up, s) in enumerate(values, start=1):
                f.write(f"{x},{run},{request},{position},{up:.3f},{s:.2f}\n")


def write_convergence(delays, nodes):
    loads = sorted({x for x, _ in delays})
    lines = ["# Stage 4 — does the delay-aware rule settle after warm-up?", "",
             f"Load sweep, mixed users, 5 runs per point. Each run has {REQUESTS} requests; the "
             f"first {WARMUP} are warm-up. The measured part is split into thirds. Δ is the last "
             "third minus the first, paired over runs (95% CI). A learner still improving would "
             "show a negative Δ whose CI excludes 0; the oracle (s_min in the Interest) does not "
             "learn, so its Δ shows the run-to-run noise.", "",
             "## Mean delay by third of the run (ms)", "",
             "| offered load | policy | requests 501–1000 | 1001–1500 | 1501–2000 | Δ last − first "
             "| settled |", "|---|---|---|---|---|---|---|"]
    settled = {}
    for x in loads:
        for policy in ("delay-aware", "threshold-a1"):
            runs = delays[(x, policy)]
            phases = np.array([[phase_mean(runs[r], p) for p in PHASES] for r in sorted(runs)])
            diff, half = ci(phases[:, 2] - phases[:, 0])
            ok = abs(diff) <= half
            if policy == "delay-aware":
                settled[x] = ok
            means = " | ".join(f"{m:.2f}" for m in phases.mean(axis=0))
            lines.append(f"| {x:g} | {policy} | {means} | {diff:+.2f} ± {half:.2f} | "
                         f"{'yes' if ok else '**no**'} |")
    lines += ["", f"Delay-aware: Δ's CI includes 0 at {sum(settled.values())} of {len(settled)} "
              "loads.", "",
              "## Learned state at request 1000 and at request 2000 (mean over runs)", "",
              "| offered load | cache | D_up at 1000 (ms) | D_up at 2000 (ms) | s* at 1000 | "
              "s* at 2000 | change in s* |", "|---|---|---|---|---|---|---|"]
    for x in loads:
        for position in range(5):
            mid = np.mean(nodes[(x, 1000, position)], axis=0)
            end = np.mean(nodes[(x, REQUESTS, position)], axis=0)
            # the two snapshots come from the same runs, so the change is paired
            change, half = ci([e[1] - m[1] for m, e in zip(nodes[(x, 1000, position)],
                                                          nodes[(x, REQUESTS, position)])])
            lines.append(f"| {x:g} | v{position + 1} | {mid[0]:.2f} | {end[0]:.2f} | {mid[1]:.3f} "
                         f"| {end[1]:.3f} | {change:+.3f} ± {half:.3f} |")
    lines += ["", "## Snapshots with s* below 0.5 after warm-up (the cache answers whatever it holds)",
              "",
              "A cache's s* is logged every 100 requests. Each cell: share of the snapshots after "
              "warm-up, over all runs, with s* < 0.5, then the mean D_up in those snapshots and in "
              "the others (ms).", "",
              "| offered load | " + " | ".join(f"v{p + 1}" for p in range(5)) + " |",
              "|---|---|---|---|---|---|"]
    for x in loads:
        cells = []
        for position in range(5):
            shots = [v for r in range(WARMUP + WINDOW, REQUESTS + 1, WINDOW)
                     for v in nodes[(x, r, position)]]
            low = [up for up, s in shots if s < 0.5]
            high = [up for up, s in shots if s >= 0.5]
            cells.append(f"{100 * len(low) / len(shots):.0f}% ({np.mean(low):.0f} / "
                         f"{np.mean(high):.0f})" if low else "0%")
        lines.append(f"| {x:g} | " + " | ".join(cells) + " |")
    (OUT / "convergence.md").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print("\n".join(lines))


def plot(delays, nodes, loads=(0.0, 0.9, 0.95)):
    fig, axes = plt.subplots(2, len(loads), figsize=(13, 6.4), sharex=True)
    requests = np.arange(1, REQUESTS // WINDOW + 1) * WINDOW
    colors = plt.cm.viridis(np.linspace(0, 0.9, 5))
    for col, x in enumerate(loads):
        ax = axes[0][col]
        shown = [("delay-aware", "delay-aware (ours)", "tab:green", "-", 2.2),
                 ("threshold-a1", "oracle: s_min in Interest", "black", "--", 1.5),
                 (best_fixed(delays, x), None, "tab:orange", "-", 1.5)]
        for policy, name, color, ls, lw in shown:
            runs = delays[(x, policy)]
            mean, half = zip(*(ci([runs[r][w][0] for r in sorted(runs)])
                               for w in range(REQUESTS // WINDOW)))
            mean, half = np.array(mean), np.array(half)
            ax.plot(requests, mean, color=color, ls=ls, lw=lw,
                    label=name or f"best fixed α: exclusion α = {policy.split('-a')[1]}")
            ax.fill_between(requests, mean - half, mean + half, color=color, alpha=0.15, lw=0)
        ax.set_ylabel(f"Mean delay per {WINDOW} requests (ms)")
        ax.set_title(f"Offered backhaul load {x:g}", fontsize=10)
        ax.legend(fontsize=7)
        ax = axes[1][col]
        for position, color in enumerate(colors):
            ax.plot(requests, [np.mean(nodes[(x, r, position)], axis=0)[1] for r in requests],
                    "o-", ms=3, color=color, label=f"v{position + 1}")
        ax.set_ylabel("Mean learned threshold s* for its own client")
        ax.set_xlabel("Requests completed in the run")
        if col == 0:
            ax.legend(fontsize=7, title="cache", title_fontsize=7, loc="lower left")
    for ax in axes.flat:
        ax.axvspan(0, WARMUP, color="0.9", lw=0)
        ax.grid(True, alpha=0.3)
    fig.suptitle("Delay-aware caches over a run (grey = warm-up, excluded from the results)",
                 fontsize=10)
    save(fig, "stage4_convergence")


def main():
    delays, nodes = window_means(), node_traces()
    write_traces(delays, nodes)
    write_convergence(delays, nodes)
    FIG.mkdir(parents=True, exist_ok=True)
    plot(delays, nodes)


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8", newline="\n")
    main()
