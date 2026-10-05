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
- Every Data passing back toward the consumer reaches `afterReceiveData` (`strategy.hpp:229`). That is where on-path insertion into our store happens, when insertion is enabled.
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

## Loss

**Decision:** burst loss through a custom `GilbertElliottErrorModel`, a subclass of `ns3::ErrorModel` with p_gb, p_bg and per-state loss as ns-3 attributes.
- It plugs into the device's `ReceiveErrorModel` the same way.
- This also lets us reuse last semester's Gilbert–Elliott parameters.

Evidence:
- NDN traffic passes through the point-to-point device, so device-level error models apply.
- ndnSIM's topology reader already attaches an `ErrorModel` per link via `ReceiveErrorModel` (`ndnSIM/utils/topology/annotated-topology-reader.cpp:398-429`; attribute defined at `point-to-point/model/point-to-point-net-device.cc:60`).
