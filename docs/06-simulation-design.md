# 06 — Simulation Design (How the Game Scales)

**Target:** 10,000 citizens + about 25,000 other entities (soldiers, enemies, animals, carts, ships, haul tasks) at 4× speed on an M1 Mac, with the sim at ≤ 8 ms per frame.

**The core idea:** *most entities, most of the time, are not doing anything that needs per-tick work.* A citizen asleep, working at a bench, or walking a known path off-screen can be represented by a few numbers and a scheduled wake-up time. We only spend CPU at **decision points** and on **visible motion**.

---

## 1. Data layout

Citizens live in a custom SoA pool, split into hot and cold data:

| Hot (touched often, packed arrays) | Cold (touched rarely) |
|---|---|
| position / path progress, current activity, LOD tier, next-wake tick, needs (value + rate + last-update tick), household id, job id | name, traits, skills, memory log, family links, life goals, history |

- IDs are `uint32` indices with generation counters; freed slots are reused via a free list.
- Buildings, events and other low-volume objects use EnTT.
- No pointers between entities; IDs only. This makes saves a memcpy of pools.

## 2. Agent LOD tiers

| Tier | When | Cost |
|---|---|---|
| **Visible-moving** | Walking, inside the camera view (+ margin) | Position advanced every tick for smooth animation |
| **Offscreen-moving** | Walking, outside the view | **No per-tick update.** Position is a function of `(path, start_tick, speed)` and is computed on demand. An arrival event is scheduled at `start_tick + path_length / speed`. |
| **Stationary** | Working, sleeping, inside a building, shopping | **No per-tick update.** The next decision is scheduled for the end of the activity. |

When the camera moves, entities entering the view are given a computed position and promoted. Demotion happens when they leave the view.

## 3. The event scheduler (timing wheel)

- A hierarchical timing wheel keyed by tick holds `(tick, entity, event_type)`.
- Each tick pops due events: arrival, activity finished, shift start, need threshold crossed.
- Popped events become **decision jobs**, processed in parallel batches with enkiTS.
- **Decision budget:** if more than N decisions are due in one tick (for example, dawn when everyone wakes), the overflow spills into the next ticks. Shift start times are jittered ±10 minutes of game time to flatten spikes.

## 4. Needs: lazy evaluation

Needs are not decremented every tick. Each need stores `value_at(last_update_tick)` and a `rate`. The current value is computed when queried. A threshold crossing ("will become hungry at tick T") is solved analytically and scheduled as an event. Rates change only when the activity changes, at which point the need is re-based.

## 5. Decisions: utility AI at decision points

At each decision point a citizen picks its next activity:
1. The **schedule template** (class × job shift × day phase) gives the default activity.
2. **Urgent needs** can override it (very hungry → eat; sick → physician).
3. **Errands** from the household queue (buy food, fetch water) fill free time.
4. **Life goals** occasionally insert actions (visit the forum to petition, look for better housing).

Scoring is a small weighted utility function over candidate activities. Candidates are limited to a handful, so a decision costs microseconds. Destinations for "nearest X" come from the service lookup (section 7), not from searching.

## 6. Pathfinding

The grid has per-tile move costs (paved road < dirt road < open ground; impassable for water and walls, with gates as conditional passage). Costs are in tenths of a tick per tile: paved road 10, dirt road 15, grass 50, forest 70, marsh 85. A game hour is 50 ticks, so a dirt road covers about 33 tiles an hour. Roads must be fast: with 3 ticks a tile, commutes took 40% of the working day and cities stalled around 2,000 people. Service ranges and patrol reach (`kPatrolReachTicks`) are measured in walking ticks.

1. **Connectivity regions:** flood-filled walkable components, updated incrementally when buildings and walls change. An unreachable destination is rejected in O(1) before any search, as Dwarf Fortress does.
   - Every building has one entrance tile on its perimeter. It prefers the south side, then roads, then tiles in the same region as the entry tile.
   - When the walkable map changes (a building placed or removed, a road laid or torn up, a wall breached), buildings whose entrance is blocked or cut off from the entry pick a new one.
   - A building left with no way in raises a "has no road access" alert. Hiring needs home and workplace in the same region, so a cut-off building can never be staffed.
