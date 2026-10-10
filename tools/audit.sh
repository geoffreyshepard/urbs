#!/usr/bin/env bash
# Simulation audit: deeper bot-driven tests than ctest. See docs/testing.md.
# Usage: tools/audit.sh [part...]   parts: missions chaos soak sandbox (default: all)
# Builds nothing: build the release preset first (a failed build would silently test an old binary).
# Prints one line per run and "audit: N failed" at the end; exit status 1 if anything failed.
cd "$(dirname "$0")/.."
BIN=./build/release/urbs_headless
[ -x "$BIN" ] || { echo "build the release preset first"; exit 2; }
PARTS=${*:-missions chaos soak sandbox}
OUT=$(mktemp -d)
JOBS=${AUDIT_JOBS:-8}
failed=0

run() {  # name, args...
  local name=$1; shift
  ( "$BIN" "$@" > "$OUT/$name.txt" 2>&1; echo $? > "$OUT/$name.rc" ) &
  while [ "$(jobs -r | wc -l)" -ge "$JOBS" ]; do sleep 0.2; done
}

report() {  # name, what must appear (regex, optional)
  local name=$1 want=$2 rc
  rc=$(cat "$OUT/$name.rc")
  local f="$OUT/$name.txt"
  local line
  line="$(grep -h '^autoplay:' "$f" | sed 's/autoplay: [0-9]* actions, //' | cut -c1-60) $(grep -h '^invariants\|^roundtrip\|^chaos' "$f" | tr '\n' ' ')"
  if [ "$rc" != 0 ] || { [ -n "$want" ] && ! grep -Eq "$want" "$f"; }; then
    failed=$((failed + 1))
    printf "FAIL %-28s rc=%s %s\n" "$name" "$rc" "$line"
    grep -E '^CHECK|^  |^ROUNDTRIP|error|Assertion|FAIL' "$f" | head -8 | sed 's/^/     /'
    cp "$f" "/tmp/urbs_audit_$name.txt"
  else
    printf "ok   %-28s %s\n" "$name" "$line"
  fi
}

names=()
for part in $PARTS; do
  case $part in
    missions)
      # Every campaign mission with the bot: daily consistency checks, a save/reload every 25 days.
      for s in data/scenarios/m*/scenario.json; do
        m=$(basename "$(dirname "$s")")
        run "mission_$m" --scenario "$s" --ticks 360000 --autoplay --check --roundtrip 25
        names+=("mission_$m|mission WON")
      done ;;
    chaos)
      # The bot builds a city while a chaos player builds, demolishes, taxes, raises armies and calls
      # disasters at random; the world must stay consistent and never allocate in the tick.
      for m in m02_ostia m04_capua m07_aquileia m09_campania m12_alexandria m13_colonia m16_roma; do
        for seed in 1 2; do
          run "chaos_${m}_$seed" --scenario "data/scenarios/$m/scenario.json" --ticks 72000 --autoplay \
              --keep-playing --chaos 8 --check --roundtrip 15 --seed "$seed"
          names+=("chaos_${m}_$seed|")
        done
      done ;;
    soak)
      # Long play past victory: Rome for 1,000 days (about 60 game years).
      run soak_m16_roma --scenario data/scenarios/m16_roma/scenario.json --ticks 1200000 --autoplay \
          --keep-playing --check --roundtrip 100
      names+=("soak_m16_roma|")
      ;;
    sandbox)
      # Random sandbox maps (sizes, coasts, rivers, desert, Nile, regions) played by the bot.
      mkdir -p "$OUT/sandbox"
      python3 tools/sandbox_maps.py "$OUT/sandbox" 24
      for s in "$OUT"/sandbox/*.json; do
        n=$(basename "$s" .json)
        run "sandbox_$n" --scenario "$s" --ticks 240000 --autoplay --check
        names+=("sandbox_$n|mission WON")
      done ;;
    *) echo "unknown part: $part"; exit 2 ;;
  esac
done
wait
for e in "${names[@]}"; do report "${e%%|*}" "${e#*|}"; done
echo "audit: $failed failed of ${#names[@]}"
rm -rf "$OUT"
[ "$failed" = 0 ]
