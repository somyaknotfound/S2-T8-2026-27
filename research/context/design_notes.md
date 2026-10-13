# Design notes

Written in Stage 1 (5 Oct 2026). Every decision cites the source line that forces it. Paths are
relative to `/root/ndnSIM/ns-3/src/` inside the `ndnsim` container.

## Environment

- **Host:** Windows 11 with Docker Desktop 28.3.3.
- **Container:** `ndnsim`, `ubuntu:20.04` (20.04.6 LTS), 12 CPUs, ~7 GB RAM. The repo is mounted at `/work`.
- **ndnSIM 2.9 (NFD 22.02):** commit `90d50396654dabad54b6979f2dc8fa929ade544c` (5 May 2022).
- **ns-3 fork:** commit `1b3bab9b6dba5f1e616c0b286eb889e9ce3d5d59` (5 May 2022).
- **Build:** `build/env/setup_ndnsim.sh` with `JOBS=4`; log at `/root/build.log`. The debug profile built and installed with `EXIT 0`, and `ndn-simple` ran. The optimized profile (`build-opt`) installs alongside it.

### Scenario template (`build/scenario`)
The 2019 template targets ndnSIM 2.5. Four fixes were needed for ndnSIM 2.9:
1. `wscript`: `os.environ.has_key` (Python 2 only) replaced with `'PKG_CONFIG_PATH' not in os.environ`.
2. `wscript`: `--run` now uses `Context.out_dir`, because builds go to `/root/scenario-build`, not into the OneDrive folder.
3. `.waf-tools/default-compiler-flags.py`: `-std=c++14` changed to `-std=c++17`, matching the ndnSIM 2.9 build.
4. `.waf-tools/ns3.py`: package name changed from `libns3-dev-<module>-<profile>` to `libns3.35-<module>-<profile>`.

