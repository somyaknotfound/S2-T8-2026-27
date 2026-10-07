#!/usr/bin/env bash
# Stage 3 follow-up: runs 6-25 for every threshold and exclusion cell (gate 1 re-test).
# The 5-run sweep missed one threshold cell out of 24, which is likely at 95% per cell. These
# runs re-test every gated cell with 25 runs, not just the failed one.
# Run inside the container: JOBS=10 bash /work/build/experiments/stage3_followup.sh
set -euo pipefail

BIN=/root/scenario-build/tandem
RAW=/work/research/results/raw/stage3_followup
JOBS="${JOBS:-10}"
mkdir -p "$RAW"

jobs_file=$(mktemp)
for policy in exclusion threshold; do
  for alpha in 1.0 0.9 0.5 0.1; do
    for s in 1.0 0.95 0.9; do
      for placement in per-request static; do
        for run in $(seq 6 25); do
          name="${policy}_a${alpha}_s${s}_${placement}_r${run}"
          echo "$BIN --policy=$policy --alpha=$alpha --threshold=$s --placement=$placement --requests=1000 --run=$run --out=$RAW/$name.csv" >> "$jobs_file"
        done
      done
    done
  done
done

echo "$(wc -l < "$jobs_file") runs, $JOBS in parallel"
xargs -P "$JOBS" -I{} bash -c '{} > /dev/null 2>&1 || echo "FAILED: {}"' < "$jobs_file"
rm -f "$jobs_file"

# merge: one header, then every run's rows
merged="$RAW/all.csv"
# a glob, not `ls | head`: head closing the pipe early kills ls with SIGPIPE under pipefail
runs=("$RAW"/*_r[0-9]*.csv)
head -1 "${runs[0]}" > "$merged"
for f in "${runs[@]}"; do
  tail -n +2 "$f" >> "$merged"
done
echo "rows: $(($(wc -l < "$merged") - 1))"
echo "SWEEP_DONE"
