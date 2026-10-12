# 02 — Game Design

All numbers in this document are starting values for tuning and live in `data/defs/`. Content lists (buildings, goods, chains) are in `04-content-catalog.md`.

---

## 1. Core loop

**Minute to minute:** place buildings, roads and workplaces → citizens move in, take jobs and run their routines → goods flow through production chains → needs are met or not → housing evolves or decays → the city grows and unlocks more.

**Hour to hour:** balance class needs, keep the gods and Rome satisfied, survive threats and disasters, hit mission goals, then earn promotion to the next city.

**The player's levers:**

| Lever | Examples |
|---|---|
| Placement | Buildings, roads, zoning, plazas, walls |
| Labor priorities | Which sectors get workers first (Caesar-style labor screen) |
| Policies / laws | Tax rates, wages, rations, manumission policy, curfews, conscription |
| Spending | Festivals, games, temple offerings, monuments, loans from Rome |
| Military | Raise, equip, station and order legions |
| Trade | Open routes and set import/export quotas and stockpile targets |

The player never directly orders an individual citizen. The only exceptions are soldiers in battle and optionally assigning a named character to a post.

---

## 2. Time and calendar

- The sim runs at a fixed 20 ticks/sec at 1× speed. Speeds available: pause, 1×, 2×, 4×. While paused, the player can still build, demolish, lay roads and change policies and trade settings; these take effect at once.
- **1 in-game day ≈ 60 real seconds at 1×.** A day has phases: night, dawn, morning, midday, afternoon, evening.
- **1 year = 16 days** (4 seasons × 4 days). A 6-hour mission covers about 22 in-game years, so generations visibly turn over.
- The Roman calendar drives festivals (Saturnalia, Lupercalia, Ludi Romani and others). Festivals are scheduled events the player can fund for happiness and favor with the gods.

---

## 3. Map, terrain and placement

- Tile grid, 16×16 px tiles, chunked in 32×32 tiles. Map sizes range from 192² (mission 1) to 768² (final missions).
- **Terrain:** grass, fertile soil (farm yield multiplier), sand, rock, forest, marsh (disease risk), shallow and deep water, and elevation in discrete height levels.
- **Elevation matters:** aqueducts need a downhill gradient; walls and forts on high ground get a defense bonus; hills block line of sight for towers.
- **Placement:** everything snaps to the grid. Roads support 8 directions, including diagonals.
- **Two building types:**
  - *Fixed-footprint buildings* (temples, monuments, baths, houses): placed whole from a template. Houses can also be dragged: the drag lays one house per footprint across the area, each facing its street.
  - *Painted workplaces* (Songs of Syx style: farms, workshops, warehouses, quarries): the player paints an area, and capacity and jobs scale with size. Some require furniture or tools placed inside (workbenches, kilns, presses).
- **Construction is physical.** Builders haul materials to the site and work it; nothing appears instantly. Paid rush construction is available as a money sink.

---

## 4. Citizens

### 4.1 Classes

| Class | Role | Housing line | Notes |
|---|---|---|---|
| **Patricians** | Elite; few in number; pay high taxes; fill senior posts (magistrates, priests) | Domus → Villa Urbana | Demand luxuries, culture and prestige. Their happiness strongly affects Favor. |
| **Equites** | Merchant and administrative class; run trade and contracts | Domus | Drive trade efficiency and provide clerks and administrators. |
| **Plebeians** | The workforce: craftsmen, farmers, laborers, shopkeepers; army recruits | Tent → Hut → Casa → Insula tiers | The bulk of the population. Housing evolution is the main growth engine. |
| **Freedmen (Liberti)** | Former slaves; legally free; can become wealthy | Plebeian line | Often skilled and entrepreneurial. A path to social mobility. |
| **Slaves (Servi)** | Unpaid labor owned by households (*servi privati*) or the city (*servi publici*) | Owner's household or slave quarters | See 4.6 below. |
| **Soldiers** | Legionaries and auxiliaries, drawn from citizens | Barracks / fort | Paid, equipped and fed by the city. See Military. |