The template also ships no example, so `scenarios/smoke-ndn-simple.cpp` (a copy of ndnSIM's `ndn-simple`) is the smoke test.

To configure: `./waf configure --debug --out=/root/scenario-build` for development, or without `--debug` for the optimized libraries.
- **No old-style ContentStore:** ndnSIM 2.9 doesn't have `ns3::ndn::ContentStore` any more. `ndnSIM/model/` has no `cs/` directory; the only content store is NFD's `nfd::cs::Cs` (`ndnSIM/NFD/daemon/table/cs.*`).

## Q1 — Where does similarity lookup live?

**Decision:** in our own forwarding strategy, `SimilarityStrategy`, a subclass of `BestRouteStrategy`.
- It holds its own `SimilarityStore` (item ids + replacement policy). The strategy is instantiated per forwarder, so each node has its own store.
- NFD and ndnSIM sources are not modified.

Evidence:
- NFD's Content Store matches by exact name, and the forwarder consults it *before* the strategy, only when no other request for the same name is pending (`ndnSIM/NFD/daemon/fw/forwarder.cpp:160-167`). It can't do nearest-neighbour lookup.
- On a Content Store miss the strategy gets `afterReceiveInterest` (`ndnSIM/NFD/daemon/fw/strategy.hpp:147`). From there it can answer itself with `sendData(data, egress, pitEntry)` (`strategy.hpp:297`). That call deletes the pending request's record of where the Interest came from and closes the request when nothing else is waiting (`strategy.cpp:258-275`).
- ~~Every Data passing back toward the consumer reaches `afterReceiveData` (`strategy.hpp:229`).~~ **Corrected 8 Oct (found in S4):** this holds for upstream NFD, not for ndnSIM 2.9's fork.
  - Here `Forwarder::onIncomingData` sends every matching PIT entry through `Strategy::satisfyInterest` (`forwarder.cpp:341-347`), which calls the virtual **`beforeSatisfyInterest`**. `afterReceiveData` is never invoked.
  - A strategy that wants to see returning Data must override `beforeSatisfyInterest`. That is where the delay-aware cache measures its upstream RTT, and where on-path insertion would go.
- The Data's name must match the pending request's name (`strategy.cpp:250`). See Q2.

The three modes live in `afterReceiveInterest`:

| Mode | Rule |
|---|---|
| `blind` | Answer with the best match with probability α, else forward (the paper, including exact hits) |
| `threshold` | Answer iff best match ≥ s_min from the Interest |
| `delay-aware` | Answer iff s' ≥ F⁻¹(1 − D_up / (δ + D_retry)) |

**To confirm while implementing:** that StrategyChoice creates exactly one instance per node for the `/sim` prefix.

## Q2 — How does a substitute item get back past NDN name matching?

**Decision:** encapsulate it under the Interest's own name.
- **Interest name:** `/sim/<itemId>/<consumerId>/<seq>/<attempt>`. These names are unique per request, so NFD's Content Store never hits on them.
- **Interest ApplicationParameters** (`ndnSIM/ndn-cxx/ndn-cxx/interest.hpp:289-375`) carry `s_min`, the retry bound, and the exclusion bitmap of hops already tried. Setting ApplicationParameters appends a ParametersSha256Digest name component, so the Data must use the **full** Interest name.
- **Data name** = the full Interest name. **Data content** = `{servedItemId, similarity, servingHop, servingNodeId}`. The origin producer answers with `servedItemId = itemId` and `similarity = 1`.
- **NFD Content Store:** set to its minimum (`StackHelper::setCsSize`, `ndnSIM/helper/ndn-stack-helper.cpp:114,175`). It never hits on unique names anyway, so this only saves memory. All real caching is our `SimilarityStore`, keyed by `servedItemId`.

Evidence:
- Returning Data is matched against the PIT (the table of pending requests) via `m_pit.findAllDataMatches` (`ndnSIM/NFD/daemon/fw/forwarder.cpp:327`). Non-matching Data goes to `onDataUnsolicited` (`forwarder.cpp:330, 404`) and is not forwarded.
- `Strategy::sendData` asserts `pitEntry->getInterest().matchesData(data)` (`ndnSIM/NFD/daemon/fw/strategy.cpp:250`).

For the report:
- NDN's matching semantics are unchanged; the "similar answer" is application-level payload.
- **Limitation:** the original producer's signature on the substitute isn't carried. We could embed the original Data's wire encoding if an examiner pushes on this.

### As implemented in Stage 3 (5 Oct)
- **ApplicationParameters worked, so no name-component fallback.** The text is `smin=<s>;excl=<node>,<node>` (`EncodeParams`/`ParseParams` in `sim-common.hpp`). It is set through the `ConstBufferPtr` overload, because the `(ptr, len)` overload is deprecated in this ndn-cxx (`interest.hpp:344-352`).
  - The digest component makes the name six components long. The strategy's `sendData` assertion still holds, because `MakeAnswerData` uses the full Interest name.
  - The exclusion list holds the node IDs that served rejected answers, not a hop bitmap.
  - The retry bound stays the consumer's `--maxAttempts`; it is not on the wire.
- **Blind Interests carry no parameters.** A blind cache never learns s, which is the paper's model. It also keeps blind Interests byte-identical to Stage 2.
  - That matters because delays include serialisation at 1 Gbps (one hop = 2.000864 ms), so ~40 extra bytes would shift every delay by microseconds.
  - Verified: `--policy=blind` replays three S2 runs (2,200 rows) identically on every S2 column.
- **`ShouldAnswer` draws exactly one α-coin per call under every policy**, so the controller's RNG stream is consumed the same way regardless of policy.
- **Cache and consumer share `Acceptable(similarity, sMin)`** (tolerance 1e-9). A threshold-mode answer is therefore never rejected, which is why there is exactly 1 attempt.
- **`interest_hops` counts answered attempts only.** A timed-out attempt's reach is unknown. In S3 (no loss) nothing times out; S4 must revisit this.
- **Analytical expectations are exact, not Monte Carlo.** Each cache is an independent uniform B-subset, so for item c the flags "cache i holds an item with similarity ≥ s" are i.i.d. Bernoulli(p_c). Every policy's delay depends on a configuration only through those flags, so `persistent_expectation` enumerates all 2^m patterns.
  - For blind, the result matches S2's 800-configuration Monte Carlo within its noise (α=0.9, s=1: 13,718 vs 13,709 ms).

## Q3 — Can RED/CoDel act on NDN traffic?

**Decision:** no. Congestion comes from the point-to-point device's drop-tail transmit queue, sized with `p2p.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", ...)` (`ndnSIM/utils/topology/annotated-topology-reader.cpp:352`). The report makes no AQM claims. **Stretch only:** a custom `ns3::Queue<Packet>` subclass implementing CoDel at the device level.

Evidence:
- `NetDeviceTransport::doSend` calls `m_netDevice->Send()` directly (`ndnSIM/model/ndn-net-device-transport.cpp:121`). This bypasses ns-3's TrafficControlLayer, which is where `RedQueueDisc` and CoDel queue discs live (`traffic-control/model/`).
- The bundled `ndnSIM/examples/ndn-grid-topo-plugin-red-queues.cpp` asks for `ns3::RedQueue` as a *device* queue (`examples/topologies/topo-grid-3x3-red-queues.txt`). This ns-3 fork has only `drop-tail-queue` in `network/utils/`.
- **Verified (5 Oct):** running that example aborts at t=0 with `Invalid value for attribute set (MaxSize) on ns3::DropTailQueue<Packet>` (`object-factory.cc:84`) and dumps core. There is no working RED path for NDN in this ndnSIM.

## Known issue — the optimized build segfaults when our strategy answers

**Symptom:** with the optimized ndnSIM libraries, `tandem` segfaults the first time `SimilarityStrategy` calls `sendData()`.
- Valgrind reports an invalid read inside the library's own `nfd::fw::Strategy::sendData` (`strategy.cpp:252`), on a `shared_ptr` release.
- The same code against the **debug** libraries is Valgrind-clean (0 errors, `--requests=3`, α = 1). It also reproduces the model (see STATUS).

**Ruled out:**
- `NDEBUG` / `NS3_LOG_ENABLE` / `NS3_ASSERT_ENABLE` mismatch: removed, still crashes.
- `-O3 -march=native` mismatch: matched, still crashes.
- Differing generated headers: `build/ns3` and NFD/ndn-cxx `config.hpp` are identical between profiles.
- Re-entrancy: answering via `Simulator::ScheduleNow` still crashes.

**What it depends on:**
- With α = 0 the optimized binary runs fine. Our consumer, producer and forwarding all work; only *our strategy answering* crashes.
- That is also the only path where a Data packet built in our translation unit is delivered straight to a local app face, through `AppLinkService::doSendData` → `data.shared_from_this()`. A layout or ABI mismatch for `ndn::Data` across the library boundary is the leading hypothesis. It is unconfirmed.

**Decision:** run experiments on the debug build; parallel runs on 12 cores are fast enough. Revisit only if sweep time becomes a problem.

## ndn-cxx scheduler leak — patched in ndnSIM (7 Oct)

This is the only change made to ndnSIM itself. It lives in `build/env/ndn-cxx-scheduler-leak.patch`, and `setup_ndnsim.sh` applies it once after cloning.

**Symptom:** the S3 sweep's heaviest runs (blind, α=1, s=1, ~900k attempts) were still going after 52 min of CPU each.
- Measured scaling was super-linear: 10k / 20k / 40k / 80k attempts took 7.9 / 16.2 / 45.1 / 138.7 s.
- gdb stack samples were almost all in ns-3's `MapScheduler` (`std::map` insert/erase), called from `ndn::scheduler::Scheduler::scheduleNext`.

**Cause:** ndnSIM's port of the ndn-cxx scheduler (`ndn-cxx/ndn-cxx/util/scheduler.cpp`) leaks ns-3 events.
- `schedule()` calls `scheduleNext()` whenever a new event becomes the earliest. `scheduleNext()` overwrites `m_timerEvent` with a new `ns3::Simulator::Schedule` without removing the old one. Real ndn-cxx uses an asio timer, whose `expires_from_now()` replaces the pending wait.
- The orphaned event later fires `executeEvent()`, which finds nothing due and, on scope exit, reschedules itself for the current head. Orphans therefore never die, and each one fires once per timer expiry.
- NFD creates a new earliest timer for every satisfied PIT entry (a 0 ms expiry), i.e. about once per attempt. Work per simulated second therefore grows with everything simulated before it.

**Fix:** `scheduleNext()` first does `Simulator::Remove` on the pending `m_timerEvent`, as `cancelImpl()` already does.

**Results are unaffected; only speed changes.** Callbacks always ran once, at the right time; the orphans only re-armed the timer.
- 216 runs (all 72 S3 cells × 3 runs, 50-attempt cap) are byte-identical before and after the patch.
- Three S2 runs (2,200 rows) still replay exactly, so S2 results stand as published.
- 80k attempts now take 19.3 s instead of 138.7 s, with near-linear scaling.
- The rebuilt scenario binary is byte-identical (md5 `0430bfc5…`), because ndnSIM links dynamically.

## Delay-aware cache — as implemented in Stage 4 (8 Oct)

- **Where the state lives:**
  - `DelayAwareState` (`extensions/delay-aware.{hpp,cpp}`), one per cache, is owned by the controller (keyed by node id) so it can be logged.
  - The strategy feeds it from three hooks:
    - `afterReceiveInterest`: decide; record the forward.
    - `beforeSatisfyInterest`: an upstream Data arrived. Not `afterReceiveData`, which this NFD fork never calls; see Q1.
    - The forwarder's `beforeExpirePendingInterest` signal: a forwarded Interest died unsatisfied.
- **D_up** is the SRTT (gain 1/8) of forwarded /sim Interests. Expiries count as samples equal to the time the client lost.
  - It averages near answers (an upstream cache) and far ones (the origin over the backhaul). That is the right quantity for an expected-cost rule.
  - It is why v5, whose forwards all cross the backhaul, has the highest D_up under load.
  - It measures the first answer only, so a rejection further upstream is not in it. This is an approximation.
- **RTT_h** = h × the smallest per-hop RTT seen. h comes from HopLimit (consumers set 32 and NFD decrements at every node, `forwarder.cpp:104-113`). The per-hop RTT comes from the HopLimit the server echoes in the Data payload.
  - The minimum filter assumes the client-to-cache segment is uncongested, which is true here: only the backhaul is a bottleneck.
- **F is isotonic.** Pool-adjacent-violators runs over the Laplace-smoothed per-bin rates, weighted by observations.
  - Whoever accepts s′ accepts anything closer, so F must be non-decreasing.
  - Without the fit, rarely seen low bins kept the 1/2 prior. Once D_up > δ the rule then answered items no user accepts (s* = 0 at three caches under load).
- **Exploration is confidence-based:** answer anyway only while the rule's verdict differs at F ± 2·SE (SE from the pooled weight).
  - A fixed 30-samples-per-bin budget was tried first: rejections were still 6.6% of requests after 1,750.
  - Permanent ε-greedy would never converge to the oracle.
  - With s = 0.95 for every user, delay-aware equals the oracle after warm-up: 1.16 vs 1.17 ms, no rejections.
- **Feedback** reuses the S3 `excl` list:
  - a retry naming this cache is a rejection;
  - no retry within 1 s is an acceptance;
  - a retry *not* naming it means the answer was lost, which is not counted.
- **Known weakness (found 12 Oct in the traces, not fixed):** the isotonic fit pools every bin below 0.90 with the [0.90, 0.95) bin.
  - Why: a best match below 0.90 is rare, so those bins have almost no observations. Their 1/2 prior sits above the bin's fitted value, and pool-adjacent-violators merges them. Logged example (v5, 0.95 load): F0…F18 = 0.205, F19 = 0.38.
  - Effect: when D_up is high enough that the [0.90, 0.95) bin is worth answering, every lower bin is too, so `Threshold()` returns 0 and the cache answers whatever it holds, including items no user in {0.9, 0.95, 1.0} accepts.
  - How often: `research/results/stage4/convergence.md`, last table. Never up to 0.8 load; 5–20% of snapshots at 0.95.
  - A fix would be to treat bins with no observations of their own as F = 0. That changes the algorithm, so S4 would have to be re-run.
- **Wire format:** delay-aware Interests carry `excl=` but no `smin` (the cache never learns the user's threshold), plus a HopLimit.
  - The other policies' packets are unchanged: S2/S3 runs still replay byte-for-byte.

## Loss

**Decision:** burst loss through a custom `GilbertElliottErrorModel`, a subclass of `ns3::ErrorModel` with p_gb, p_bg and per-state loss as ns-3 attributes.
- It plugs into the device's `ReceiveErrorModel` the same way.
- This also lets us reuse last semester's Gilbert–Elliott parameters.

Evidence:
- NDN traffic passes through the point-to-point device, so device-level error models apply.
- ndnSIM's topology reader already attaches an `ErrorModel` per link via `ReceiveErrorModel` (`ndnSIM/utils/topology/annotated-topology-reader.cpp:398-429`; attribute defined at `point-to-point/model/point-to-point-net-device.cc:60`).
