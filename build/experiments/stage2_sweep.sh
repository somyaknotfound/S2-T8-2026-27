#!/usr/bin/env bash
# Stage 2 sweep: blind similarity caching on the base paper's tandem (results 1 and 2).
# Run inside the container: JOBS=10 bash /work/build/experiments/stage2_sweep.sh
# Uses the debug build: the optimized build segfaults when our strategy answers (design_notes).
set -euo pipefail

BIN=/root/scenario-build/tandem
RAW=/work/research/results/raw/stage2
JOBS="${JOBS:-10}"
mkdir -p "$RAW"

jobs_file=$(mktemp)
# heavy cells (alpha 0.9, contents persisting across retries) first, so workers stay busy
for placement in per-request per-attempt; do
  for alpha in 0.9 0.5 0.1; do
    for s in 1.0 0.99 0.95 0.9; do
      requests=1000
      if [ "$placement" = per-request ] && [ "$alpha" = 0.9 ]; then
        requests=200
      fi
      for run in 1 2 3 4 5; do
        name="a${alpha}_s${s}_${placement}_r${run}"
        echo "$BIN --alpha=$alpha --threshold=$s --placement=$placement --requests=$requests --run=$run --out=$RAW/$name.csv" >> "$jobs_file"
      done
    done
  done
done

echo "$(wc -l < "$jobs_file") runs, $JOBS in parallel"
xargs -P "$JOBS" -I{} bash -c '{} > /dev/null 2>&1 || echo "FAILED: {}"' < "$jobs_file"
rm -f "$jobs_file"

# merge: one header, then every run's rows
merged="$RAW/all.csv"
first=$(ls "$RAW"/a*_r*.csv | head -1)
head -1 "$first" > "$merged"
for f in "$RAW"/a*_r*.csv; do
  tail -n +2 "$f" >> "$merged"
done
echo "rows: $(($(wc -l < "$merged") - 1))"
echo "SWEEP_DONE"