Foreign traders and caravans visit but aren't residents.

### 4.2 The individual citizen

Every citizen is a real entity with:
- **Identity:** a Roman-style name (praenomen/nomen/cognomen for citizens; single names for slaves), sex, age, class, origin.
- **Household:** family members, home, shared household inventory and savings.
- **Traits:** 1–3 traits from a pool (Pious, Lazy, Ambitious, Drunkard, Thrifty, Sickly, Brawler, Scholar, ...) that affect needs, productivity and choices.
- **Skills:** per-sector proficiency (farming, crafting, building, trading, soldiering, scholarship), which grows with work.
- **Job:** a workplace and shift, or student, retired, unemployed.
- **Needs:** current satisfaction per need (section 5).
- **Happiness:** a weighted result of needs, class expectations, events and gods, with a visible breakdown.
- **Health:** sickness, injury, age.
- **Memory log:** a short history of notable events (born, married, promoted, survived fire, freed), shown in the inspector.

### 4.3 Lifecycle

- **Born** into a household. The birth rate depends on happiness, housing space and Venus's favor.
- **Child (0–13):** attends school if available, which raises adult skill caps.
- **Adult (14–55):** works, marries, forms households. Marriage happens within or adjacent to the citizen's class.
- **Elder (55+):** reduced work, then retirement; can mentor and boost workplace skill growth.
- **Death:** from old age, sickness, disaster or war. The household inherits savings, and funerals are a minor religious event.

### 4.4 Daily life (the visible simulation)

Each class has a **daily schedule template** of time blocks. Individuals vary by traits and job shift.

| Phase | Typical plebeian | Typical patrician |
|---|---|---|
| Night | Sleep | Sleep |
| Dawn | Wake, fetch water, breakfast | Morning salutatio (receiving clients at home) |
| Morning | Work | Civic duty at forum, basilica or curia |
| Midday | Meal break: tavern or home | Meal |
| Afternoon | Work | Baths, leisure |
| Evening | Errands, leisure, worship, tavern | Dinner party, theater |

### 4.5 Errands and personal goals (the "missions" each citizen has)

Citizens carry out **errands**, short tasks generated by needs and their household:
- Buy bread and oil at the market for the household
- Fetch water from a fountain
- Visit the baths, a temple, the theater
- Take a sick child to the physician
- Deliver an offering for a festival

They also pursue **life goals**, long-term ambitions that make individuals feel alive:
- Move to better housing
- Save enough to marry or to open a shop
- Get an apprenticeship for a child
- (Slaves) earn manumission
- (Ambitious patricians) win a magistracy

Achieving a life goal gives a happiness spike and an entry in the memory log. Failing repeatedly breeds discontent.

### 4.6 Slavery

Slavery was a central part of Roman society and the game represents it as a system with real social and economic consequences. It isn't decoration, and it isn't glorified.
- **Sources:** slave markets (trade) and war captives.
- **Ownership:** wealthy households own *servi privati*, who work in the household and its businesses; the city owns *servi publici* for public works.
- **Economics:** no wages, but owners pay upkeep (food, housing); productivity depends on conditions and skill.
- **Treatment policy** (city law): affects slave happiness and productivity, and the risk of escape and revolt.
- **Manumission:** a policy plus individual life goals; freed slaves become *liberti*, with an upward-mobility path.
- **Servile revolt:** if the slave share is high and conditions are bad, revolts can trigger. A whole campaign mission is built around this (see campaign).

### 4.7 Immigration and emigration

Immigrants arrive at the map edge and look for housing when **city appeal** is positive (jobs, housing, food, safety, festivals, ratings). They emigrate when unhappy and housing is bad. Each class has its own appeal calculation; for example, patricians only come if prestige housing and culture exist.

---

## 5. Needs and services

