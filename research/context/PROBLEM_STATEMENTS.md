# CN Project — Research Directions on Similarity Caching

**Base paper:** R. Nakamura, N. Kamiyama, "Analysis of Similarity Caching on General Cache Networks,"
*IEEE Access* 12:163338–163348, 2024 (doi 10.1109/ACCESS.2024.3489620).
**Course:** CS301 Computer Networks, Prof. B. R. Chandavarkar · **Platform:** NeST · **Status:** pre-decision (4 Oct 2026)

> **Update 5 Oct 2026:** the project must use **UnetStack, ns-3 or MATLAB**, so NeST is out.
> §4–§5 and the NeST experiment plans need rewriting for whichever tool we pick. §1–§3 and the
> ideas in §6 still stand. [research_prompt.xml](research_prompt.xml) is the prompt for other
> research models.
>
> **Review #1 (checked):** [external_review_1_ns3.md](external_review_1_ns3.md) recommends
> ns-3 + ndnSIM, with PS1 + PS2 as the core. Two fixes from checking it:
> - its PS2 rule points the wrong way;
> - NDN name matching blocks substitute answers unless we design around it.
>
> Also confirmed: the guide publishes on underwater acoustic sensor networks.

This is a discussion document, not a decision. §2 holds the ammunition (three things we found
wrong or fragile in the base paper, with numbers). §6 holds eight candidate problem statements,
each with what we would derive, build, measure, and the questions MTech/PhD students will throw
at it. §7–§8 compare them and suggest bundles.

---

## 1. The base paper in one page

**Model.** Cache network $G=(V,E)$, cache nodes $V_c$ (capacity $B$) and origin nodes $V_o$.
A request for content $c$ from client $v$ travels a fixed path $P_{v,c}=(v,\dots,v_c)$ toward the
origin, CCN/NDN style. At each cache, **with probability $\alpha$** the node replies with its most
similar cached item $\arg\max_{c'\in\mathcal B} s(c,c')$; otherwise it forwards. The origin always
returns $c$ itself. The client accepts only if $s(c,c')\ge s$; otherwise it waits $\delta$ and
re-requests.

**Metric.** *Similar-content delivery delay*

$$D_{v,c}=\sum_s \beta^{(s)}_v\,D^{s(\ge)}_{v,c},\qquad
D^{s(\ge)}=\sum_{n\ge1}\Delta^{s(\ge)}_n R^{s(\ge)}_n,\qquad
\Delta_n=(n-1)(d^{s(<)}+\delta)+d^{s(\ge)},\qquad R_n=(1-\rho)^{n-1}\rho .$$

$p^{(s)}_c$ (Eq. 3) is the probability that a cache's best match has similarity exactly $s$, computed
combinatorially as if the cache held a **uniform random $B$-subset** of the catalogue.

**Evaluation.** Numerical only. Tandem of 5 caches + 1 origin, one client per cache, $\tau=1$ ms per
link, $B=10$, $\delta=10$ ms, uniform popularity. Synthetic similarity $s(a,b)\propto d_{\max}-|a-b|^\gamma$
with $C=100$; real similarity from 788 Kaggle "motorbike" images (ORB features).

**Findings.** (i) Delay grows with required similarity; large $\alpha$ helps low-similarity users
and hurts high-similarity users. (ii) Position matters: far clients suffer more; the "acceptable
similarity" is unfair across clients at small $\alpha$ and evens out as $\alpha$ grows.

**Their own future work.** Other topologies; a concrete network architecture; a smarter client
that fetches higher-similarity content instead of blindly retrying.

---

## 2. Where it breaks — our ammunition

### 2.1 Assumptions and what each one hides

| # | Assumption (where) | Why it matters | NeST knob that tests it |
|---|---|---|---|
| A1 | Infinite bandwidth, no queuing, no loss (§III-A) | Delay is pure propagation, so the main *networking* benefit of approximation (avoiding congested upstream links) is invisible | `set_bandwidth`, TCP cross-traffic, qdiscs, `set_packet_loss_gemodel` |
| A2 | Cache = uniform random $B$-subset (Eq. 3) | No replacement policy, no popularity skew, no on-path insertion of responses | Run LRU / qLRU-ΔC in the cache daemon |
| A3 | Uniform popularity, popularity ⟂ similarity (after Eq. 3) | Real catalogues have both dense popular regions and long tails | Zipf + clustered request generators |
| A4 | One global $\alpha$ at every node, applied **even to exact hits** | A cache holding $c$ itself forwards with prob. $1-\alpha$. No position or load awareness | Per-node policies (PS3) |
| A5 | Retries are independent draws (Eq. 9) | Real caches keep their contents across a 10 ms retry, so attempts are correlated. See 2.2(c) | Persistent cache state |
| A6 | Client can measure similarity | True for images; false for LLM answers, where a "similar" answer may simply be wrong | PS8 |
| A7 | Similarity lookup is free | Brute-force argmax is fine at $B=10$; at realistic $B$ it needs an ANN index and costs CPU per hop | Measure lookup time vs. $B$ |
| A8 | Fixed $\delta$, no timeouts | Loss turns into RTO-driven delay | GE loss + timeout logic |
| A9 | Tandem topology, numerical evaluation only | No routing, no path diversity, no packet-level validation | Trees and meshes with FRR routing |

### 2.2 Three things we found (checked, with numbers)

**(a) Eq. (6) is mis-printed; their plots use the correct formula.** As printed,

$$d^{s(\ge)}=\sum_{i=2}^{|P|} 2\,\tau_{P[i-1],P[i]}\;\eta_i ,$$

which weights only the *last-hop* RTT by the probability that node $i$ serves. The expected delay
needs the *cumulative* RTT $\sum_{k=2}^{i}2\tau_{P[k-1],P[k]}$. Check at $\alpha=0.1$ with every
item acceptable, averaged over the five clients: the correct form gives **4.73 ms** and the printed
form gives **1.8 ms**; Fig. 5(a) shows **≈5 ms**. Our re-implementation of the corrected model
reproduces Fig. 5(a) at $s=1$: **9.96 / 45.97 / 80.6 ms** for $\alpha$ = 0.1 / 0.5 / 0.9, against
≈10 / ≈50 / ≈87 in the paper. Why it is useful: it shows in a viva that we re-derived the model
rather than quoting it.

**(b) Eqs. (8) and (10) appear to describe two different caches.**
$\eta_i=\prod_{j<i}(1-q_j)\,q_i$ (Eq. 8) lets a request continue past a node that *answered with an
unacceptable item*. That can only happen if the node knew $s$ and declined, i.e. a
**threshold-aware** cache. $\rho=\sum_i(1-\alpha)^{i-1}q_i$ (Eq. 10) stops the request whenever a
node decides to answer, acceptable or not, i.e. a **blind** cache. So $d^{s(\ge)}$ is computed in
one world and the retry probability in the other. The remaining ~8% gap at $\alpha=0.9$ above is
consistent with this: Eq. (8) puts more probability mass on far nodes. To be safe, word this in a
viva as "appears to — we confirmed by emulation." Bonus: Eq. (8) is *exactly* right for
threshold-carrying requests (PS1).

**(c) Independent retries hide a heavy tail.** This is our strongest finding.
[`build/retry_correlation_check.py`](../../build/retry_correlation_check.py) (raw output in
[`research/results/retry_correlation.txt`](../results/retry_correlation.txt)) uses the paper's own setup (tandem,
synthetic $C=100$, $B=10$, $\delta=10$ ms, all five clients). It compares the model against caches
whose random contents **persist** across retries. The persistent column is the exact expectation
per configuration, averaged over 800 configurations.

