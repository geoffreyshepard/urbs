#!/usr/bin/env bash
# Plays the scripts in tests/play/ through the game's real UI (--play) and fails if any expectation
# fails, the game logs an [error], or it exits badly.  A script's first line may give game arguments
# ("# args: --scenario ..."); its commands are in docs/testing.md.  Needs the release build.
# Usage: tools/playtest.sh [out_dir] [script...]   (screenshots and logs land in out_dir)
set -uo pipefail
cd "$(dirname "$0")/.."
GAME=./build/release/urbs.app/Contents/MacOS/urbs
[ -x "$GAME" ] || GAME=./build/release/urbs
OUT=${1:-build/playtest}
shift || true
mkdir -p "$OUT"
scripts=("$@")
[ ${#scripts[@]} -eq 0 ] && scripts=(tests/play/*.txt)
fail=0
for s in "${scripts[@]}"; do
  name=$(basename "$s" .txt)
  args=$(sed -n '1s/^# args://p' "$s")
  user="$OUT/$name.user"
  rm -rf "$user" "$OUT/$name"
  mkdir -p "$OUT/$name"
  start=$(date +%s)
  # shellcheck disable=SC2086
  "$GAME" --play "$s" --play-out "$OUT/$name" --user-dir "$user" --mute $args > "$OUT/$name.log" 2>&1
  code=$?
  errs=$(grep -c '\[error\]' "$OUT/$name.log")
  oks=$(grep -c 'play: ok' "$OUT/$name.log")
  if [ $code -ne 0 ] || [ "$errs" -ne 0 ]; then
    echo "FAIL $name (exit $code, $errs errors, $oks checks passed, $(( $(date +%s) - start )) s)"
    grep -E '\[error\]' "$OUT/$name.log" | sed 's/^/    /' | head -10
    fail=1
  else
    echo "ok   $name ($oks checks, $(( $(date +%s) - start )) s)"
  fi
done
exit $fail
