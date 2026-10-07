"""Base-paper model vs. persistent caches (Nakamura & Kamiyama 2024, tandem topology, synthetic dataset).

The paper's R_n = (1-rho)^(n-1) rho treats every retry as an independent draw of cache contents.
Real caches keep their contents between retries (delta = 10 ms), so attempts are correlated and
E[attempts] = E[1/rho_config] >= 1/E[rho_config]  (Jensen).  This script measures the gap.

`--stage3` instead prints the Stage 3 expectations (blind, threshold, exclusion policies).
"""
import sys
from functools import lru_cache
from itertools import product
from math import comb

import numpy as np

C, B, TAU, DELTA = 100, 10, 1.0, 10.0
N_CACHES = 5
TRIALS = 4000
CAP = 5000  # simulator default: a request is logged as censored after this many attempts
rng = np.random.default_rng(7)

idx = np.arange(C)
SIM = 1.0 - np.abs(idx[:, None] - idx[None, :]) / (C - 1)


def model_delay(c, start, alpha, s, cap=None):
    """Paper's model (cumulative RTT, internally consistent conditional delays)."""
    below = int((SIM[c] < s).sum())
    p_ok = 1.0 - comb(below, B) / comb(C, B)
    m = N_CACHES - start
    ok, bad = [], []
    for i in range(m):
        reach = (1 - alpha) ** i
        ok.append((2 * TAU * i, reach * alpha * p_ok))
        bad.append((2 * TAU * i, reach * alpha * (1 - p_ok)))
    ok.append((2 * TAU * m, (1 - alpha) ** m))
    return expected_delay(ok, bad, cap)


def expected_delay(ok, bad, cap=None):
    """Expected delay until an acceptable answer.

    With `cap`, mirrors the simulator: after `cap` failed attempts the request ends at the
    receipt of the last rejected answer, i.e. (cap - 1) * (d_bad + delta) + d_bad.
    """
    rho = sum(p for _, p in ok)
    d_ok = sum(t * p for t, p in ok) / rho
    fail = sum(p for _, p in bad)
    d_bad = sum(t * p for t, p in bad) / fail if fail > 0 else 0.0
    retry = d_bad + DELTA
    if cap is None:
        return d_ok + (1 / rho - 1) * retry

    q = 1.0 - rho
    if q <= 0:
        return d_ok
    tail = q ** cap
    # sum_{m=0}^{cap-1} m q^m in closed form
    s = q * (1 - cap * q ** (cap - 1) + (cap - 1) * q ** cap) / (1 - q) ** 2
    return d_ok * (1 - tail) + retry * rho * s + tail * ((cap - 1) * retry + d_bad)


def persistent_delay(best, c, start, alpha, s, cap=None):
    """Exact expected delay for one fixed cache configuration (contents persist across retries)."""
    m = N_CACHES - start
    ok, bad = [], []
    for i in range(m):
        reach = (1 - alpha) ** i
        hit = best[start + i, c] >= s
        ok.append((2 * TAU * i, reach * alpha * hit))
        bad.append((2 * TAU * i, reach * alpha * (not hit)))
    ok.append((2 * TAU * m, (1 - alpha) ** m))
    return expected_delay(ok, bad, cap)


def main():
    configs = []
    for _ in range(TRIALS):
        best = np.empty((N_CACHES, C))
        for j in range(N_CACHES):
            cached = rng.choice(C, B, replace=False)
            best[j] = SIM[:, cached].max(axis=1)
        configs.append(best)

    print(f"{'alpha':>5} {'s':>5} | {'model ms':>9} {'persist ms':>11} {'ratio':>6} | "
          f"{'P(D>100ms)':>10} {'P(D>1s)':>8} {'worst ms':>10} | "
          f"{'model cap':>9} {'persist cap':>11}")
    for alpha in (0.1, 0.5, 0.9):
        for s in (0.90, 0.95, 0.99, 1.0):
            pairs = [(c, k) for c in range(C) for k in range(N_CACHES)]
            model = np.mean([model_delay(c, k, alpha, s) for c, k in pairs])
            model_cap = np.mean([model_delay(c, k, alpha, s, CAP) for c, k in pairs])
            grid = [(best, c, k) for best in configs[:800] for c in range(0, C, 2)
                    for k in range(N_CACHES)]
            samples = np.array([persistent_delay(b, c, k, alpha, s) for b, c, k in grid])
            capped = np.array([persistent_delay(b, c, k, alpha, s, CAP) for b, c, k in grid])
            print(f"{alpha:5.1f} {s:5.2f} | {model:9.2f} {samples.mean():11.2f} "
                  f"{samples.mean() / model:6.2f} | {np.mean(samples > 100):10.4f} "
                  f"{np.mean(samples > 1000):8.4f} {samples.max():10.0f} | "
                  f"{model_cap:9.2f} {capped.mean():11.2f}")