| α | s | model (ms) | persistent (ms) | ratio | P(D>100 ms) | P(D>1 s) | worst |
|---|---|---|---|---|---|---|---|
| 0.1 | 0.95 | 6.51 | 6.65 | 1.02 | 0 | 0 | 19 ms |
| 0.5 | 0.95 | 7.00 | 13.91 | 1.99 | 0.012 | 0 | 372 ms |
| 0.5 | 1.00 | 45.97 | 97.77 | 2.13 | 0.275 | 0 | 372 ms |
| 0.9 | 0.90 | 1.72 | 109.78 | **64** | 0.023 | 0.015 | 1022 s |
| 0.9 | 0.95 | 6.38 | 3186.81 | **499** | 0.172 | 0.118 | 1022 s |
| 0.9 | 1.00 | 80.62 | 137187.73 | **1702** | 0.697 | 0.648 | 1022 s |

*Why.* Suppose every cache on the path holds only unacceptable items. Then an attempt succeeds
only if all $m$ caches choose to forward, which has probability $(1-\alpha)^m=10^{-5}$ for
$\alpha=0.9$ and $m=5$. That means about $10^5$ attempts × ($\delta$ + RTT), roughly 1000 s. The
model averages $\rho$ over random contents and then inverts it; the truth averages $1/\rho$. By
Jensen's inequality, $\mathbb E[1/\rho]\ge 1/\mathbb E[\rho]$.

*Caveat — say this before they do.* Static contents is the opposite extreme from the model's
contents that re-randomise every 10 ms. Real caches churn because of *other* users' requests.
The truth lies in between, governed by **churn time-scale ÷ δ**, and that is exactly what our
first NeST experiment measures (E0.3).

*Implication.* The paper's Fig. 7 conclusion, "higher α evens out fairness," is computed in the
regime where the heavy tail is worst. It may not survive a real network.

---

## 3. Literature map (2008–2026)