Services are **visit-based**: citizens physically go to them during their routine. The exceptions are protective services (vigiles fire watch, engineers, urban guard), which **patrol** like Caesar walkers.

| Need | Satisfied by | Classes that care |
|---|---|---|
| Food (calories) | Any food at home | All |
| Food variety | 2–4 distinct food types in household stock | Plebs (2), Equites (3), Patricians (4) |
| Water | Well (basic), fountain (good), piped water (best) | All |
| Rest | Home bed quality | All |
| Hygiene | Latrina, balneum, thermae | Plebs+ |
| Health | Physician, hospital; clean water; no marsh nearby | All |
| Religion | Shrine / temple visits, festivals | All (Pious trait ×2) |
| Entertainment | Tavern, theater, amphitheater, circus, games | Plebs+, scaled by class |
| Education | School (children), library, academy | Equites, Patricians; skilled plebs |
| Clothing | Clothing goods; dyed clothing for elites | All except slaves (basic) |
| Luxury goods | Pottery → wine/glassware → jewelry/garum/marble décor | Rising by class |
| Safety | Guard coverage, low crime, walls | All |
| Prestige | Nearby monuments, gardens, statues, high desirability | Equites, Patricians |

---

## 6. Housing and desirability

- Houses **evolve or devolve** according to the needs met for their residents and the local **desirability**.
- **Desirability** is a tile-level field. Gardens, statues, plazas, temples and fountains add to it; workshops, tanneries, kilns, barracks, markets (slightly) and graveyards subtract. It falls off with distance.
- **Plebeian line:** Tent → Hut → Casa → Small Insula → Large Insula (multi-story, high density, fire and collapse risk).
- **Patrician line:** Domus → Grand Domus → Villa Urbana (needs luxuries, culture, high desirability).
- A house needs a road connection and access to the required services within walking distance at reasonable travel time. Coverage is measured by **real travel cost**, not radius.

---

## 7. Economy

### 7.1 Production

Production chains are of medium depth, between Caesar and Anno (full list in the catalog). Workplaces have job slots; output depends on worker count, skill, tools, workplace size and furniture, and inputs on hand.

### 7.2 Logistics (physical, Songs of Syx style)

- Every good physically exists in a stockpile or is carried by someone.
- **Porters** carry small loads; **carts** (mule or ox, unlocked mid-campaign) carry bulk along roads; **ships** carry trade goods to and from the harbor.
- **Warehouses and granaries** have per-good policies (accept, reject, stockpile target).
- **Markets** pull goods from storage via market staff; households send a member on a shopping errand.
- The player sets priorities and stockpile targets; the haul-job system does the rest (see sim design).

### 7.3 Money

- **Currency:** denarii.
- **City treasury:** income from taxes, exports, the mint and Rome's grants. Spending covers wages for public workers and soldiers, imports, construction, festivals, offerings and loan interest.
- **Households** hold savings and pay for goods at market prices. Wages are paid by workplaces; public workplaces are paid from the treasury.
- **Taxes:** collected through forum/tax office coverage, with a per-class tax rate policy. High taxes reduce happiness.
- **Governor's personal savings** (Caesar-style): a salary by rank, persists across the campaign, and can be donated to the treasury in emergencies or spent on career perks.
- **Debt:** loans from Rome at interest. Being in debt too long damages Favor and can end a mission on harder settings.

### 7.4 Trade

- Land routes (caravans via roads to the map edge) and sea routes (harbor).
- The trade partners on the province map buy and sell specific goods, each with **dynamic prices** affected by supply.
- Routes cost money to open and can be disrupted by war or pirates.
- **Rome's demands:** periodic requests for specific goods or troops, with deadlines. Meeting them grants Favor and money; failing costs Favor.

---

## 8. Water and infrastructure