2. **Hierarchical A\* (HPA\*):** 32×32 chunks with portal nodes on their borders. Search the abstract graph, then refine each chunk segment lazily, only when the agent enters that chunk (and only for visible agents; off-screen agents need only the abstract path length and timing).
3. **Path cache:** keyed by (source building entrance, destination building entrance). Commutes are extremely repetitive (home → work), so the hit rate is high. Entries are invalidated by chunk dirty flags.
4. **Flow fields:** used for military movement, mass evacuations, and the "everyone goes to the amphitheater" crowds.
5. **Async:** requests go into a queue processed on worker threads with a per-tick budget, and results are consumed next tick. While waiting, the agent stays in its current activity.

Citizens don't collide with each other; they overlap visually, like Songs of Syx and Caesar. Soldiers in combat use local separation through the spatial grid.

## 7. Spatial queries and service lookup

- **Spatial hash grid** (by chunk) for entities (combat, crime, disease spread).
- **Service maps:** for each service type, a multi-source Dijkstra over the road and walk graph from every service building gives each tile its *nearest provider* and *travel cost*. These maps are recomputed incrementally or on a slow cadence (every few game hours or on build changes, spread across ticks). They power:
  - citizen destination choice ("nearest temple of Ceres": an O(1) lookup)
  - coverage overlays shown to the player
  - housing evolution checks

## 8. Logistics and haul jobs

- Producers, warehouses and consumers publish **supply** and **demand** per good.
- A **matcher** runs per good on a staggered cadence, pairing supply with demand by proximity bucket. It creates **haul tasks** with **reservations** (reserving source quantity and destination space) so that two porters never chase the same goods.
- Idle porters and carters pull the best-scored haul task near them.
- Household shopping: the household's needs create a shopping errand; the market reserves goods for it.
- Goods in transit are attached to the carrier (rendered as a small sprite on the carrier).

## 9. Workplaces and labor

- Each workplace has job slots by skill or class. A **labor allocator** runs once per game hour: it ranks vacancies by the player's sector priorities and assigns unemployed or underemployed citizens, preferring short commutes and matching skills.
- Within one priority level, hiring is round-robin (one worker per workplace per round), so every link of a production chain gets some staff before any one workplace is full. A sector with vacancies takes workers from sectors at least two priority levels lower.
- Construction sites hire builders only once their materials are delivered or on the way.
- Workers who live within 25 tiles go home for the midday meal; the rest eat at work.
- Wages compare with the going rate in Rome (4 denarii a day): each denarius above or below moves worker happiness by 3 (capped at −12 / +9).
- Production is computed per work interval (the event-scheduled end of a work block), not per tick: `output = f(workers present × skill × tools × efficiency × inputs)`.

## 10. Combat

- Units (cohorts) are groups of individual soldiers. Unit-level orders produce flow fields or formation targets; soldiers steer locally with separation.
- Melee resolves in pairs or small groups each combat tick via the spatial grid; ranged fire uses projectile entities, pooled.
- Battles are the one place where many entities tick every tick, so combat is budgeted separately and parallelized by spatial cell.

## 11. Slow systems (staggered)

These run at low frequency and are spread across ticks so they never spike a frame:

| System | Cadence |
|---|---|
| Desirability field | On change, incremental |
| Service maps | Every few game hours, chunked |
| Housing evolution checks | Each house every game day, staggered |
| Happiness recompute | Per citizen per game day, staggered |
| Immigration / emigration | Per game day |
| Market prices / trade | Per game day |
| Gods' moods, ratings, Favor | Per game day / month |
| Births, aging, deaths | Per game day |
| Fire / collapse / disease rolls | Per building per game hour, staggered |

## 12. Threading model

- The main thread owns the sim tick orchestration.
- Inside a tick, systems run as an enkiTS task graph. Independent systems (pathfinding batch, decision batch, production, matcher) run in parallel, each writing to its own output buffers; buffers are applied in a fixed order at sync points.
- The renderer runs after the tick on the main thread at first; the sim can move to its own thread later if needed.

## 13. Performance gate (Milestone M2)

The headless scenario `stress_50k`: 50,000 agents on a 768² map with roads, 500 buildings, and agents commuting, shopping and sleeping on schedules, run at 4× for 3 in-game days.
**Pass:** average sim time per frame ≤ 8 ms on M1, p99 ≤ 16 ms, zero heap allocations inside `tick()` (verified by an allocation hook in debug builds). Don't start game content until this passes.
