"""Stage 3: blind vs. threshold vs. exclusion, simulated delay against the exact expectations.

Run from the repo root after build/experiments/stage3_sweep.sh (runs 1-5, every cell) and
build/experiments/stage3_followup.sh (runs 6-25, threshold and exclusion cells):
    python build/analysis/stage3_summary.py
Writes research/results/stage3/{summary.csv,analysis.md} and submission/figures/stage3_*.{pdf,png}.
"""
import csv
import math
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import retry_correlation_check as model  # noqa: E402

RAWS = [Path("research/results/raw/stage3/all.csv"),
        Path("research/results/raw/stage3_followup/all.csv")]
FIRST_PASS_RUNS = 5  # the main sweep; the follow-up adds runs 6-25
OUT = Path("research/results/stage3")
FIG = Path("submission/figures")
EPS_MS = 0.1  # serialisation + processing on top of 2*tau per hop (~1.4 us per hop at 1 Gbps)
MAX_DELAY_MS = 2 * model.TAU * model.N_CACHES + EPS_MS
POLICIES = ["blind", "exclusion", "threshold"]
COLORS = {"blind": "tab:red", "exclusion": "tab:orange", "threshold": "tab:green"}
S_COLORS = {0.9: "tab:blue", 0.95: "tab:purple", 1.0: "tab:brown"}


def t_quantile(p, df):
    """Student-t quantile (p > 0.5) by bisection on the numerically integrated CDF; no scipy here."""
    c = math.exp(math.lgamma((df + 1) / 2) - math.lgamma(df / 2)) / math.sqrt(df * math.pi)

    def cdf(x):
        xs = np.linspace(0.0, x, 20001)
        return 0.5 + np.trapezoid(c * (1 + xs ** 2 / df) ** (-(df + 1) / 2), xs)

    lo, hi = 0.0, 60.0
    for _ in range(60):
        mid = (lo + hi) / 2
        lo, hi = (mid, hi) if cdf(mid) < p else (lo, mid)
    return (lo + hi) / 2


def load_runs():
    cells = defaultdict(lambda: defaultdict(list))
    for path in RAWS:
        if not path.exists():
            continue
        with path.open(encoding="utf-8") as f:
            for row in csv.DictReader(f):
                key = (row["policy"], float(row["alpha"]), float(row["threshold"]),
                       row["placement"])
                cells[key][int(row["run"])].append(row)
    return cells


def jain(values):
    values = np.asarray(values, dtype=float)
    return values.sum() ** 2 / (len(values) * (values ** 2).sum()) if values.any() else 1.0


def summarise(cells, expected, max_run=None):
    summary = []
    for (policy, alpha, s, placement), all_runs in sorted(cells.items()):
        runs = {n: rows for n, rows in all_runs.items() if max_run is None or n <= max_run}
        run_means = np.array([np.mean([float(r["delay_ms"]) for r in rows])
                              for rows in runs.values()])
        rows = [r for rows in runs.values() for r in rows]
        delays = np.array([float(r["delay_ms"]) for r in rows])
        attempts = np.array([int(r["attempts"]) for r in rows])
        k = len(run_means)
        se = run_means.std(ddof=1) / np.sqrt(k) if k > 1 else float("nan")
        half = t_quantile(0.975, k - 1) * se
        mean = run_means.mean()
        ref = expected[(alpha, s)][policy]
        per_client = defaultdict(list)
        for r in rows:
            per_client[r["client_pos"]].append(float(r["delay_ms"]))
        accepted = [float(r["similarity"]) for r in rows if r["censored"] == "0"]
        summary.append({
            "policy": policy, "alpha": alpha, "threshold": s, "placement": placement, "runs": k,
            "requests": len(rows), "mean_ms": mean, "ci95_ms": half,
            "median_ms": np.median(delays), "p95_ms": np.percentile(delays, 95),
            "p99_ms": np.percentile(delays, 99), "max_ms": delays.max(),
            "mean_attempts": attempts.mean(), "max_attempts": int(attempts.max()),
            "path_bound_ok": all(int(r["attempts"]) <= model.N_CACHES - int(r["client_pos"]) + 1
                                 for r in rows),
            "censored_share": np.mean([int(r["censored"]) for r in rows]),
            "accepted_similarity": np.mean(accepted) if accepted else float("nan"),
            "interest_hops": np.mean([int(r["interest_hops"]) for r in rows]),
            "jain": jain([np.mean(v) for v in per_client.values()]),
            "expected_ms": ref, "t_stat": (mean - ref) / se if se > 0 else float("nan"),
            "within_ci": abs(mean - ref) <= half,
        })
    return summary


