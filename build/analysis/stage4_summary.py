"""Stage 4: delay-aware caching vs fixed-alpha caching under backhaul congestion and burst loss.

Run from the repo root after build/experiments/stage4_sweep.sh:
    python build/analysis/stage4_summary.py
Writes research/results/stage4/{summary.csv,nodes.csv,analysis.md} and
submission/figures/stage4_*.{pdf,png}.
"""
import csv
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from stage3_summary import jain, t_quantile  # noqa: E402

RAW = Path("research/results/raw/stage4")
OUT = Path("research/results/stage4")
FIG = Path("submission/figures")
WARMUP = 500             # requests per run excluded from every metric (delay-aware is learning)
CROSS_DATA_BYTES = 1075  # cross-traffic Data on the wire (calibrated, RUNBOOK)
BOTTLENECK_BPS = 2e6
P_BG = 0.3
SCENARIOS = {"load": "Offered cross-traffic load on the backhaul",
             "loss": "Mean loss per link (%)",
             "fixed": "Offered cross-traffic load on the backhaul"}
EXCLUSION = [f"exclusion-a{a}" for a in ("0.1", "0.3", "0.5", "0.7", "0.9", "1.0")]
STYLE = {"delay-aware": ("tab:green", "o", "-", 2.2), "threshold-a1": ("black", "s", "--", 1.5),
         "blind-a0.5": ("tab:red", "^", ":", 1.5)}
NAMES = {"delay-aware": "delay-aware (ours)", "threshold-a1": "oracle: s_min in Interest",
         "blind-a0.5": "blind α = 0.5 (paper)"}
EXCL_COLORS = plt.cm.Oranges(np.linspace(0.35, 0.95, len(EXCLUSION)))


def label(row):
    policy = row["policy"]
    if policy == "delay-aware":
        return policy
    return f"{policy}-a{float(row['alpha']):g}" if policy != "exclusion" else \
        f"exclusion-a{float(row['alpha']):.1f}"


def point(scenario, row):
    if scenario == "loss":
        pgb = float(row["loss_pgb"])
        return round(100 * pgb / (pgb + P_BG), 1)
    return round(float(row["cross_rate"]) * CROSS_DATA_BYTES * 8 / BOTTLENECK_BPS, 2)


def read(path):
    with path.open(encoding="utf-8") as f:
        return list(csv.DictReader(f))


def load_cells(scenario):
    cells = defaultdict(lambda: defaultdict(list))
    for row in read(RAW / scenario / "req.csv"):
        if int(row["request"]) > WARMUP:
            cells[(point(scenario, row), label(row))][int(row["run"])].append(row)
    runs = defaultdict(dict)
    for row in read(RAW / scenario / "run.csv"):
        runs[(point(scenario, row), label(row))][int(row["run"])] = row
    return cells, runs


def ci(values):
    values = np.asarray(values, dtype=float)
    k = len(values)
    return values.mean(), t_quantile(0.975, k - 1) * values.std(ddof=1) / np.sqrt(k)


