#!/usr/bin/env bash
# Stage 3 sweep: blind vs threshold vs exclusion on the base paper's tandem (result 3).
# Run inside the container: JOBS=10 bash /work/build/experiments/stage3_sweep.sh
# Uses the debug build: the optimized build segfaults when our strategy answers (design_notes).
set -euo pipefail

BIN=/root/scenario-build/tandem
RAW=/work/research/results/raw/stage3
JOBS="${JOBS:-10}"
mkdir -p "$RAW"

jobs_file=$(mktemp)
# heavy cells (blind, high alpha, contents persisting across retries) first, so workers stay busy
for policy in blind exclusion threshold; do
  for alpha in 1.0 0.9 0.5 0.1; do
    for s in 1.0 0.95 0.9; do
      for placement in per-request static; do
        requests=1000
        # as in Stage 2; at alpha = 1, s = 1 about 90% of blind requests run to the 5000-attempt cap
        if [ "$policy" = blind ] && { [ "$alpha" = 0.9 ] || [ "$alpha" = 1.0 ]; }; then
          requests=200
        fi
        for run in 1 2 3 4 5; do
          name="${policy}_a${alpha}_s${s}_${placement}_r${run}"
          echo "$BIN --policy=$policy --alpha=$alpha --threshold=$s --placement=$placement --requests=$requests --run=$run --out=$RAW/$name.csv" >> "$jobs_file"
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
