"""Base-paper model vs. persistent caches (Nakamura & Kamiyama 2024, tandem topology, synthetic dataset).

The paper's R_n = (1-rho)^(n-1) rho treats every retry as an independent draw of cache contents.
Real caches keep their contents between retries (delta = 10 ms), so attempts are correlated and
E[attempts] = E[1/rho_config] >= 1/E[rho_config]  (Jensen).  This script measures the gap.
"""
from math import comb

import numpy as np

C, B, TAU, DELTA = 100, 10, 1.0, 10.0
N_CACHES = 5
TRIALS = 4000
rng = np.random.default_rng(7)

idx = np.arange(C)
SIM = 1.0 - np.abs(idx[:, None] - idx[None, :]) / (C - 1)


def model_delay(c, start, alpha, s):
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
    return expected_delay(ok, bad)


def expected_delay(ok, bad):
    rho = sum(p for _, p in ok)
    d_ok = sum(t * p for t, p in ok) / rho
    fail = sum(p for _, p in bad)
    d_bad = sum(t * p for t, p in bad) / fail if fail > 0 else 0.0
    return d_ok + (1 / rho - 1) * (d_bad + DELTA)


def persistent_delay(best, c, start, alpha, s):
    """Exact expected delay for one fixed cache configuration (contents persist across retries)."""
    m = N_CACHES - start
    ok, bad = [], []
    for i in range(m):
        reach = (1 - alpha) ** i
        hit = best[start + i, c] >= s
        ok.append((2 * TAU * i, reach * alpha * hit))
        bad.append((2 * TAU * i, reach * alpha * (not hit)))
    ok.append((2 * TAU * m, (1 - alpha) ** m))
    return expected_delay(ok, bad)


def main():
    configs = []
    for _ in range(TRIALS):
        best = np.empty((N_CACHES, C))
        for j in range(N_CACHES):
            cached = rng.choice(C, B, replace=False)
            best[j] = SIM[:, cached].max(axis=1)
        configs.append(best)

    print(f"{'alpha':>5} {'s':>5} | {'model ms':>9} {'persist ms':>11} {'ratio':>6} | "
          f"{'P(D>100ms)':>10} {'P(D>1s)':>8} {'worst ms':>10}")
    for alpha in (0.1, 0.5, 0.9):
        for s in (0.90, 0.95, 0.99, 1.0):
            model = np.mean([model_delay(c, k, alpha, s) for c in range(C) for k in range(N_CACHES)])
            samples = np.array([
                persistent_delay(best, c, k, alpha, s)
                for best in configs[:800] for c in range(0, C, 2) for k in range(N_CACHES)
            ])
            print(f"{alpha:5.1f} {s:5.2f} | {model:9.2f} {samples.mean():11.2f} "
                  f"{samples.mean() / model:6.2f} | {np.mean(samples > 100):10.4f} "
                  f"{np.mean(samples > 1000):8.4f} {samples.max():10.0f}")


if __name__ == "__main__":
    main()
