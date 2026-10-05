# External review #1 — the ns-3 + ndnSIM recommendation (checked)

This is the answer another research model gave to [research_prompt.xml](research_prompt.xml)
(5 Oct 2026), checked and corrected. The original answer was pasted in chat. This file keeps what
survived checking, my corrections, and what it changes in
[PROBLEM_STATEMENTS.md](PROBLEM_STATEMENTS.md).

## 1. What it recommends

- **Tool:** ns-3 with ndnSIM.
  - The ContentStore lookup is where similarity matching goes.
  - A forwarding strategy decides whether to answer or forward.
  - ns-3 has queueing, loss and energy models.
  - Its UAN module covers underwater acoustic networks.
- **Core project:** PS1 (threshold-carrying requests with bounded retries) plus PS2 (congestion- and loss-aware similarity caching). PS3 is too risky for one semester.
- **Underwater angle:** worth it. Round trips take seconds, so a retry costs seconds and our finding F3 gets much worse. Sending data is very expensive in energy, and sensor and AUV data is naturally similar. It found no published work on approximate or semantic caching underwater, so this looks like an open gap.
- **Our findings F1–F3:** it re-checked all three, found no prior publication of any of them, and rated F3 the strongest.
- **Useful additions:** Jain's fairness index as a metric, an oracle baseline that knows user acceptance, and reporting 95th-percentile delay, not just means.

## 2. Citations — all checked this session (Crossref / arXiv)

| Citation | Status |
|---|---|
| Neglia, Garetto, Leonardi, "Similarity caching: theory and algorithms," IEEE/ACM ToN 2022, doi 10.1109/TNET.2021.3126368 | ✅ |
| Ben Mazziane, Alouf, Neglia, Menasche, "TTL model for an LRU-based similarity caching policy," Comput. Netw. 2024, doi 10.1016/j.comnet.2024.110206 | ✅ |
| **L. Wang, Y. Wang, Z. Yu, F. Xiong, "Similarity caching in dynamic cooperative edge networks: an adversarial bandit approach," IEEE TMC 2025, doi 10.1109/TMC.2024.3500132** | ✅ **New. Cooperative, online similarity caching — read before claiming PS3 or PS4 novelty** |
| Hidouri, Touati, Hadded, Hajlaoui, "Q-ICAN," Comput. Netw. 2023, doi 10.1016/j.comnet.2023.109998 | ✅ Cache-pollution defense in NDN (prior work for PS6) |
| Chaudhary, Hubballi, Kulkarni, "Content caching methods in named data networks," arXiv 2605.13104 (2026) | ✅ Survey of NDN caching |
| Biton, Friedman, "From exact hits to close enough: semantic caching for LLM embeddings," arXiv 2603.03301 (2026) | ✅ |
| Mansoor, Ahmad, Yoon, "Risk-constrained freshness-aware semantic caching for open-web RAG LLMs," arXiv 2607.04281 (2026) | ✅ Per-tier error budgets — relevant to PS8 |
| "Privacy-aware semantic cache for LLMs," arXiv 2403.02694 | ✅ but this is **MeanCache** (an earlier title). Cite it as Gill et al., IPDPS 2025 |
| **P. Nazareth, B. R. Chandavarkar, "Hop-based void avoidance routing protocol for UASNs," IJAHUC 2023, doi 10.1504/IJAHUC.2023.131772** | ✅ **Confirms the guide works on underwater networks** |
| **P. Nazareth, B. R. Chandavarkar, "Cluster-based multi-attribute routing protocol for UASNs," Wireless Pers. Commun. 2024, doi 10.1007/s11277-024-10926-6** | ✅ The model left this DOI as a placeholder; this is the real one |
| Ren, Wu, Zhu, "Asynchronous pattern-designed channel access protocol in underwater acoustic WSNs," JMSE 2023, doi 10.3390/jmse11101899 | ✅ |
| GPTCache (F. Bang, 2023) | Real; cite the NLP-OSS 2023 workshop paper |
| WHOI micro-modem power figures (TX ~50 W, RX ~158 mW, sleep ~5.8 mW) | ⚠️ No source given. Don't use these numbers without a datasheet |

## 3. Corrections — where the review is wrong

