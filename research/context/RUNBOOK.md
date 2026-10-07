# Runbook — exact commands, and pitfalls already hit

Read this before running anything. Every pitfall below cost real time on 5 Oct 2026.

## Container
```powershell
# Windows host, PowerShell, from the repo root (Docker Desktop must be running)
docker start ndnsim          # if it already exists
docker run -dit --name ndnsim -v "${PWD}:/work" -w /work ubuntu:20.04 bash   # first time only
```
- A fresh container needs ndnSIM built: `docker exec -d ndnsim bash -lc 'JOBS=4 bash /work/build/env/setup_ndnsim.sh > /root/build.log 2>&1; echo "EXIT $?" >> /root/build.log'`. That builds and installs the debug profile, then the optimized profile, in about 60–80 minutes.
- **From Git Bash, always `export MSYS_NO_PATHCONV=1` first**, or `/root/...` turns into `C:/Program Files/Git/root/...`.
- PowerShell mangles double quotes nested inside `bash -c '...'`. Use Git Bash with `MSYS_NO_PATHCONV=1` for anything non-trivial.

## Build our scenarios (debug — the only build that works for experiments)
```bash
docker exec ndnsim bash -c 'cd /work/build/scenario && ./waf configure --debug --out=/root/scenario-build && ./waf -j4'
# binaries: /root/scenario-build/tandem, /root/scenario-build/smoke-ndn-simple
```
- Check warnings in **our** files only. ns-3's headers spam `-Wpedantic`:
  `grep -cE "(extensions|scenarios)/[^ ]+:[0-9]+:[0-9]+: (warning|error)" <buildlog>`. It must print 0.
- The optimized profile (`WAFLOCK=.lock-waf_opt ./waf configure --out=/root/scenario-build-opt`) **builds, but segfaults when our strategy answers.** See design_notes "Known issue". Don't use it for results.

## Run
```bash
docker exec ndnsim bash -c '/root/scenario-build/tandem --policy=threshold --alpha=0.5 --threshold=1.0 --placement=per-request --requests=1000 --run=1 --out=/root/x.csv'
```
- Flags: `--policy=blind|threshold|exclusion --alpha --threshold --placement=per-attempt|per-request|static --catalog --cacheSize --caches --requests --maxAttempts --deltaMs --linkDelayMs --linkRate --run --out`. The default policy is `blind`, which replays Stage 2 exactly.
- CSV columns: `run,policy,alpha,threshold,placement,catalog,cache_size,delta_ms,request,client_node,client_pos,item,attempts,timeouts,delay_ms,served_node,served_hop,interest_hops,similarity,censored`. `interest_hops` = links climbed by all answered attempts of the request.

## Sweeps and analysis
- Sweep driver pattern: `build/experiments/stage2_sweep.sh`.
  - It builds a job list with the heavy cells first, runs `xargs -P 10`, then merges into `research/results/raw/<stage>/all.csv` (gitignored).
  - Launch it detached, logging to `/root/<stage>.log` with an `EXIT $?` line.
- Analysis pattern: `build/analysis/stage2_summary.py`.
  - Mean ± 95% CI is computed over **runs** (t-interval, 5 runs → t = 2.776), compared against the capped analytical value.
  - It writes `summary.csv` and `analysis.md` into `research/results/<stage>/` and figures into `submission/figures/`.
- The analytical model is `build/retry_correlation_check.py`; it regenerates `research/results/retry_correlation.txt`. Its capped columns use `CAP = 5000`, the same as the simulator's `--maxAttempts`.
- **Stage 3:**
  - `build/experiments/stage3_sweep.sh` runs 360 runs in about 18 min with `JOBS=10`, on the patched ndnSIM. Blind cells with α ≥ 0.9 get 200 requests; at α=1, s=1 about 90% of blind requests run to the cap.
  - `build/experiments/stage3_followup.sh` then adds runs 6–25 for every threshold and exclusion cell (960 runs, about 15 min). This is the gate-1 re-test.
  - `python build/analysis/stage3_summary.py` imports the model directly.
  - `python build/retry_correlation_check.py --stage3 > research/results/stage3/model.txt` regenerates the expectations table (exact, takes about 1 s).