def summarise(scenario, cells, runs):
    summary = []
    for (x, policy), by_run in sorted(cells.items()):
        rows = [r for rs in by_run.values() for r in rs]
        delays = np.array([float(r["delay_ms"]) for r in rows])
        run_means = [np.mean([float(r["delay_ms"]) for r in rs]) for rs in by_run.values()]
        mean, half = ci(run_means)
        attempts = np.array([int(r["attempts"]) for r in rows])
        timeouts = np.array([int(r["timeouts"]) for r in rows])
        per_client = defaultdict(list)
        for r in rows:
            per_client[r["client_pos"]].append(float(r["delay_ms"]))
        accepted = [float(r["similarity"]) for r in rows if r["censored"] == "0"]
        net = list(runs[(x, policy)].values())
        cross = [float(n["cross_delay_ms"]) for n in net if n["cross_delay_ms"]]
        drops = sum(int(n["error_drops"]) for n in net)
        received = sum(int(n["received_packets"]) for n in net)
        summary.append({
            "scenario": scenario, "x": x, "policy": policy, "runs": len(run_means),
            "requests": len(rows), "mean_ms": mean, "ci95_ms": half,
            "p95_ms": np.percentile(delays, 95), "p99_ms": np.percentile(delays, 99),
            "mean_attempts": attempts.mean(),
            "rejections_per_req": (attempts - 1 - timeouts).mean(),
            "timeouts_per_req": timeouts.mean(),
            "censored_share": np.mean([int(r["censored"]) for r in rows]),
            "accepted_similarity": np.mean(accepted) if accepted else float("nan"),
            "interest_hops": np.mean([int(r["interest_hops"]) for r in rows]),
            "jain": jain([np.mean(v) for v in per_client.values()]),
            "backhaul_util": np.mean([float(n["util_down_max"]) for n in net]),
            "cross_delay_ms": np.mean(cross) if cross else float("nan"),
            "queue_drops": sum(int(n["queue_drops"]) for n in net),
            "measured_link_loss": drops / (drops + received) if drops + received else 0.0,
            "run_means": run_means,
        })
    return summary


def gate(summary):
    """Per point: delay-aware against the best fixed-alpha exclusion, the oracle and blind."""
    lines, verdicts = [], []
    by_point = defaultdict(dict)
    for r in summary:
        by_point[r["x"]][r["policy"]] = r
    for x, cells in sorted(by_point.items()):
        best = min((cells[p] for p in EXCLUSION if p in cells), key=lambda r: r["mean_ms"])
        aware, oracle = cells["delay-aware"], cells["threshold-a1"]
        # same seeds under every policy, so the per-run differences are paired
        diff, diff_half = ci(np.array(aware["run_means"]) - np.array(best["run_means"]))
        ok = aware["mean_ms"] <= best["mean_ms"] + best["ci95_ms"]
        gap = best["mean_ms"] - oracle["mean_ms"]
        recovered = (best["mean_ms"] - aware["mean_ms"]) / gap if gap > 0 else float("nan")
        verdicts.append(ok)
        lines.append(f"| {x:g} | {aware['mean_ms']:.2f} ± {aware['ci95_ms']:.2f} | "
                     f"{best['policy']} {best['mean_ms']:.2f} ± {best['ci95_ms']:.2f} | "
                     f"{diff:+.2f} ± {diff_half:.2f} | {oracle['mean_ms']:.2f} | "
                     f"{100 * recovered:.0f}% | {'PASS' if ok else '**FAIL**'} |")
    return all(verdicts), lines


def node_states():
    """Delay-aware caches after warm-up, load sweep: mean upstream delay and learned threshold per
    position. Trace rows carry no scenario columns, so the cross rate comes from the per-run file
    name (delay-aware_x<crossRate>_r<run>.csv)."""
    acc = defaultdict(lambda: defaultdict(list))
    for path in sorted((RAW / "load" / "nodes").glob("delay-aware_x*_r*.csv")):
        rate = float(path.stem.split("_x")[1].split("_r")[0])
        x = round(rate * CROSS_DATA_BYTES * 8 / BOTTLENECK_BPS, 2)
        for row in read(path):
            if int(row["request"]) > WARMUP:
                acc[(x, int(row["position"]))]["up"].append(float(row["upstream_ms"]))
                acc[(x, int(row["position"]))]["s"].append(float(row["threshold_own"]))
    return acc