I checked every entry below this session against arXiv, IEEE/ACM listings, or the base paper's
own reference list. Entries marked † are standard classics I did not re-check here; verify them
before they go into the report. The background research agent also produced a 70-paper list, but
it had wrong venues, years, and author orders. For example, it put FoggyCache in SOSP (it is
MobiCom 2018) and Cachier in TMC (it is ICDCS 2017), and it gave one malformed arXiv ID. I kept
only verified items.

### 3.1 Similarity caching — theory and algorithms
| Paper | What it does | Gap it leaves |
|---|---|---|
| Falchi et al., LSDS-IR 2008; Pandey et al., WWW 2009 | Origin of similarity caching (metric cache for search; NN caching for ad content-match) | Single cache, heuristic |
| Neglia, Garetto, Leonardi, *IEEE/ACM ToN* 30(2), 2022 (arXiv 1912.03888) | Theory: optimal static placement is NP-hard; dynamic policies qLRU-ΔC and DUEL | Single cache; cost model, not network delay |
| Zhou, Simeone, Zhang, Wang, *IEEE Netw. Lett.* 2020 (arXiv 2010.07585) | Offline/online similarity caching | Single cache, small scale |
| Sabnis et al., GRADES, INFOCOM 2021 | Gradient descent on cache state | Optimises *what to store*, not *whether to answer* |
| Si Salem, Neglia, Carra, AÇAI, ITC-33 2021; ToN (doi 10.1109/TNET.2022.3217012) | Mirror ascent + approximate indexes, adversarial guarantees | Index cost not modelled; single cache |
| Garetto, Leonardi, Neglia, *Computer Networks* 201, 2021 (arXiv 2102.04974) | Placement in **networks** of similarity caches (optimal on trees, heuristic on general graphs) | Offline, no queueing, no packet-level evaluation |
| Ben Mazziane, Alouf, Neglia, Menasché, "Computing the hit rate of similarity caching," arXiv 2209.03174 | Hit-rate computation for similarity caches | Single cache |
| Ben Mazziane et al., "TTL model for an LRU-based similarity caching policy," *Computer Networks* 241, 2024 | TTL approximation for similarity-LRU, validated on DNS traces | Single cache — **this is the tool to replace Eq. 3's uniform-subset assumption** |
| Nakamura & Kamiyama, *IEEE Access* 2024 | **Base paper** — user-level delay over a path | §2 |
| Nakamura, "Random walk based similar content discovery," *IEICE Trans. Commun.* E108-B(8):934–943, 2025 | Same author: *s-content discovery time* via random walks on graphs | Analytical; no implementation |

### 3.2 Semantic caching for LLM / GenAI — where the field is hottest (2023–2026)
| Paper | What it does | Gap it leaves |
|---|---|---|
| GPTCache, NLP-OSS 2023 † | Open-source embedding-similarity cache for LLM APIs | Static threshold; single server-side cache |
| MeanCache (Gill et al.), IPDPS 2025 (arXiv 2403.02694) | Client-side, federated semantic cache; contextual queries | Device-only; no network tiers |
| SCALM (Li et al.), IWQoS 2024 (arXiv 2406.00025) | Real chat-log analysis; clustering-based semantic cache | Single cache |
| vCache (Schroeder et al.), arXiv 2502.03771, 2025 | Per-embedding **learned thresholds** with a user-set error bound | Single cache; no network delay |
| Zhu, Zhu, Jiao, arXiv 2402.01173, 2024 | Prompt caching via embedding similarity, learned similarity | Single cache |
| Iyengar et al., arXiv 2503.17603, 2025 | Generative cache that synthesises answers from cached ones | No network model |
| Liu et al., arXiv 2508.07675, 2025 | Semantic caching from offline learning to online adaptation | Single cache |
| Chakraborty et al., arXiv 2511.17565, 2025 | Generative caching for structurally similar prompts (agent workflows) | Single cache |
| **Singh et al., "Asynchronous verified semantic caching for tiered LLM architectures," arXiv 2602.13165, 2026** | Verified semantic caching **across tiers** | **Closest to PS8 — read in full before choosing PS8** |
| Bergman et al. (Proximity), arXiv 2503.05530, 2025 | Approximate caching of RAG retrieval results | Single node |
| Agarwal et al. (NIRVANA), arXiv 2312.04429 / NSDI 2024 | Approximate caching for diffusion models (reuses intermediate noise states) | Single GPU server |
| Zhu et al., arXiv 2306.02003, 2023 | Optimal caching + model multiplexing for large-model inference | Single tier |

### 3.3 Approximate computation reuse at the edge and in the network
| Paper | Relevance |
|---|---|
| Drolia et al., Cachier, ICDCS 2017 | Edge caching of recognition results; balances edge vs. cloud using network estimates |
| Guo, Hu, Li, Hu, FoggyCache, MobiCom 2018 | Cross-device approximate reuse with A-LSH; 3–10× latency/energy savings |
| Xu et al., ISAC, INFOCOM 2023 (base-paper ref 23) | In-switch approximate cache on programmable switches |
| Nour & Cherkaoui, arXiv 2104.03818, 2021 | Network-based compute-reuse architecture for IoT (ICN-flavoured) |
| Al Azad & Mastorakis, arXiv 2109.01608, 2021 | Promise and challenges of computation deduplication at the edge |
| Si Salem et al., "Towards inference delivery networks," arXiv 2105.02510 | Distributing ML inference over a network with optimality guarantees |

