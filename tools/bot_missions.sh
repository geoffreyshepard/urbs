#!/usr/bin/env bash
# Runs the autoplay bot on every campaign mission in parallel and prints one summary line each.
# Usage: tools/bot_missions.sh [days=200] [difficulty=1] [build=release]
cd "$(dirname "$0")/.."
DAYS=${1:-200}; DIFF=${2:-1}; BUILD=${3:-release}
TICKS=$((DAYS * 1200))
OUT=$(mktemp -d)
for s in data/scenarios/m*/scenario.json; do
  m=$(basename "$(dirname "$s")")
  ./build/$BUILD/urbs_headless --scenario "$s" --ticks $TICKS --autoplay --difficulty "$DIFF" > "$OUT/$m.txt" 2>&1 &
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
