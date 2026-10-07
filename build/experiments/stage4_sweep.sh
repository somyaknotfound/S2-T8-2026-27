#!/usr/bin/env bash
# Stage 4 sweep: congestion and burst loss on the backhaul, delay-aware vs fixed-alpha caching.
# Run inside the container: JOBS=10 bash /work/build/experiments/stage4_sweep.sh
# Uses the debug build: the optimized build segfaults when our strategy answers (design_notes).
#
# Scenarios (all per-request placement, 2000 requests, the first 500 are warm-up in the analysis):
#   load  : mixed users (s from 0.9/0.95/1.0), cross traffic at offered backhaul load 0..0.95
#   loss  : mixed users, Gilbert-Elliott loss on every link (mean burst 3.3 packets), no cross traffic
#   fixed : every user s = 0.95, at load 0 and 0.9: delay-aware should match the oracle
# Blind alpha = 0.9 is left out: its heavy tail (S2/S3) stretches a run over hours of simulated
# time, which cross traffic turns into tens of millions of packet events.
set -euo pipefail

BIN=/root/scenario-build/tandem
RAW=/work/research/results/raw/stage4
JOBS="${JOBS:-10}"
COMMON="--placement=per-request --requests=2000 --bottleneckRate=2Mbps --queuePackets=100 --interestLifetimeMs=200 --nodeLogEvery=100"
MIXED="--thresholds=0.9,0.95,1.0"
# cross Data is 1075 B on the wire: offered load 0.3/0.6/0.8/0.9/0.95 of 2 Mbps
LOADS="0 70 140 186 209 221"
# mean loss (percent):p_gb with p_bg = 0.3, p_gb = L * 0.3 / (1 - L)
LOSSES="1:0.0030303 5:0.0157895 10:0.0333333"
POLICIES=("delay-aware --policy=delay-aware"
          "threshold-a1 --policy=threshold --alpha=1"
          "blind-a0.5 --policy=blind --alpha=0.5")
for a in 0.1 0.3 0.5 0.7 0.9 1.0; do
  POLICIES+=("exclusion-a$a --policy=exclusion --alpha=$a")
done

for scenario in load loss fixed; do
  mkdir -p "$RAW/$scenario/req" "$RAW/$scenario/run" "$RAW/$scenario/nodes"
done

job() { # scenario point label flags...
  local scenario=$1 point=$2 label=$3 run=$4
  shift 4
  local name="${label}_x${point}_r${run}"
  local trace=""
  if [ "$label" = delay-aware ]; then
    trace="--nodeOut=$RAW/$scenario/nodes/$name.csv"
  fi
  echo "$BIN $COMMON $* --run=$run --out=$RAW/$scenario/req/$name.csv --runOut=$RAW/$scenario/run/$name.csv $trace"
}

jobs_file=$(mktemp)
for run in 1 2 3 4 5; do
  for entry in "${POLICIES[@]}"; do
    set -- $entry; label=$1; shift; flags="$*"
    for x in $LOADS; do job load "$x" "$label" "$run" $MIXED --crossRate="$x" $flags >> "$jobs_file"; done
    for entry_loss in $LOSSES; do
      job loss "${entry_loss%%:*}" "$label" "$run" $MIXED --lossPgb="${entry_loss#*:}" --lossPbg=0.3 $flags >> "$jobs_file"
    done
    case $label in blind-*) ;; *)
      for x in 0 209; do job fixed "$x" "$label" "$run" --threshold=0.95 --crossRate="$x" $flags >> "$jobs_file"; done ;;
    esac
  done
done

echo "$(wc -l < "$jobs_file") runs, $JOBS in parallel"
xargs -P "$JOBS" -I{} bash -c '{} > /dev/null 2>&1 || echo "FAILED: {}"' < "$jobs_file"
rm -f "$jobs_file"

# merge each scenario's per-run files: one header, then every run's rows
for scenario in load loss fixed; do
  for kind in req run nodes; do
    runs=("$RAW/$scenario/$kind"/*_r[0-9]*.csv)
    merged="$RAW/$scenario/$kind.csv"
    head -1 "${runs[0]}" > "$merged"
    for f in "${runs[@]}"; do
      tail -n +2 "$f" >> "$merged"
    done
    echo "$scenario/$kind: $(($(wc -l < "$merged") - 1)) rows"
  done
done
echo "SWEEP_DONE"