### 3.4 Security and privacy
| Paper | Relevance |
|---|---|
| Lauinger et al., 2012 †; Acs et al., "Cache privacy in named-data networking," ICDCS 2013 † | Timing reveals ICN cache hits; random caching and delay padding as defenses — **the exact-caching ancestor of PS5** |
| Song et al., "The early bird catches the leak," arXiv 2409.20002, 2024 | Timing side channels in LLM serving (prefix/semantic caches) |
| Zheng et al., InputSnatch, arXiv 2411.18191, 2024 | Stealing user inputs via cache-timing in LLM services |
| Zhang et al., "Key collision attack on LLM semantic caching," arXiv 2601.23088, 2026 | Crafted queries collide with victims' cache keys |
| Zhang et al., "Similarity is not validity," arXiv 2609.35908 (28 Sep 2026) | Defending semantic caches against poisoning — a week old, so very current |
| Xie et al., CacheShield, INFOCOM 2012 †; Conti, Gasti, Teoli, *Computer Networks* 2013 † | ICN cache-pollution attacks and detection — ancestors of PS6 |
| Radovanović et al., "Hubs in space," JMLR 2010 † | Hubness in high-dimensional nearest-neighbour search — the lever in PS6 |

### 3.5 Video, cooperation, and analysis tools
| Paper | Relevance |
|---|---|
| Araldo, Martignon, Rossi, IFIP Networking 2016 (base-paper ref 24) | Representation selection — a lower bitrate counts as a "similar" substitute (PS7) |
| Li et al., "Keep your low-bitrate close, and high-bitrate closer," arXiv 1903.09701 | QoE in cache hierarchies for multi-bitrate video |
| Fan, Cao, Almeida, Broder, "Summary cache," SIGCOMM 1998 † | Bloom-filter cache digests — ancestor of PS4 |
| Che et al., JSAC 2002 †; Fricker et al., ITC 2012 †; Martina et al., INFOCOM 2014 (base-paper ref 29) | LRU / cache-network approximations for the analytical work |
| Rai, Narayan G., Dhanasekhar M., Monis, Tahiliani, "NeST: Network Stack Tester," ANRW 2020 (doi 10.1145/3404868.3406670) | Our platform |

**The common gap across all of §3:** similarity caching is either analysed as a single cache, or
optimised for *placement* in a network with a delay model that has no queueing, loss, or real
stack. Nobody has (i) a packet-level evaluation of a similarity cache *network*, or (ii) a
response decision ("answer approximately or forward?") that adapts to position, load, and user
feedback.

---

## 4. What NeST gives us (read from the source on GitLab, Oct 2026)