def accounting(cells):
    """Per-request identity in a loss-free run: delay = 2*tau*interest_hops + delta*(attempts - 1)
    + link overhead, where the overhead is serialisation only (no processing delay in ndnSIM), so it
    is exactly 0 for 0-hop requests and a few microseconds per hop otherwise. Any accounting slip
    (a lost attempt, a missed hop, a wrong delta) would show up as >= 2 ms."""
    zero_hop, per_hop, timeouts = [], [], 0
    for runs in cells.values():
        for rows in runs.values():
            for r in rows:
                hops, attempts = int(r["interest_hops"]), int(r["attempts"])
                timeouts += int(r["timeouts"])
                overhead = (float(r["delay_ms"]) - 2 * model.TAU * hops
                            - model.DELTA * (attempts - 1))
                (per_hop if hops else zero_hop).append(overhead / max(hops, 1))
    per_hop = np.array(per_hop) * 1000  # us
    return {"timeouts": timeouts, "zero_hop_max_abs_ms": float(np.max(np.abs(zero_hop))),
            "per_hop_us_min": per_hop.min(), "per_hop_us_max": per_hop.max(),
            "per_hop_us_mean": per_hop.mean()}


def t_by_placement(cells, expected):
    """Mean t per policy and placement, raw and with each request's link serialisation removed
    (delay -> 2*tau*interest_hops + delta*(attempts - 1), exactly what the model describes).
    Static cells with the same run number share one cache configuration across every alpha and s,
    so their t values are correlated draws, unlike per-request cells."""
    lines = []
    for policy in ("threshold", "exclusion"):
        for placement in ("per-request", "static"):
            t_raw, t_ideal, shift_us = [], [], []
            for (p, alpha, s, place), runs in cells.items():
                if p != policy or place != placement:
                    continue
                ref = expected[(alpha, s)][policy]
                raw = np.array([np.mean([float(r["delay_ms"]) for r in rows])
                                for rows in runs.values()])
                ideal = np.array([np.mean([2 * model.TAU * int(r["interest_hops"])
                                           + model.DELTA * (int(r["attempts"]) - 1)
                                           for r in rows]) for rows in runs.values()])
                k = len(raw)
                t_raw.append((raw.mean() - ref) / (raw.std(ddof=1) / np.sqrt(k)))
                t_ideal.append((ideal.mean() - ref) / (ideal.std(ddof=1) / np.sqrt(k)))
                shift_us.append((raw.mean() - ideal.mean()) * 1000)
            lines.append(f"| {policy} | {placement} | {np.mean(t_raw):+.2f} | "
                         f"{np.mean(t_ideal):+.2f} | {np.max(np.abs(t_ideal)):.2f} | "
                         f"{min(shift_us):.1f}–{max(shift_us):.1f} |")
    return lines


def ci_verdict(rows):
    """Per-cell 95% CI count, plus the family-wise check: with n cells, ~5% of them miss a 95% CI
    even when the model is exact, so the cells jointly pass if max |t| stays below the Bonferroni
    critical value t_{1 - 0.05/(2n)}."""
    misses = [r for r in rows if not r["within_ci"]]
    df = min(r["runs"] for r in rows) - 1
    t_fw = t_quantile(1 - 0.05 / (2 * len(rows)), df)
    ts = np.array([r["t_stat"] for r in rows])
    status = "PASS" if not misses else (
        "PASS (family-wise 5%)" if np.abs(ts).max() <= t_fw else "**FAIL**")
    detail = (f"{len(rows) - len(misses)}/{len(rows)} cells within their own 95% CI"
              + "".join(f" [miss: α={r['alpha']} s={r['threshold']} {r['placement']} "
                        f"t={r['t_stat']:.2f}]" for r in misses)
              + f"; max |t| = {np.abs(ts).max():.2f} vs Bonferroni t_crit = {t_fw:.2f} "
              f"({len(rows)} cells, df = {df}); t mean {ts.mean():+.2f}, sd {ts.std(ddof=1):.2f}")
    return status, detail


