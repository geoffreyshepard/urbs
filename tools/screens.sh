#!/usr/bin/env bash
# Game smoke test: renders every screen, every panel and a played city in screenshot mode and checks
# that the game exits cleanly and writes a non-blank image each time. Opens a window per shot.
# Usage: tools/screens.sh [out_dir]   (release build; builds nothing). See docs/testing.md.
cd "$(dirname "$0")/.."
GAME=./build/release/urbs.app/Contents/MacOS/urbs
[ -x "$GAME" ] || GAME=./build/release/urbs
[ -x "$GAME" ] || { echo "build the release preset first"; exit 2; }
OUT=${1:-$(mktemp -d)}
mkdir -p "$OUT"
failed=0

shot() {  # name, args...
  local name=$1; shift
  local png="$OUT/$name.png"
  rm -f "$png"
  "$GAME" --mute --screenshot "$png" "$@" > "$OUT/$name.log" 2>&1
  local rc=$?
  # A blank or missing frame compresses to almost nothing; a rendered one is far larger.
  local size=$(stat -f %z "$png" 2>/dev/null || stat -c %s "$png" 2>/dev/null || echo 0)
  if [ "$rc" != 0 ] || [ "$size" -lt 50000 ] || grep -q '\[error\]' "$OUT/$name.log"; then
    failed=$((failed + 1))
    printf "FAIL %-22s rc=%s png=%s bytes\n" "$name" "$rc" "$size"
    grep -E '\[error\]|Assert|abort' "$OUT/$name.log" | head -5 | sed 's/^/     /'
  else
    printf "ok   %-22s %s bytes\n" "$name" "$size"
  fi
}

# Menus and the screens reached from them.
shot menu --frames 30
for s in campaign mods custom editor; do shot "screen_$s" --screen "$s" --frames 30; done

# A played city: the bot builds Rome for 20 days, then each panel opens over it.
CITY=(--scenario data/scenarios/m16_roma/scenario.json --autoplay --ticks 24000 --frames 20)
shot city "${CITY[@]}" --show nomsg
for p in rome advisors finance goods trade labor religion military research alerts help pause settings load save; do
  shot "panel_$p" "${CITY[@]}" --show "nomsg,$p"
done
for z in 0 2; do shot "zoom_$z" "${CITY[@]}" --show nomsg --zoom "$z"; done
shot night "${CITY[@]}" --show nomsg --hour 22
shot inspect "${CITY[@]}" --show nomsg,bld:forum

# A battle (units, effects) and the tutorial mission with its first message.
shot battle --scenario data/scenarios/m13_colonia/scenario.json --autoplay --ticks 40000 --frames 20 --show nomsg
shot tutorial --scenario data/scenarios/m01_latium/scenario.json --frames 20

echo "screens: $failed failed (images in $OUT)"
[ "$failed" = 0 ]