def write(all_summary, gates, nodes):
    OUT.mkdir(parents=True, exist_ok=True)
    fields = [k for k in all_summary[0] if k != "run_means"]
    with (OUT / "summary.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields, lineterminator="\n", extrasaction="ignore")
        writer.writeheader()
        writer.writerows(all_summary)
    with (OUT / "nodes.csv").open("w", newline="", encoding="utf-8") as f:
        f.write("offered_load,position,upstream_ms,threshold_own\n")
        for (x, pos), v in sorted(nodes.items()):
            f.write(f"{x},{pos},{np.mean(v['up']):.3f},{np.mean(v['s']):.4f}\n")

    lines = ["# Stage 4 results — delay-aware caching under backhaul congestion and loss", "",
             "Tandem of 5 caches + origin, 1 Gbps links except a 2 Mbps backhaul (v5–origin, "
             "100-packet DropTail), τ = 1 ms, δ = 10 ms, Interest lifetime 200 ms, contents "
             "per request. 2000 requests per run; the first 500 are warm-up and excluded "
             "everywhere. Mean ± 95% CI over 5 runs. Users: mixed (s drawn from 0.9 / 0.95 / "
             "1.0) unless stated. Cross traffic: Poisson Interests from v1, 1075 B Data from the "
             "origin. Loss: Gilbert–Elliott on every link, both directions, mean burst 3.3.", "",
             "Gate: delay-aware within the CI of, or better than, the best fixed-α exclusion at "
             "every point. Δ = delay-aware − best fixed α, paired over runs (same seeds). "
             "Recovered = share of the gap between the best fixed α and the oracle (s_min in the "
             "Interest) that delay-aware closes without ever seeing s_min.", ""]
    for scenario, (ok, rows) in gates.items():
        lines += [f"## {scenario}: {'PASS' if ok else '**FAIL**'}", "",
                  f"| {SCENARIOS[scenario]} | delay-aware (ms) | best fixed α (ms) | Δ (ms) | "
                  "oracle (ms) | recovered | gate |", "|---|---|---|---|---|---|---|"] + rows + [""]
    lines += ["## Learned state of the delay-aware caches (load sweep, after warm-up)", "",
              "| offered load | position | upstream delay D_up (ms) | learned s* (own client) |",
              "|---|---|---|---|"]
    lines += [f"| {x} | {pos} | {np.mean(v['up']):.2f} | {np.mean(v['s']):.3f} |"
              for (x, pos), v in sorted(nodes.items())]
    lines += ["", "## All cells", "",
              "| scenario | x | policy | mean ± CI (ms) | p95 | p99 | attempts | rejections/req | "
              "timeouts/req | similarity | Interest hops | Jain | backhaul util | cross delay (ms) "
              "| link loss |", "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in all_summary:
        lines.append(
            f"| {r['scenario']} | {r['x']:g} | {r['policy']} | {r['mean_ms']:.2f} ± "
            f"{r['ci95_ms']:.2f} | {r['p95_ms']:.1f} | {r['p99_ms']:.1f} | "
            f"{r['mean_attempts']:.3f} | {r['rejections_per_req']:.3f} | "
            f"{r['timeouts_per_req']:.3f} | {r['accepted_similarity']:.4f} | "
            f"{r['interest_hops']:.2f} | {r['jain']:.3f} | {r['backhaul_util']:.3f} | "
            f"{r['cross_delay_ms']:.1f} | {r['measured_link_loss']:.4f} |")
    (OUT / "analysis.md").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print("\n".join(lines[:60]))


def save(fig, name):
    fig.tight_layout()
    for ext in ("pdf", "png"):
        fig.savefig(FIG / f"{name}.{ext}", dpi=200)
    plt.close(fig)


def series(summary, policy, key="mean_ms"):
    rows = sorted((r for r in summary if r["policy"] == policy), key=lambda r: r["x"])
    return [r["x"] for r in rows], [r[key] for r in rows], [r["ci95_ms"] for r in rows]


def plot_delay(summary, scenario, name, title):
    fig, ax = plt.subplots(figsize=(8.5, 4.2))
    for policy, color in zip(EXCLUSION, EXCL_COLORS):
        x, y, e = series(summary, policy)
        ax.errorbar(x, y, yerr=e, color=color, marker=".", lw=1, capsize=2,
                    label=f"exclusion α = {policy.split('-a')[1]}")
    for policy, (color, marker, ls, lw) in STYLE.items():
        x, y, e = series(summary, policy)
        if x:
            ax.errorbar(x, y, yerr=e, color=color, marker=marker, ls=ls, lw=lw, capsize=3,
                        label=NAMES[policy])
    ax.set_yscale("log")
    ax.set_xlabel(SCENARIOS[scenario])
    ax.set_ylabel("Mean delivery delay after warm-up (ms)")
    ax.set_title(title, fontsize=10)
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=7, loc="center left", bbox_to_anchor=(1.01, 0.5))
    save(fig, name)


def plot_nodes(nodes):
    fig, axes = plt.subplots(1, 2, figsize=(10, 4))
    positions = sorted({pos for _, pos in nodes})
    colors = plt.cm.viridis(np.linspace(0, 0.9, len(positions)))
    for pos, color in zip(positions, colors):
        xs = sorted(x for x, p in nodes if p == pos)
        axes[0].plot(xs, [np.mean(nodes[(x, pos)]["up"]) for x in xs], "o-", color=color,
                     label=f"v{pos + 1}")
        axes[1].plot(xs, [np.mean(nodes[(x, pos)]["s"]) for x in xs], "o-", color=color,
                     label=f"v{pos + 1}")
    axes[0].set_ylabel("Measured upstream delay D_up (ms)")
    axes[1].set_ylabel("Learned threshold s* for its own client")
    for ax in axes:
        ax.set_xlabel(SCENARIOS["load"])
        ax.grid(True, alpha=0.3)
        ax.legend(fontsize=8, title="cache", title_fontsize=8)
    fig.suptitle("Delay-aware caches: congestion raises D_up, so caches answer more", fontsize=10)
    save(fig, "stage4_learned")


def plot_cross(summary):
    fig, ax = plt.subplots(figsize=(8.5, 4.2))
    for policy, color in zip(EXCLUSION, EXCL_COLORS):
        x, y, _ = series(summary, policy, "cross_delay_ms")
        ax.plot(x, y, color=color, marker=".", lw=1, label=f"exclusion α = {policy.split('-a')[1]}")
    for policy, (color, marker, ls, lw) in STYLE.items():
        x, y, _ = series(summary, policy, "cross_delay_ms")
        if x:
            ax.plot(x, y, color=color, marker=marker, ls=ls, lw=lw, label=NAMES[policy])
    ax.set_xlabel(SCENARIOS["load"])
    ax.set_ylabel("Cross-traffic mean delay (ms)")
    ax.set_title("Externality: what each policy does to other traffic on the backhaul",
                 fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=7, loc="center left", bbox_to_anchor=(1.01, 0.5))
    save(fig, "stage4_cross")


def main():
    all_summary, gates, summaries = [], {}, {}
    for scenario in SCENARIOS:
        cells, runs = load_cells(scenario)
        summaries[scenario] = summarise(scenario, cells, runs)
    # 0% loss is the load sweep's zero-load point: same configuration and seeds, so reuse it
    summaries["loss"] += [dict(r, scenario="loss", x=0.0) for r in summaries["load"]
                          if r["x"] == 0]
    for scenario, summary in summaries.items():
        gates[scenario] = gate(summary)
        all_summary += summary
    nodes = node_states()
    write(all_summary, gates, nodes)
    FIG.mkdir(parents=True, exist_ok=True)
    plot_delay(summaries["load"], "load", "stage4_load",
               "Mixed users, congestion on the backhaul")
    plot_delay(summaries["loss"], "loss", "stage4_loss",
               "Mixed users, Gilbert–Elliott burst loss on every link")
    plot_nodes(nodes)
    plot_cross(summaries["load"])


if __name__ == "__main__":
    main()
