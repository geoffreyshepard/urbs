# 01 — Vision

## Pitch

Build Rome one family at a time. Found a muddy Latin hamlet, grow it into a provincial capital, and finish your career rebuilding Rome itself. Every citizen is a person with a name, a household, a job, a daily routine and their own small errands, and the city you see moving on screen is the real simulation, not decoration. You govern through buildings, policies and priorities, so a city of 10,000 never turns into micromanagement.

## Design pillars

1. **A city that is alive.** Zoom in anywhere and watch real people: a baker walking to work at dawn, a freedwoman carrying oil home from the macellum, children at school, a patrician heading to the baths in the afternoon. You can click any of them and follow their life.
2. **Scale without stress.** The player manages systems, not individuals: labor priorities, zoning, service coverage, laws, festivals. Problems surface as clear alerts and overlays with a traceable cause.
3. **Rome from village to capital.** A 16-mission campaign across the Republic and Empire. Each mission is a new city, bigger and more complex than the last, from 30-minute starters to 6-hour finales.
4. **Readable systems.** Every number has a "why". Hover over unhappiness and see that it is caused by no wine, a noisy tannery next door and a bad harvest.
5. **Stylized, not a history lecture.** Historically flavored, with gods that answer prayers and punish neglect. Authentic where it's fun, gamey where it helps.
6. **Open and moddable.** Open source, data-driven content, Lua-scripted missions and events.

## Reference games: what we take and what we leave

| Game | Take | Leave |
|---|---|---|
| Caesar III / Pharaoh | Housing evolution, desirability, ratings, requests from Rome, career ranks, mission campaign, gods | Random walker coverage (we use citizens who actually visit services) |
| Songs of Syx | Every citizen simulated, huge populations, painted room-based workplaces, physical logistics, mass battles, pixel art style and fixed camera | Steep UI learning curve |
| RimWorld | Individual stories, traits, inspectable lives | Per-pawn micromanagement, low population caps |
| Anno | Class-based consumption tiers, production chain satisfaction | Very deep chains |
| Factorio | Engineering discipline: sleeping entities, abstracted items, perf culture | — |

## Non-goals (for now)

- Multiplayer of any kind
- Controller or Steam Deck support
- 3D, rotating camera, or high-resolution art
- Deep RTS-style combat micro
- Sound effects and localization (English only). Architecture leaves room for them later. (Music is now in scope: see docs/11-music.md.)
- Mobile platforms

## Constraints

- Solo developer working with Claude Code, as an open source hobby project, with no deadline.
- Art is generated as code through Claude Code (see art pipeline), so the style must be achievable that way.
- Must run well on an Apple Silicon Mac, which sets the minimum spec.