1. **Its PS2 mechanism points the wrong way.** It sets $\alpha_{\text{eff}}=\alpha_0 f(q,\text{PER})$ with
   $f$ *decreasing* in congestion, so congested caches answer less and forward more. But forwarding
   is what sends a request into the congested upstream links. Under upstream congestion or loss,
   a cache should answer locally *more*. The review mixes up two different signals:
   - the cache's **own** egress queue, which barely matters, because a local answer goes downstream;
   - the **upstream** path delay and loss, which is exactly what PS3's $\hat D_{\text{up}}$ measures.

   Its AQM hypothesis is ours and is fine: a good AQM reduces the optimal amount of approximation.
   Only the mechanism is backwards.
2. **"Data still matches the Interest name" is false as stated, and examiners will catch it.** NDN
   only accepts a Data packet whose name matches the Interest's name. A substitute item has a
   *different* name, so a stock NDN forwarder would treat it as unsolicited and drop it. ndnSIM's
   ContentStore looks items up by name. We need an explicit design. Options:
   - (a) Name the request after the query (for example `/sim/<query-id>/<s_min>`) and put the substitute's name and its original producer signature inside the Data payload. This is an encapsulated Data, so check signature validation.
   - (b) Change the matching rule in our ndnSIM fork, and argue the security consequences in the report.

   Either way, this is a required design section, not a footnote.
3. **Platform risk it missed.** ndnSIM 2.x builds on its **own fork of an older ns-3** (not
   mainline). Its documentation lists support only for Ubuntu 20.04 and 21.10; newer releases have a
   newer compiler and Boost. **Plan:** a VMware VM running **Ubuntu 20.04**. Get ndnSIM building
   and its examples running in week 1, before designing anything. ndnSIM also looks mostly inactive since about 2021–22; verify this,
   and have an answer ready for "why an old simulator?"
4. **ndnSIM + UAN together is not a given.** ndnSIM sends packets over standard ns-3 network
   devices, so running NDN over the underwater module's devices should work in principle. As far
   as I know nobody has done it. Treat the underwater evaluation as a stretch goal until a
   ten-line test shows Interests crossing a UAN link.
5. **Its hypothesis numbers are invented.** ">30% lower delay", ">10× lower 95th-percentile
   delay", "≥20% lower α". They are fine as targets to try to disprove, but don't present them as
   predictions. For PS1 on the tandem we can compute the expected delay *exactly* in advance,
   using [retry_correlation_check.py](../../build/retry_correlation_check.py) extended to the
   variants. That's stronger than a guessed percentage.
6. **Its PS1 experiment uses δ = 1 ms as the baseline.** Keep the paper's δ = 10 ms so the
   reproduction lines up with their figures. Use a large δ only as the stress case.

## 4. What this changes

| Before | After |
|---|---|
| Tool open (UnetStack / ns-3 / MATLAB) | **ns-3 + ndnSIM is the leading choice.** MATLAB stays as the analysis and plotting companion, as last semester |
| My pick: B2 (PS3 core) | **Reconciled:** PS1 + PS2 as the core (the review's view, and right given the ndnSIM learning curve). PS3's decision rule is the *mechanism* inside PS2, which fixes the review's backwards rule. Full online learning of $F$ is the stretch goal |
| Underwater = speculative | **The guide's underwater research is confirmed**, and underwater similarity caching looks unpublished. Strong option: run the same mechanism on a wired ICN tandem *and* an underwater acoustic tandem (ns-3 UAN), where seconds-long round trips make F3 catastrophic |
| NDN naming not addressed | **New required design item:** how a substitute item travels back in response to a request for a different name (Correction 2) |

## 5. Its open questions, with my suggested defaults

- **Topology:** tandem for the main results, so they compare against the paper. Add one small general graph only if PS4 is chosen.
- **Similarity data:** synthetic $|a-b|^\gamma$ first, to reproduce the paper. Add one real dataset later.
- **Multi-path NDN retries:** no, not in the first version — keep the paper's fixed path.
- **Underwater:** at least a "future work" section with ns-3 UAN parameters. A full scenario if the ndnSIM-over-UAN test passes early.
- **Oracle baseline:** yes. It's cheap and makes every curve easier to read.
