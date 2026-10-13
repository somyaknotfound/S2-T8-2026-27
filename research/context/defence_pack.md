# Defence pack — headline numbers and the ten hardest questions

For the viva (Somyak, Pranav, Chris). Each number traces to `research/results/`; the file is in brackets.

## Headline numbers
- **Reproduction:** ns-3 matches the corrected base-paper model in all 18 distinct configurations. Stage 2 lists 24 cells because s = 0.99 and s = 1 are the same for C = 100. [stage2/analysis.md]
- **Heavy tail:** at α = 0.9, s = 1, the model gives 80.6 ms. With persistent contents ns-3 gives 14.5 s, 22% of requests hit the 5000-attempt cap, and p99 is 51.2 s. That is ≥ 180× even with the cap; the exact uncapped ratio is ~1700×. [stage2/analysis.md, retry_correlation.txt]
- **Threshold:** 4.84 ms, always 1 attempt, never more than 10.0077 ms. It matches the closed form in 24/24 cells (25 runs, max |t| 1.61). α = 1 is optimal: 5.89 → 4.72 ms as α goes 0.1 → 1. [stage3/analysis.md, summary.csv]
- **Exclusion:** 32.9 ms, at most 6 attempts, caches stay blind. Upstream Interest hops: blind 158, exclusion 5.0, threshold 2.4. [stage3/summary.csv]
- **Delay-aware vs best fixed α:** −2.8 to −7.0 ms under load, −5.8 to −20.8 ms under loss; every paired CI excludes 0. It closes 43–50% (load) and 28–50% (loss) of the gap to the oracle, and reaches 1.16 vs 1.17 ms when users agree. [stage4/analysis.md]
- **Learned state:** at 0.95 load, v5 measures D_up = 23.5 ms against 11.6 ms at v1, and has the lowest mean s* (0.77). That mean is a mixture: s* flips between about 0.95 and 0 when D_up spikes, in 20% of v5's snapshots. Never call 0.77 "the threshold it learned". [stage4/nodes.csv, convergence.md]
- **Convergence:** no significant drift in delay after warm-up at any load; up to 0.9 load no cache's s* moves more than 0.03 between requests 1000 and 2000. [stage4/convergence.md]

## The ten hardest questions
1. **"Isn't threshold mode trivial — just send s?"** The header field is incidental. The contribution is the analysis: the size of the heavy tail; that s_min makes the paper's Eq. (8) exact; and that it makes α obsolete (α = 1 optimal, proved in §3.4 and seen in the data).
2. **"NDN dropped Selectors such as Exclude in packet format v0.3. How do you carry s_min and the exclusion list?"** In ApplicationParameters, the v0.3 way: a parameterised Interest, which appends a digest component. Answers are built under the full Interest name.
3. **"A substitute item has a different name — doesn't that break NDN?"** We never send Data under the substitute's name. The Data carries the Interest's own name, and its payload states which item it is and how similar. Limitation: the original producer's signature isn't carried. Fix if pushed: embed the original Data's wire encoding.
4. **"Doesn't s_min leak each user's tolerance?"** Yes, which is why exclusion and delay-aware exist. Their decisions never use s. Delay-aware Interests omit it; our exclusion Interests still carry it, but caches ignore it.
5. **"Real caches churn. Persistent contents are the opposite extreme from the model."** True: the truth lies between, set by churn time ÷ δ. We report both ends (per-attempt = the model; per-request and static = persistent). The model is only right at the end where caches churn faster than a 10 ms retry, which real caches don't.
6. **"Why no RED/CoDel?"** ndnSIM 2.9's NetDeviceTransport calls `NetDevice::Send` directly, bypassing ns-3's traffic-control layer. The bundled RED example aborts at t = 0. So congestion is a DropTail device queue, and we claim nothing about AQM.
7. **"Why the debug build? Are the results valid?"** The optimized libraries segfault the first time our strategy answers. The likely cause is an ABI mismatch on `ndn::Data` across the library boundary (unconfirmed). The debug build is Valgrind-clean and matches the analytical model in every gated cell, so its behaviour is checked. The optimized build was never used for a result.
8. **"You patched ndnSIM. Did that change your results?"** No. 216 runs are byte-identical before and after, and the Stage 2 runs replay exactly. The patch only stops leaked scheduler events from slowing long runs quadratically. [stage3/sweep.md]
9. **"Your learners are coupled across caches. Do they converge, or oscillate?"** Time-scale separation helps: D_up is a fast EWMA (gain 1/8), F a slow count, and F is forced monotone. We drop 500 warm-up requests, and with identical users the rule lands on the oracle. Measured: after warm-up the delay has no significant drift at any load, and up to 0.9 load the thresholds stay within 0.03. Concede two things. At 0.95 load the thresholds do not settle: they follow the queue, flipping to "answer anything" when D_up spikes. And there is no formal proof.
10. **"Silence isn't consent: how do you know an answer was accepted?"** A retry naming the cache is a rejection; no retry within W = 1 s is an acceptance; a retry *not* naming it means the answer was lost, and is not counted. The client is identified only by the request fields in the name, a per-request nonce with no persistent user ID. Sensitivity to W hasn't been measured yet; offer it as future work.

Also have ready:
- **"Best fixed α chosen after the fact is unfair."** It is unfair *to us*: no deployed α can do as well, and we still win.
- **"Why ndnSIM 2.9, from 2022?"** It is the NDN simulator that runs the real NFD forwarder, which is why it is pinned to Ubuntu 20.04 in Docker.