- **Wells** give basic water anywhere with groundwater; groundwater quality is a terrain layer.
- **Aqueducts** carry water from a high-elevation source to a reservoir and need a continuous downhill gradient. Bridges span valleys (expensive), and arches are a construction project.
- **Reservoir → pipes → fountains, baths, wealthy houses.** Pipes run underground and are drawn in an overlay. Baths and villas need pipes.
- **Sewers (cloacae)** reduce disease and are required for large insulae and thermae.
- **Engineers** patrol to reduce **collapse risk** in buildings that need maintenance.

---

## 9. Religion and gods

Gods are stylized and have real gameplay effects. Each god has a **mood** fed by temples, offerings, festivals and the sacrifices the city makes in its domain.

| God | Domain | Blessing (high mood) | Wrath (low mood) |
|---|---|---|---|
| Jupiter | Authority, the state | Favor boost, lightning destroys enemy siege engines | Lightning fires, Favor drop |
| Juno | Marriage, women, the state | Birth and marriage boost | Fewer births, household unrest |
| Mars | War | Soldier morale and training speed | Military desertion, defeats |
| Venus | Love, beauty | Happiness and desirability boost | Plague of discontent |
| Ceres | Grain, harvest | Bumper harvests | Blight, crop failure |
| Neptune | Sea, water, earthquakes | Safe sea trade, fishing boost | Storms sink ships, earthquakes |
| Mercury | Trade, travelers | Better trade prices, extra caravans | Merchants shun the city |
| Minerva | Crafts, wisdom | Workshop productivity, research speed | Workshop accidents |
| Vulcan | Fire, the forge | Smithing boost, fire resistance | Great fire |
| Vesta | The hearth, home | Household happiness, lower fire risk | Household fires |

The available gods vary per mission; regional and imperial cults unlock later, such as the Imperial cult from mission 11 and Egyptian gods in Alexandria. Priests come from the patrician class.

---

## 10. Politics, favor and ratings

**City ratings** (mission goals reference these):
- **Culture:** coverage of temples, theaters, schools and libraries.
- **Prosperity:** housing quality, wealth, treasury health.
- **Security:** crime, fire and collapse incidents, defense, military strength.
- **Favor:** the Senate (Republic missions) or the Emperor (Empire missions). Affected by meeting requests, tribute, debt, events and patrician happiness.

**Characters:** named advisors with portraits and personalities (chief engineer, quaestor/treasurer, haruspex/priest, legate). They comment, warn and offer quests. Rival governors and visiting dignitaries appear in events.

**Events:** Lua-scripted, with text, choices and consequences, such as "A delegation of merchants asks for lower port tariffs" or "The haruspex reads ill omens: hold a festival to Jupiter?"

**Laws and policies:** taxes per class, wages, grain dole (*annona*) for the poor, slave treatment, manumission, curfew, conscription, building codes (max insula height trades density for fire risk).

---

## 11. Military

Not the focus, but it must be solid. Combat is straightforward with light tactics.

- **Raising troops:** barracks recruit adult plebeians (Republic) or volunteers (Empire). Recruits leave the workforce, which is a real trade-off.
- **Upkeep:** wages, food, weapons and armor from the armory chain, replacement equipment after battles.
- **Unit types:** legionaries (heavy infantry), auxiliary archers, auxiliary cavalry, and later siege engines (ballista, onager). Militia are available early.
- **Training:** training grounds raise unit experience.
- **Defense:** walls, gates, towers, forts (castra), garrisons.
- **Battles:** thousands of individual soldiers on screen, Syx-style. The player commands **units** (cohorts), not individuals: move, attack, hold, formation (line, wedge, testudo), stance (aggressive/defensive). Morale and fatigue matter; routed units flee.
- **Off-map campaigns** (optional per mission): send a legion away to fight an abstracted campaign for loot, captives and Favor, at the risk of losses.

---

## 12. Threats and disasters

