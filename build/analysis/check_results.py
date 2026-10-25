#!/usr/bin/env python3
"""check_results.py - re-verify the headline claims in research/context/STATUS.md.

Reads only the committed summary CSVs in research/results/ and checks each claim
the report relies on. Prints PASS/FAIL per check and exits non-zero if any fails,
so it can be run before every report edit or commit:

    python3 build/analysis/check_results.py [path/to/research/results]

Needs pandas.
"""
import sys
from pathlib import Path

import pandas as pd

ROOT = Path(__file__).resolve().parents[2]
RES = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "research" / "results"
TAU_MS, CACHES = 1.0, 5          # tandem of 5 caches, 1 ms link delay
MAX_THRESHOLD_MS = 2 * TAU_MS * CACHES + 0.1   # S3 gate 2: 2*tau*5 + epsilon

results = []


def check(name, ok, detail=""):
    results.append(bool(ok))
    print(f"{'PASS' if ok else 'FAIL'}  {name}" + (f"  [{detail}]" if detail else ""))


def as_bool(col):
    return col.astype(str).str.lower().eq("true")


# ---- Stage 2: reproduce the base paper's model -------------------------------------
s2 = pd.read_csv(RES / "stage2" / "summary.csv")
inside = as_bool(s2["within_ci"])
check("S2: every cell within the 95% CI of the analytical value",
      inside.all(), f"{inside.sum()}/{len(s2)} cells")
hot = s2[(s2.alpha == 0.9) & (s2.threshold == 1.0) & (s2.placement == "per-request")]
paper = s2[(s2.alpha == 0.9) & (s2.threshold == 1.0) & (s2.placement == "per-attempt")]
if len(hot) and len(paper):
    ratio = hot.mean_ms.iloc[0] / paper.mean_ms.iloc[0]
    check("S2: persistent contents at alpha=0.9, s=1 are >100x slower than the paper's model",
          ratio > 100, f"{hot.mean_ms.iloc[0]:.0f} ms vs {paper.mean_ms.iloc[0]:.1f} ms = {ratio:.0f}x")

# ---- Stage 3: blind vs threshold vs exclusion --------------------------------------
s3 = pd.read_csv(RES / "stage3" / "summary.csv")
thr, exc = s3[s3.policy == "threshold"], s3[s3.policy == "exclusion"]
inside = as_bool(thr["within_ci"])
check("S3 gate 1: threshold mean within the CI of the closed form in every cell",
      inside.all(), f"{inside.sum()}/{len(thr)} cells, max |t| = {thr.t_stat.abs().max():.2f}")
check("S3 gate 2: threshold needs 1 attempt and <= 2*tau*5 + eps",
      (thr.max_attempts == 1).all() and (thr.max_ms <= MAX_THRESHOLD_MS).all(),
      f"max attempts {thr.max_attempts.max()}, max delay {thr.max_ms.max():.4f} ms")
check("S3 gate 3: exclusion needs <= caches + 1 attempts, per-request path bound holds",
      (exc.max_attempts <= CACHES + 1).all() and as_bool(exc.path_bound_ok).all(),
      f"max attempts {exc.max_attempts.max()}")
head = s3[(s3.alpha == 0.9) & (s3.threshold == 1.0) & (s3.placement == "per-request")]
by = head.set_index("policy").mean_ms
if {"blind", "exclusion", "threshold"} <= set(by.index):
    check("S3: headline ordering blind > exclusion > threshold at alpha=0.9, s=1",
          by["blind"] > by["exclusion"] > by["threshold"],
          ", ".join(f"{p} {by[p]:.2f} ms" for p in ("blind", "exclusion", "threshold")))

# ---- Stage 4: congestion and loss ---------------------------------------------------
s4 = pd.read_csv(RES / "stage4" / "summary.csv")
bad = []
for (scen, x), g in s4[s4.scenario.isin(["load", "loss"])].groupby(["scenario", "x"]):
    fixed = g[g.policy.str.startswith("exclusion-a")]
    da = g[g.policy == "delay-aware"]
    if fixed.empty or da.empty:
        continue
    if not da.mean_ms.iloc[0] < fixed.mean_ms.min():
        bad.append(f"{scen}={x:g}")
check("S4 gate: delay-aware beats the best fixed-alpha exclusion at every load and loss point",
      not bad, "fails at " + ", ".join(bad) if bad else "all points")
load = s4[(s4.scenario == "load") & s4.policy.str.startswith("exclusion-a")]
best = load.loc[load.groupby("x").mean_ms.idxmin()].set_index("x").policy
check("S4: no single fixed alpha is best at every load",
      best.nunique() > 1, "best by load: " + ", ".join(f"{x:g}->{p[-3:]}" for x, p in best.items()))

print(f"\n{sum(results)}/{len(results)} checks passed")
sys.exit(0 if all(results) else 1)