def acceptable_prob(c, s):
    """P(a uniform random B-subset holds an item with similarity >= s to item c)."""
    below = int((SIM[c] < s).sum())
    return 1.0 - comb(below, B) / comb(C, B)


def threshold_delay(hits, alpha):
    """Threshold-carrying requests: cache i answers iff it holds an acceptable item and its
    alpha-coin says so. Nothing is ever rejected, so there is no retry term (Eq. 8 made exact).
    `hits[i]` = P(cache i on the path holds an acceptable item): a probability for random
    placement, 0/1 for one fixed configuration."""
    delay, reach = 0.0, 1.0
    for i, hit in enumerate(hits):
        q = alpha * hit
        delay += 2 * TAU * i * reach * q
        reach *= 1 - q
    return delay + 2 * TAU * len(hits) * reach


@lru_cache(maxsize=None)
def exclusion_delay(hits, alpha, excluded=frozenset()):
    """Blind caches, contents persisting across retries, and a retry that names the caches which
    served rejected answers: those forward from then on, so a request takes at most len(hits) + 1
    attempts. `hits` is a 0/1 tuple for one configuration."""
    delay, reach = 0.0, 1.0
    for i, hit in enumerate(hits):
        if i in excluded:
            continue
        p = reach * alpha
        delay += p * 2 * TAU * i
        if not hit:
            delay += p * (DELTA + exclusion_delay(hits, alpha, excluded | {i}))
        reach *= 1 - alpha
    return delay + reach * 2 * TAU * len(hits)


def blind_delay(hits, alpha, cap=CAP):
    """persistent_delay for a 0/1 hit pattern, extended to alpha = 1."""
    m = len(hits)
    ok, bad = [], []
    for i, hit in enumerate(hits):
        reach = (1 - alpha) ** i
        ok.append((2 * TAU * i, reach * alpha * hit))
        bad.append((2 * TAU * i, reach * alpha * (1 - hit)))
    ok.append((2 * TAU * m, (1 - alpha) ** m))
    if sum(p for _, p in ok) == 0:
        # alpha = 1 and the client's own cache holds nothing acceptable: it rejects every 0 ms
        # answer until the cap
        return (cap - 1) * DELTA
    return expected_delay(ok, bad, cap)


def persistent_expectation(f, alpha, s):
    """Exact mean of f over uniform clients, items and cache contents.

    Caches are drawn independently, so for item c the flags "cache i holds an acceptable item"
    are i.i.d. Bernoulli(acceptable_prob(c, s)), and every policy's delay depends on a
    configuration only through those flags: enumerating them replaces sampling configurations.
    """
    total = 0.0
    for c in range(C):
        p = acceptable_prob(c, s)
        for start in range(N_CACHES):
            m = N_CACHES - start
            for hits in product((0, 1), repeat=m):
                k = sum(hits)
                total += p ** k * (1 - p) ** (m - k) * f(hits, alpha)
    return total / (C * N_CACHES)


def stage3_table():
    """(alpha, s) -> expected mean delay (ms) per policy, contents persisting across retries."""
    table = {}
    for alpha in (0.1, 0.5, 0.9, 1.0):
        for s in (0.90, 0.95, 1.0):
            closed = np.mean([threshold_delay([acceptable_prob(c, s)] * (N_CACHES - k), alpha)
                              for c in range(C) for k in range(N_CACHES)])
            table[(alpha, s)] = {
                "blind": persistent_expectation(blind_delay, alpha, s),
                "threshold": closed,
                "threshold_persist": persistent_expectation(threshold_delay, alpha, s),
                "exclusion": persistent_expectation(exclusion_delay, alpha, s),
            }
    return table


def stage3_main():
    print(f"{'alpha':>5} {'s':>5} | {'blind cap ms':>12} {'threshold ms':>12} "
          f"{'thr persist ms':>14} {'exclusion ms':>12}")
    for (alpha, s), row in stage3_table().items():
        print(f"{alpha:5.1f} {s:5.2f} | {row['blind']:12.2f} {row['threshold']:12.4f} "
              f"{row['threshold_persist']:14.4f} {row['exclusion']:12.4f}")


if __name__ == "__main__":
    sys.stdout.reconfigure(newline="\n")  # LF even on Windows: the repo is LF-only
    stage3_main() if "--stage3" in sys.argv[1:] else main()