| Threat | Cause / trigger | Counter |
|---|---|---|
| Fire | Kilns, smithies, insulae, dry season, Vulcan's/Vesta's wrath | Vigiles patrols, fire-break plazas, stone buildings |
| Collapse | Unmaintained buildings, tall insulae | Engineers, building codes |
| Disease / plague | Marsh, bad water, overcrowding, no sewers, trade ships | Physicians, hospitals, aqueducts, sewers, quarantine policy |
| Crime | Unemployment, poverty, low Safety | Urban guard, employment, festivals |
| Riots | Low happiness plus a trigger (food shortage, high taxes) | Fix causes; the guard suppresses at a happiness cost |
| Servile revolt | High slave share plus bad treatment | Treatment policy, manumission, garrison |
| Barbarian raids / invasions | Mission script, frontier maps | Walls, military |
| Pirates | Sea-trade missions | Harbor defenses, the navy event chain |
| Earthquake / flood / volcano / storm | Map- and mission-specific; Neptune | Construction quality, location choices, gods |
| Drought / blight | Seasons, Ceres | Granary reserves, imports, irrigation |

Difficulty scales how often and how hard these hit (see campaign).

---

## 13. Seasons and weather

- **Spring:** planting; mild.
- **Summer:** growth; fire risk up and drought possible.
- **Autumn:** harvest; olive and grape harvest; festivals.
- **Winter:** no farm growth, higher food and fuel consumption, shipping slows.
- **Weather events:** rain, heatwaves, storms. Region modifiers apply, such as the Nile flood in Alexandria and harsh winters in Germania.
- **Floods:** a flood damages buildings on low ground near water. Fields are only soaked. Buildings that must stand at the water (harbors, wharves, the lighthouse) are built for it and take no flood damage. The yearly Nile flood is gentler than a disaster flood, and it renews the soil.

---

## 14. UI and management tools (how scale stays stress-free)

- **Overlays:** desirability, water and pipes, fire risk, collapse risk, crime, disease, service coverage (by travel time), land value, employment, class map, traffic heatmap, elevation.
- **Labor screen:** sector priorities, plus worker counts needed versus filled.
- **Ledgers:** goods (stock, production, consumption, trend), finance, population by class and age, happiness breakdown by class with cause attribution.
- **Alerts feed:** grouped, deduplicated, clickable, with the root cause and a suggested fix ("Insula block in the east district devolving: no oil for 3 days. Nearest market has no oil because no olive press is staffed."). Checked daily: a building with no workers, an idle workshop and the input that is not coming, a producer with nowhere to store its output, a full store, a market with nothing to sell, homeless citizens, unemployment of 15% or more, and the day's emigrants; plus the events (fires, disease, raids, requests, gods) as they happen. A condition that lasts stays one entry with a day count, and each kind names at most six buildings a day.
- **Inspector:** click any citizen to see their name, household, job, current errand, schedule, needs, traits and memory log. **Follow camera** mode.
- **Building inspector:** workers, inputs and outputs, efficiency breakdown, who lives or works here.
- **Advisors panel:** the named advisors summarize their domains in plain language.
- Speed controls, pause-on-event options, and auto-pause on major threats.

---

## 15. Difficulty

Each mission has a designed difficulty, and the player picks a global modifier:

| Setting | Effect |
|---|---|
| **Otium** (relaxed) | Fewer disasters, generous economy, no mission failure from debt, gentle raids |
| **Civitas** (normal) | As designed |
| **Imperium** (hard) | More frequent and harsher threats, demanding Rome, tighter margins, mission failure conditions |

Early missions lean relaxed and later missions lean challenging, so the campaign itself provides the balance between the two.

Implemented multipliers: threat frequency (fire, collapse, plague, drought, storms) ×0.5 / ×1 / ×1.5; raid size ×0.6 / ×1 / ×1.4; starting treasury ×1.5 / ×1 / ×0.75; favor lost on a failed request ×0.5 / ×1 / ×1.5. Missions fail on lost favor from Civitas up, and also on long debt on Imperium.
