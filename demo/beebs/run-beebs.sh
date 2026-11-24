#!/bin/sh
set -eu

DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
cd "$DIR"

BENCHMARKS="bubblesort compress dijkstra insertsort nsichneu qsort statemate tarai"

for b in $BENCHMARKS; do
  ./beebs-runner "./beebs-$b" "./beebs-$b-plain"
done

echo "[ALL PASS] penglai-spmp beebs"