def gates(summary, first_pass):
    threshold = [r for r in summary if r["policy"] == "threshold"]
    exclusion = [r for r in summary if r["policy"] == "exclusion"]
    first_status, first_detail = ci_verdict([r for r in first_pass
                                             if r["policy"] == "threshold"])
    status, detail = ci_verdict(threshold)
    runs = f"{min(r['runs'] for r in threshold)}–{max(r['runs'] for r in threshold)}"
    return [
        (f"1a. first pass, {FIRST_PASS_RUNS} runs: threshold mean within the 95% CI of the "
         "closed form in every cell", first_status, first_detail),
        (f"1b. re-test of every cell, {runs} runs: threshold mean within the 95% CI of the "
         "closed form", status, detail),
        ("1c. same test for exclusion (not a gate)", *ci_verdict(exclusion)),
        (f"2. threshold: max attempts = 1 and max delay ≤ 2τ·{model.N_CACHES} + ε "
         f"= {MAX_DELAY_MS:.1f} ms",
         "PASS" if all(r["max_attempts"] == 1 and r["max_ms"] <= MAX_DELAY_MS
                       for r in threshold) else "**FAIL**",
         f"max attempts {max(r['max_attempts'] for r in threshold)}, "
         f"max delay {max(r['max_ms'] for r in threshold):.4f} ms"),
        (f"3. exclusion: max attempts ≤ caches + 1 = {model.N_CACHES + 1}",
         "PASS" if all(r["max_attempts"] <= model.N_CACHES + 1 and r["path_bound_ok"]
                       for r in exclusion) else "**FAIL**",
         f"max attempts {max(r['max_attempts'] for r in exclusion)}; per-request bound "
         f"(caches on the path + 1) held: {all(r['path_bound_ok'] for r in exclusion)}"),
    ]


