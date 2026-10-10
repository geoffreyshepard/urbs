#!/usr/bin/env bash
# Runs the autoplay bot on every campaign mission in parallel and prints one summary line each.
# Usage: tools/bot_missions.sh [days=200] [difficulty=1] [seeds=1] [build=release]
# With several seeds each mission runs once per simulation seed (same map) and wins are counted per run.
cd "$(dirname "$0")/.."
DAYS=${1:-200}; DIFF=${2:-1}; SEEDS=${3:-1}; BUILD=${4:-release}
TICKS=$((DAYS * 1200))
OUT=$(mktemp -d)
for k in $(seq 1 "$SEEDS"); do
  for s in data/scenarios/m*/scenario.json; do
    m=$(basename "$(dirname "$s")")
    seedarg=""; [ "$k" -gt 1 ] && seedarg="--seed $((k * 7919))"
    ./build/$BUILD/urbs_headless --scenario "$s" --ticks $TICKS --autoplay --difficulty "$DIFF" $seedarg > "$OUT/${m}_s$k.txt" 2>&1 &
    while [ "$(jobs -r | wc -l)" -ge 8 ]; do sleep 0.2; done
  done
done
wait
won=0
for f in "$OUT"/*.txt; do
  m=$(basename "$f" .txt)
  a=$(grep '^autoplay' "$f" | sed 's/autoplay: [0-9]* actions, //')
  g=$(grep '^goals' "$f" | sed 's/goals: //')
  printf "%-20s %s | %s\n" "$m" "$a" "$g"
  grep -q "mission WON" "$f" && won=$((won + 1))
done
echo "won $won / $(ls "$OUT"/*.txt | wc -l | tr -d ' ')"
rm -rf "$OUT"
