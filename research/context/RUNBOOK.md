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
docker exec ndnsim bash -c '/root/scenario-build/tandem --alpha=0.5 --threshold=1.0 --placement=per-request --requests=1000 --run=1 --out=/root/x.csv'
```
- Flags: `--alpha --threshold --placement=per-attempt|per-request|static --catalog --cacheSize --caches --requests --maxAttempts --deltaMs --linkDelayMs --linkRate --run --out`.
- CSV columns: `run,alpha,threshold,placement,catalog,cache_size,delta_ms,request,client_node,client_pos,item,attempts,timeouts,delay_ms,served_node,served_hop,similarity,censored`.

## Sweeps and analysis
- Sweep driver pattern: `build/experiments/stage2_sweep.sh`.
  - It builds a job list with the heavy cells first, runs `xargs -P 10`, then merges into `research/results/raw/<stage>/all.csv` (gitignored).
  - Launch it detached, logging to `/root/<stage>.log` with an `EXIT $?` line.
- Analysis pattern: `build/analysis/stage2_summary.py`.
  - Mean ± 95% CI is computed over **runs** (t-interval, 5 runs → t = 2.776), compared against the capped analytical value.
  - It writes `summary.csv` and `analysis.md` into `research/results/<stage>/` and figures into `submission/figures/`.
- The analytical model is `build/retry_correlation_check.py`; it regenerates `research/results/retry_correlation.txt`. Its capped columns use `CAP = 5000`, the same as the simulator's `--maxAttempts`.

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
| RED/CoDel queue discs never see NDN traffic (`NetDeviceTransport` calls `NetDevice::Send` directly) | Congestion = DropTail device queue size; make no AQM claims |