| Need | NeST API | Use |
|---|---|---|
| Topology | `Node`, `Router`, `Switch`, `connect()`, `set_address`, `add_route`; helpers `dumbbell`, `gfc1`, `gfc2` | Tandem, tree, and multi-bottleneck (GFC = parking-lot) cache networks |
| Link emulation | `set_bandwidth`, `set_delay`, `set_delay_distribution(jitter, normal / pareto / paretonormal)`, `set_packet_loss(rate, correlation)`, **`set_packet_loss_gemodel`** (Gilbert–Elliott), `set_packet_loss_state` (4-state Markov), `set_packet_corruption` | Finite bandwidth, jitter, burst loss — the GE parameters from last sem's FEC project carry over directly |
| Background traffic | `Experiment.add_tcp_flow` (cubic, bbr…; netperf/iperf3), `add_udp_flow`, `add_mptcp_flow`, `add_http_application`, `add_mpeg_dash_application` (gpac/vlc), `add_coap_application`, `add_sip_application` | Congestion and cross-traffic; DASH for PS7 |
| Our own daemons | `with node:` → `engine.set_ns(node.id)`; start `subprocess.Popen(...)` inside the block | Cache, origin, and client processes run inside namespaces |
| Observability | `capture_packets` (tshark), `ping`/`traceroute`/`mtr`, `require_qdisc_stats`, ss stats | Queue occupancy, Wireshark evidence (as in last sem's report) |
| **Not provided** | No caching, ICN, or NDN support | **We build the cache daemons — that is the project's code** |

Notes:
- Requirements: Linux and Python ≥ 3.12, per the research agent's reading of `setup.py` (WSL Ubuntu 24.04 has 3.12.3). The PyPI name may have changed from `nitk-nest` — re-check the README on install.
- `nest.nitk.ac.in` resolves to an NITK-internal IP and refused connections from outside, so read the docs on the campus network.

**"Why NeST and not ns-3 or Mininet?"** It runs the real Linux stack (real TCP, real qdiscs), so
the same daemon would run unmodified on real hosts. It has Python-native experiment scripts with
built-in GE loss, AQM and qdisc stats, and it is maintained at NITK. **Honest limitations:** it
is a single host, so CPU contention can distort timing. Mitigations: keep the request rate
modest, measure lookup time separately, repeat trials and report confidence intervals. No
wireless.

---

## 5. Phase 0 — the shared testbed (every problem statement needs it)

```
 u1     u2     u3     u4     u5          u_i   = client daemon (request generator, retry logic, logger)
 |      |      |      |      |           cache = router namespace running simcache daemon
cache1-cache2-cache3-cache4-cache5-origin  links: set_bandwidth / set_delay / loss per experiment
```

**Daemons.** Python asyncio over UDP, requests and responses along an overlay chain that follows
the IP path.
- `simcache`: pluggable *response policy* (blind-α as in the paper, threshold-aware, learned);
  pluggable *replacement* (static random as in the paper, LRU, qLRU-ΔC); on-path insertion on or off.
- `origin`: always serves the exact item.
- `client`: uniform or Zipf requests, β distribution, retry logic.

**Defence answer to "an overlay is unrealistic":** CDN hierarchies and MEC caches *are* overlays,
and NDN's forwarder (NFD) is a user-space daemon too.

**Proposed request header** (final fields depend on the chosen PS):

| Field | Bytes | Purpose |
|---|---|---|
| ver/type | 1 | request / response |
| req_id | 4 | match responses |
| content_id | 4 | requested item |
| prev_req_hash | 4 | links a retry to its predecessor without a persistent client ID (PS3) |
| attempt | 1 | retry count |
| s_min | 2 | optional similarity threshold (PS1) |
| hop / excl bitmap | 2 | serving hop; hops to skip on retry (PS1) |

**Experiments.**
- E0.1 Reproduce Figs. 5–7 on NeST (static random contents, 1 ms links, no load). Should match the corrected model within confidence intervals.
- E0.2 Persistent contents: the heavy tail of §2.2(c) measured on a real stack.
- E0.3 Churn: background requests + LRU, sweeping churn time-scale ÷ δ.
- E0.4 Bandwidth + TCP cross-traffic.
- E0.5 GE loss.
- E0.6 Lookup cost vs. $B$.

Output: a **model-error map** showing where the analysis holds and where it breaks. That alone is
a defensible reproducibility-plus-realism contribution, and it de-risks every option below.

```python
# Phase-0 skeleton — API names read from NeST source; argument types to confirm on first run
import subprocess
from nest.topology import Node, connect

caches = [Node(f"cache{i}") for i in range(1, 6)]
origin = Node("origin")
for n in caches:
    n.enable_ip_forwarding()

a, b = connect(caches[0], caches[1])
a.set_address("10.0.1.1/24")
b.set_address("10.0.1.2/24")
for iface in (a, b):
    iface.set_attributes("10mbit", "1ms")   # E0.5 adds iface.set_packet_loss_gemodel(...)

with caches[0]:
    subprocess.Popen(["python3", "simcache.py", "--policy", "blind", "--alpha", "0.5",
                      "--upstream", "10.0.1.2"])
```

---

## 6. Candidate problem statements

Each option follows the same pattern: statement → why it's open → core idea → what we derive →
NeST experiments → hard questions.

### PS1 — Threshold-carrying requests and the correlated-retry problem
**Statement.** Given that blind similarity caches produce heavy-tailed delay under persistent
contents (§2.2c), design request-level signalling that bounds retries, derive closed-form delay
for each variant, and validate on NeST.

**Variants.**
- V0 blind (the paper).
- V1 **threshold in request** ($s_{\min}$): a cache answers only if its best match ≥ $s_{\min}$, so zero retries — and Eq. (8) becomes exact.
- V2 **exclusion on retry**: the retry carries a bitmap of hops that served unacceptable items; those hops forward. At most $m+1$ attempts.
- V3 **escalation**: the retry carries `min_hop` = last serving hop + 1. Stateless, at most $m$ retries.

**Derive.** Closed forms for V1–V3. The ordering $\mathbb E[D_{V0}]\ge \mathbb E[D_{V2}]\ge \mathbb E[D_{V1}]$.
A crisp result: **under V1, α = 1 is delay-optimal.** If an acceptable item is cached, answering
always beats forwarding. α only made sense because caches were blind.

**NeST.** Tandem, then tree topologies. Measure delay distributions (not just means), upstream
bytes, and header overhead.

**Hard questions.**
- *"Isn't V1 trivial?"* — The contribution is the analysis: the heavy-tail proof and its size, that V1 makes the paper's own Eq. (8) exact, and that it makes α obsolete. The header field is incidental.
- *"NDN removed selectors such as Exclude."* — Yes, NDN packet format v0.3 dropped them for scalability (†). $s_{\min}$ would go in ApplicationParameters (a parameterised Interest).
- *"Doesn't $s_{\min}$ leak user preferences?"* — Yes; see PS5.

**Effort / risk.** Low–medium. The risk is that it looks small, so pair it with PS2 or PS3.

### PS2 — Congestion- and loss-aware similarity caching ("approximation as a congestion valve")
**Statement.** Given finite-capacity, lossy links with cross-traffic, characterise how the optimal
response probability shifts with load. Design a load-adaptive similarity cache and validate on
NeST against TCP cross-traffic, AQM, and Gilbert–Elliott loss.

**Why open.** A1. With infinite bandwidth, serving approximately only saves propagation time.
Under load it also avoids queues — and it *reduces the load* other flows see.

**Derive.** Add per-link queueing $W(\lambda)$ (M/D/1 for fixed-size responses) and per-hop loss
with timeout to $d^{s(\cdot)}$. Show that the optimal α* rises with upstream utilisation. Then study
the feedback loop: more approximation → less upstream load → less need for approximation.
Stability (oscillation vs. damping) is a good PhD-level discussion point.

**NeST.**
- Topology: the GFC-2 helper (multi-bottleneck) with caches at routers.
- Cross-traffic: cubic and bbr flows.
- Queueing: **pfifo vs. fq_codel vs. cake**.
- Loss: GE, with parameters reused from last sem's FEC project.
- Metrics: similar-content delay, delivered similarity, **cross-traffic throughput and RTT** (the externality), queue occupancy from `require_qdisc_stats`.

**Non-obvious hypothesis.** A good AQM *reduces* the optimal amount of approximation, because
fq_codel removes the queueing delay that made approximating attractive. So AQM and similarity
caching are partly substitutes.

**Hard questions.**
- *"M/D/1 is a toy."* — Validate it against measured qdisc backlog.
- *"Isn't this just load balancing?"* — No: the trade is *answer quality* for delay, which no load balancer offers.
- *"The cross-traffic choice is arbitrary."* — Sweep it.

**Effort / risk.** Medium / low. The most syllabus-aligned option: performance metrics, queueing,
congestion.

### PS3 — A delay-optimal, self-tuning response rule learned from implicit feedback
**Statement.** Replace the base paper's global α with a per-node rule derived from the
user-level delay metric itself. Learn its inputs online from signals already present in the
network — retries and RTTs — and validate on NeST across positions, loads, and loss.

**Derivation (the core).** Node $v$ holds best candidate $c'$ with similarity $s'$. Let $F(s')=\sum_{s\le s'}\beta(s)$
be the probability that a user accepts $s'$, $\hat D_{\text{up}}$ the expected delay if we forward,
and $D_{\text{retry}}$ the delay after a rejection. The return path is common to both actions, so

$$\mathbb E[D\mid\text{answer}]=(1-F(s'))(\delta+D_{\text{retry}}),\qquad \mathbb E[D\mid\text{forward}]=\hat D_{\text{up}}$$

$$\Rightarrow\ \text{answer iff } s'\ge s^*_v=F^{-1}\!\left(1-\frac{\hat D_{\text{up}}}{\delta+D_{\text{retry}}}\right).$$

**Properties.**
1. An exact hit ($s'=1$) is always served — fixes A4.
2. $s^*_v$ rises as $\hat D_{\text{up}}$ falls, so nodes near the origin are stricter. This *derives* the paper's position effect (Figs. 6–7) instead of observing it.
3. Congestion inflates $\hat D_{\text{up}}$, so $s^*$ drops automatically. PS2's valve comes for free.
4. Soft state: if node $v$ already served $c'$ to this request chain, it forwards on the retry. Retries are bounded, which fixes §2.2(c) **without a header change**.

**Learning the inputs.**
- $\hat D_{\text{up}}$: EWMA of forwarded-request RTTs — the **Jacobson/Karels SRTT/RTTVAR estimator TCP uses for RTO** (direct syllabus tie).
- $\hat F$: per-similarity-bin acceptance. An approximation served at bin $k$ followed by a linked retry within window $W$ counts as a rejection; otherwise as an acceptance. Decay old counts.
- Exploration: ε-greedy, or Thompson sampling to learn $F$ in the low bins.

**Baselines.** The paper's α ∈ {0.1 … 0.9}; the best static α per scenario (grid oracle); PS1-V1
with known β; a full-information oracle that knows β and $D_{\text{up}}$.

**Hypothesis.** It tracks the oracle and beats every static α across load levels, because no single
α is right for all positions and loads.

**Novelty claim.** The first network-level similarity cache whose answer/forward decision is
(a) derived from the user-level delay metric and (b) learned online from in-network signals.
vCache learns thresholds for *correctness* at a single cache; GRADES and AÇAI optimise *what to
store*.

**Hard questions.**
- *"Linking retries needs client identity."* — Use `prev_req_hash` chaining: a per-request nonce, no persistent ID.
- *"Coupled learners — do they converge?"* — Upstream policy changes downstream $\hat D_{\text{up}}$. Argue time-scale separation plus monotonicity; show convergence and damping plots.
- *"Users differ."* — $F$ is the population CDF, so the rule is population-optimal; add per-class $F$ if requests carry a class tag.
- *"Acceptance is censored — silence isn't consent."* — Analyse how the choice of $W$ misclassifies.
- *"Why not just PS1?"* — PS3 needs no protocol change and does not reveal $s$ (PS5). We compare against PS1 anyway.

**Effort / risk.** Medium–high. Risk: noisy learning at low request rates.

### PS4 — Similarity digests for off-path cooperative lookup on general topologies
**Statement.** Answer the paper's "other topologies" future work. Neighbouring caches exchange
compact **LSH-in-Bloom-filter digests** of their contents, so a node can detour to an *off-path*
neighbour that likely holds an item with similarity ≥ $s$. Quantify control overhead against delay
on NeST.

**Derive.** For LSH $(k,L)$, the probability that a similar item shares a bucket. Bloom false
positives $f=(1-e^{-kn/m})^k$. Staleness as a function of the digest period $T$. Detour only if
expected gain > detour RTT.

**NeST.** Tree (CDN hierarchy), ring/mesh, and a small ISP-like topology with FRR routing. Vary $T$,
Bloom size and LSH parameters. Metrics: delay, similarity, digest bytes/s, detour rate, cost of
false positives. Baselines: on-path only (the paper); global-knowledge oracle; full uncompressed
index exchange.

**Hard questions.**
- *"LSH on 1-D synthetic data is trivial."* — Use embeddings for the real evaluation.
- *"Why not a DHT?"* — Multi-hop lookup cost and no locality.
- *"Isn't this Summary Cache with LSH?"* — Yes, and the contribution is quantifying it for similarity: similarity-dependent false positives and the gain/overhead frontier.

**Effort / risk.** High / engineering-heavy. The most "protocol" option.

### PS5 — Leakage in similarity cache networks; α as a privacy knob
**Statement.** Show that similarity caching leaks more than exact caching. An attacker sharing an
edge cache with a victim learns about the *neighbourhood* of the victim's requests — through
timing, and also directly, because **the approximate answer *is* the victim's cached item**.
Quantify the privacy–delay trade-off of defenses on NeST.

**Threat model.** The attacker probes near a sensitive region $R$ of content space (a medical image
class, a prompt topic). Advantage = accuracy/AUC at distinguishing "victim requested from $R$" from
"did not".

**Defenses.**
- D1: α as randomised response.
- D2: first-$k$ delay padding (Acs et al. style).
- D3: k-anonymous approximate serving — only serve an item as an approximation once $k$ distinct requesters asked for it.
- D4: noise or quantisation on the returned similarity.

A sharp viva point: **α alone does not give differential privacy**, because "not cached" never
produces a cache response. A formal guarantee needs padding or decoys. That is honest and
interesting.

**NeST.** Real cross-namespace timing plus `set_delay_distribution` jitter; tshark capture to
*show* the leak. Output: attacker-advantage vs. delay curves per defense.

**Hard questions.**
- *"Is this realistic?"* — Shared campus or MEC edge caches, and LLM prompt caches (InputSnatch, Early Bird).
- *"Why not encrypt?"* — Encryption hides payloads from on-path observers, not hit/miss to the requester.
- *"Your privacy definition?"* — Advantage-based, plus a DP discussion.

**Effort / risk.** Medium / the formal privacy part can grow. Very current.

### PS6 — Hubness-based pollution of similarity caches, and rejection-feedback defense
**Statement.** In high-dimensional similarity spaces, "hub" items are nearest neighbours of many
others. An attacker who keeps hubs cached makes caches answer many requests with poor
substitutes. That causes rejections, then retries, then inflated delay and upstream load. Design
and evaluate a defense that uses rejection feedback: evict or ban items whose rejection ratio is
high, plus similarity-aware admission.

**Links.** ICN pollution (CacheShield, Conti et al.); LLM key collision (2601.23088) and poisoning
(2609.35908). It reuses PS3's feedback machinery, so it is **best as an add-on to PS3**.

**Needs.** Embeddings (1-D synthetic data has no hubs).

**Effort / risk.** Medium.

### PS7 — Representation substitution for adaptive video (DASH)
**Statement.** Treat a cached *different bitrate* of the same segment as the "similar content."
On a miss for (segment $k$, rate $r$), the edge serves cached $(k,r')$ if quality ratio ≥ σ (or a free
upgrade), and signals it so the ABR is not misled. Evaluate QoE under congestion on NeST.

**Key subtlety.** ABR throughput estimation is corrupted when the cache returns a different size,
so the response must carry the served representation in a header.

**NeST.** NeST has DASH built in (gpac/vlc), but substitution needs our own HTTP proxy and
probably our own Python ABR client (buffer-based or throughput-based), with ffmpeg-generated
bitrate ladders.

**Metrics.** QoE (bitrate, rebuffering, switches, startup), delivered quality, origin load.

**Local tie.** NeST's group at NITK works on streaming over AQM. Verify authors before claiming
this — a search surfaced arXiv 2511.17525 on PIE/FQ-PIE for multimedia.

**Hard questions.**
- *"Is a lower bitrate 'similar' in the paper's sense?"* — Yes, with similarity defined as quality.
- *"ABR already adapts."* — The cache adapts per hop, instantly, with no estimation lag.

**Effort / risk.** High (video pipeline).

### PS8 — Tiered semantic caching for LLM queries: splitting an error budget across network tiers
**Statement.** Device → edge → regional → cloud LLM. The user **cannot** judge similarity (A6), so
thresholds must be enforced in the network. Choose per-tier thresholds $\theta_t$ that minimise
expected delay subject to an end-to-end false-hit budget ε, given real tier RTTs and congestion.

**Data that makes it rigorous.** Quora Question Pairs: its `is_duplicate` labels give ground truth
for false hits. Precompute small sentence embeddings offline. The origin is a replay server with an
emulated service-time distribution, so **no real LLM is needed** and it runs on a laptop.

**Derive.** Per-tier curves $h_t(\theta)$ (hit rate) and $e_t(\theta)$ (false-hit rate) give a
constrained allocation problem (a Lagrangian), learned online vCache-style.

**Hard questions.**
- *"This is ML, not networking."* — The contribution is RTT- and congestion-aware allocation across network tiers; the LLM is a black-box origin.
- *"How do you differ from 2602.13165 (tiered verified caching, 2026)?"* — **Must read it first.** This is the main novelty risk.

**Effort / risk.** Medium / overlap risk. Strongest appeal to PhD students; weakest fit with a
networks professor unless framed carefully.

### (Theory add-on, any option) Replacement-aware analysis
Replace Eq. (3)'s uniform-subset assumption with a similarity-LRU steady state (Ben Mazziane TTL
model). Propagate *filtered miss streams* up the path, as in Che-style analysis of cache networks.
Validate on NeST. Heavy, but it makes any option above much harder to dismiss.

---

## 7. Comparison

| | Novelty | Networking depth (prof fit) | NeST fit | Theory | Engineering | Main risk | Defence strength |
|---|---|---|---|---|---|---|---|
| PS1 thresholds/retries | Medium | Medium–high | High | Closed forms | Low | "Too simple" | Strong if paired |
| PS2 congestion valve | Medium | **Very high** | **Very high** | Queueing | Medium | Low | Strong |
| PS3 self-tuning rule | **High** | High | High | Decision rule + fixed point | Medium–high | Learning noise | **Very strong** |
| PS4 similarity digests | Medium–high | Very high | High | LSH/Bloom | High | Engineering time | Strong |
| PS5 privacy | High | Medium–high | High | Privacy metrics | Medium | Formal depth | Strong, hot |
| PS6 pollution | Medium–high | Medium | Medium | Light | Medium | Needs embeddings | Best as add-on |
| PS7 DASH substitution | Medium | High | High (native DASH) | Light | High | Video pipeline | Medium–strong |
| PS8 tiered LLM | High (hot) | Medium | Medium | Optimisation | Medium | Overlap with 2602.13165 | Strong with PhDs |

## 8. Bundles

- **B1 — safe and syllabus-heavy:** Phase 0 + PS1 + PS2.
- **B2 — strongest research story (my pick):** Phase 0 + **PS3** as the core. Use PS1-V1 as a comparator and PS2's congestion/AQM/GE-loss setups as the evaluation scenarios; PS6 as a stretch.
- **B3 — hottest topic:** Phase 0 + PS8 + PS5.

Why B2: it is one thesis. *"The paper's global α is position-blind, load-blind, and causes
heavy-tailed retries. We derive the delay-optimal local rule from the paper's own equations,
learn its inputs from signals already in the network (retries; TCP's RTT estimator), and show on
NeST that it beats every static α under congestion and burst loss."* Every component traces to a
specific equation, it uses NeST's distinctive features, and §2.2(c) gives a memorable opening
slide.

## 9. Decisions for us

1. Which bundle, or which mix?
2. Real similarity data: images (ORB like the paper, or CNN embeddings) or sentences (QQP — needed for PS6 and PS8)?
3. Theory–system balance (is the replacement-aware add-on worth it)?
4. Split between partners, and the Progress Report I deadline.
5. Repo name for the report title page (template placeholder: `somyaknotfound/cn_project`).