- **Stage 4:**
  - `build/experiments/stage4_sweep.sh` runs 485 runs (load, loss and fixed-user scenarios) into `research/results/raw/stage4/<scenario>/{req,run,nodes}.csv`.
  - Then run `python build/analysis/stage4_summary.py`.
  - New `tandem` flags:
    - load: `--bottleneckRate` (v5–origin link only), `--queuePackets`, `--crossRate`, `--crossPayload`;
    - loss: `--lossPgb`, `--lossPbg`;
    - requests: `--interestLifetimeMs`, `--thresholds=0.9,0.95,1.0` (mixed users);
    - outputs: `--runOut` (network counters) and `--nodeOut` (delay-aware learning trace);
    - learner: `--aware*`.
  - Calibration (8 Oct):
    - Cross-traffic Data is **1075 B** on the wire, so offered backhaul load = crossRate × 1075 × 8 / 2 Mbps.
    - Our own traffic adds about 0.045 utilisation.
    - At 0.9 load the 1 KB RTT from v1 to the origin is 35 ms, and our maximum delay is 136 ms. Hence the 200 ms Interest lifetime.
  - `ge-check --pgb --pbg --packets` checks the Gilbert–Elliott model against its closed forms (loss and mean burst).

## Pitfalls
| Pitfall | Fix |
|---|---|
| Watching a background job by `grep -l pattern /proc/*/cmdline` falsely reports "process gone" (vanishing PIDs make grep exit 2) | `grep -qsa pattern /proc/[0-9]*/cmdline` |
| Killing processes by matching command text also kills the shell running the loop (its own text matches) | Match the executable: `readlink /proc/PID/exe` |
| Editing a shell script while bash is executing it can corrupt the run | Wait for `EXIT` before editing `setup_ndnsim.sh` or a sweep script |
| Python on Windows writes cp1252 by default, so "α" crashes the write | Pass `encoding="utf-8"` on every file write; set `PYTHONIOENCODING=utf-8` when printing |
| α=0.9 with `per-request` placement needs ~10⁵ retries per request | Keep `--maxAttempts=5000`, compare with the *capped* expectation, and run fewer requests per run with more runs in parallel |
| For C=100, s=0.99 and s=1.0 are identical (similarity step = 1/99) | Report s=1.0 only, or state it |
| The scenario template's `.waf-tools/` was once gitignored by a broad `.waf*` rule | `.gitignore` now ignores only `build/scenario/.waf3-*` |
| Windows git drops the executable bit | `git update-index --chmod=+x build/scenario/waf build/env/*.sh build/experiments/*.sh` |
| Long runs slow down quadratically: the ns-3 event map fills with orphaned ndn-cxx timer events | Fixed by `build/env/ndn-cxx-scheduler-leak.patch` (design_notes). A fresh container gets it from `setup_ndnsim.sh`; check with `git -C /root/ndnSIM/ns-3/src/ndnSIM/ndn-cxx diff --stat` |
| gdb can't attach to a running process in the container (no `CAP_SYS_PTRACE`) | Launch the run under gdb and `kill -INT` it to sample stacks |
| A kill loop matching `*sweep.sh*` killed its own shell (7 Oct, again) | Match `readlink /proc/PID/exe` only, never command text |
| A strategy's `afterReceiveData` override never runs | ndnSIM 2.9's NFD fork sends all Data through `satisfyInterest`, which calls `beforeSatisfyInterest`; override that (design_notes Q1) |
| With all tandem links at one rate, only the first link queues (the stream is smoothed after it) | Congestion uses a single slower backhaul (`--bottleneckRate`) |
| Python 3.8 in the container rejects backslashes inside f-string expressions | Use `%` formatting or a script file |
| A run's file glob also matched per-node trace files | Check for the expected column, or keep traces in their own directory |
| RED/CoDel queue discs never see NDN traffic (`NetDeviceTransport` calls `NetDevice::Send` directly) | Congestion = DropTail device queue size; make no AQM claims |
