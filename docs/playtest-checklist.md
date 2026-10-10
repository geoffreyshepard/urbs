# Playtest checklist

The autoplay bot proves that every mission can be won. It cannot tell whether a mission is clear, fair or fun, or how long a person takes. This checklist is for human playtests.

## Before you start
- Play a release build: `tools/package/package_macos.sh`, or `./build/release/urbs.app/Contents/MacOS/urbs`.
- Play on Civitas (normal) unless you are testing difficulty.
- Keep a timer running, and note the real time at each milestone below.
- Write notes as you go. A one-line note in the moment beats a summary afterwards.

## Missions to play first
| Mission | Target length | Why |
|---|---|---|
| 1 Latium | 30 min | The tutorial. Is it clear what to do next at every step? |
| 2 Ostia | 45 min | First trade route and first request from Rome. On Imperium the bot once lost here to failed requests. |
| 4 Capua | 75 min | First production chains with several steps, and patrician housing. |
| 12 Alexandria | 4 h | The Nile flood and the lighthouse. Historically the bot's hardest mission. |
| 13 Colonia, 15 Antioch | 4.25 h, 5 h | The bot wins these only about half the time (raids, debt, requests). |

## For each mission, write down
1. **Time:** real time from start to win (or to giving up), and the speed you used most (1×, 2× or 4×).
2. **Pace:** moments with nothing to do (waiting for money or people), and moments with too much to do.
3. **Confusion:** every time you didn't know what to do next, or why something happened (an empty market, families leaving, a building with no workers). Did an advisor or alert explain it?
4. **Unfairness:** a loss or setback you couldn't see coming or couldn't answer, such as a raid, request, disaster or debt.
5. **Goals:** were the goals visible and understandable throughout? Which goal was last to be met?
6. **Bugs:** anything wrong, with the day and what you were doing. Quicksave (F5) right after a bug and keep the file.

## Things to try on purpose
- **Walls and gates** (the bot never builds them): wall in part of the town with a gate on the road and let a raid arrive. Do raiders break through the wall? Do citizens use the gate?
- **Towers, training ground and stables:** do they make a visible difference in a battle?
- **Governor's palace:** raiders head for it. Does that show?
- **Slave quarters and manumission** (mission 9): do the policies feel meaningful?
- **The goldsmith** (gold from Alexandria or Hispania), the **scriptorium** and the **sculptor**: are these worth building for export?
- **Blocking an entrance:** build something on another building's doorstep. The blocked building should find another way in. If none is left, a "has no road access" alert should appear.
- **Difficulty:** replay mission 1 or 2 on Otium and on Imperium. Is the difference noticeable?

## Reporting
Put the notes in an issue, or in `docs/milestones/playtest-<date>.md`, with any save files. Balance changes should cite them.