def write_tables(summary, gate_results, acct, t_lines):
    OUT.mkdir(parents=True, exist_ok=True)
    with (OUT / "summary.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(summary[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(summary)

    lines = ["# Stage 3 results — blind vs. threshold vs. exclusion", "",
             "Tandem of 5 caches + origin, C = 100, B = 10, τ = 1 ms, δ = 10 ms, 5000-attempt cap. "
             f"Mean ± 95% CI over {min(r['runs'] for r in summary)}–"
             f"{max(r['runs'] for r in summary)} runs per cell (run-level t interval); "
             "t = (mean − expected) / SE. "
             "Expected = exact expectation from `build/retry_correlation_check.py --stage3` "
             "(contents persisting across retries; the same value applies to static placement). "
             "Similarity = mean over accepted (uncensored) requests. Jain = fairness of per-client "
             "mean delay.", "", "## Gates", ""]
    lines += [f"- {status} — {name}: {detail}" for name, status, detail in gate_results]
    lines += ["", "## Accounting check (every request of every run)", "",
              f"- Timeouts: {acct['timeouts']} (loss-free network, so must be 0).",
              "- delay − 2τ·interest_hops − δ·(attempts − 1) is serialisation only: "
              f"max |·| = {acct['zero_hop_max_abs_ms']:.6f} ms for 0-hop requests; "
              f"{acct['per_hop_us_min']:.3f}–{acct['per_hop_us_max']:.3f} µs per hop "
              f"(mean {acct['per_hop_us_mean']:.3f}) otherwise. The analytical model omits "
              "it; it is far below every confidence interval."]
    lines += ["", "## Bias check: mean t by placement", "",
              "With an exact model, t averages about 0. Columns: t with the simulated delay; t "
              "with each request's link serialisation removed; the largest |t| after that; how "
              "much serialisation adds to a cell mean. Static cells with the same run number "
              "share one cache configuration across all α and s, so their t values move "
              "together.", "",
              "| policy | placement | mean t | mean t, no serialisation | max abs t | "
              "serialisation adds (µs) |", "|---|---|---|---|---|---|"] + t_lines
    lines += ["", "## All cells", "",
              "| policy | α | s | placement | mean ± CI (ms) | expected (ms) | t | within CI | "
              "p95 | p99 | max | attempts mean/max | censored | similarity | Interest hops | "
              "Jain |", "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in summary:
        lines.append(
            f"| {r['policy']} | {r['alpha']} | {r['threshold']} | {r['placement']} | "
            f"{r['mean_ms']:.3f} ± {r['ci95_ms']:.3f} | {r['expected_ms']:.3f} | "
            f"{r['t_stat']:.2f} | {'yes' if r['within_ci'] else '**no**'} | {r['p95_ms']:.1f} | "
            f"{r['p99_ms']:.1f} | {r['max_ms']:.1f} | {r['mean_attempts']:.2f} / "
            f"{r['max_attempts']} | {r['censored_share']:.3f} | {r['accepted_similarity']:.4f} | "
            f"{r['interest_hops']:.2f} | {r['jain']:.3f} |")
    (OUT / "analysis.md").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    print("\n".join(lines))


def pick(summary, **match):
    return [r for r in summary if all(r[k] == v for k, v in match.items())]


def save(fig, name):
    fig.tight_layout()
    for ext in ("pdf", "png"):
        fig.savefig(FIG / f"{name}.{ext}", dpi=200)
    plt.close(fig)


def plot_policies(summary):
    """Figure 1: mean and p99 delay vs s for the three policies, alpha = 0.9, per-request."""
    fig, axes = plt.subplots(1, 2, figsize=(10, 4), sharey=True)
    for policy in POLICIES:
        rows = sorted(pick(summary, policy=policy, alpha=0.9, placement="per-request"),
                      key=lambda r: r["threshold"])
        x = [r["threshold"] for r in rows]
        # log axis: keep the lower whisker above zero
        lower = [min(r["ci95_ms"], 0.95 * r["mean_ms"]) for r in rows]
        axes[0].errorbar(x, [r["mean_ms"] for r in rows],
                         yerr=[lower, [r["ci95_ms"] for r in rows]],
                         fmt="o", color=COLORS[policy], capsize=3, label=f"{policy} (ns-3)")
        axes[0].plot(x, [r["expected_ms"] for r in rows], "--", color=COLORS[policy], alpha=0.7)
        axes[1].plot(x, [r["p99_ms"] for r in rows], "o-", color=COLORS[policy], label=policy)
    for ax, title in zip(axes, ["Mean delay (dashed = analytical)", "99th-percentile delay"]):
        ax.set_yscale("log")
        ax.set_xlabel("Required similarity s")
        ax.set_title(title, fontsize=10)
        ax.grid(True, which="both", alpha=0.3)
    axes[0].set_ylabel("Similar-content delivery delay (ms)")
    axes[0].legend(fontsize=8)
    fig.suptitle("α = 0.9, cache contents persist across retries", fontsize=10)
    save(fig, "stage3_policies")


def plot_threshold_alpha(summary):
    """Figure 2: threshold-mode delay falls monotonically in alpha, so alpha = 1 is optimal."""
    fig, ax = plt.subplots(figsize=(5.5, 4))
    for s, color in S_COLORS.items():
        for placement, marker, shift in (("per-request", "o", -0.012), ("static", "s", 0.012)):
            rows = sorted(pick(summary, policy="threshold", threshold=s, placement=placement),
                          key=lambda r: r["alpha"])
            ax.errorbar([r["alpha"] + shift for r in rows], [r["mean_ms"] for r in rows],
                        yerr=[r["ci95_ms"] for r in rows], fmt=marker, color=color, capsize=3,
                        mfc="none" if placement == "static" else color,
                        label=f"s = {s}, {placement}")
        alphas = np.linspace(0.05, 1.0, 40)
        ax.plot(alphas, [np.mean([model.threshold_delay([model.acceptable_prob(c, s)]
                                                        * (model.N_CACHES - k), a)
                                  for c in range(model.C) for k in range(model.N_CACHES)])
                         for a in alphas], "--", color=color, alpha=0.7)
    ax.set_xlabel("Answer probability α")
    ax.set_ylabel("Mean delay, threshold mode (ms)")
    ax.set_title("Threshold mode: delay falls monotonically to α = 1\n(dashed = Eq. 8, exact)",
                 fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=7)
    save(fig, "stage3_threshold_alpha")


def plot_hops(summary):
    """Figure 3: upstream Interest hops per request, the bandwidth cost of each policy."""
    fig, ax = plt.subplots(figsize=(6, 4))
    s_values = sorted(S_COLORS)
    width = 0.25
    for i, policy in enumerate(POLICIES):
        rows = {r["threshold"]: r for r in pick(summary, policy=policy, alpha=0.9,
                                                 placement="per-request")}
        bars = ax.bar(np.arange(len(s_values)) + (i - 1) * width,
                      [rows[s]["interest_hops"] for s in s_values], width,
                      color=COLORS[policy], label=policy)
        ax.bar_label(bars, fmt="%.2f", fontsize=7)
    ax.set_xticks(np.arange(len(s_values)))
    ax.set_xticklabels([f"s = {s}" for s in s_values])
    ax.set_yscale("log")
    ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda v, _: f"{v:g}"))
    ax.set_ylabel("Upstream Interest hops per request")
    ax.set_title("Bandwidth cost per request (α = 0.9, contents persist)", fontsize=10)
    ax.grid(True, axis="y", which="both", alpha=0.3)
    ax.legend(fontsize=8)
    save(fig, "stage3_hops")


def main():
    cells = load_runs()
    expected = model.stage3_table()
    summary = summarise(cells, expected)
    gate_results = gates(summary, summarise(cells, expected, max_run=FIRST_PASS_RUNS))
    write_tables(summary, gate_results, accounting(cells), t_by_placement(cells, expected))
    FIG.mkdir(parents=True, exist_ok=True)
    plot_policies(summary)
    plot_threshold_alpha(summary)
    plot_hops(summary)


if __name__ == "__main__":
    main()
