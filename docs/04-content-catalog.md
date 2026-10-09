# 04 — Content Catalog

This is the baseline content set. The "M" column gives the campaign mission where an item first unlocks. Every entry becomes a definition in `data/defs/`.

## Goods

| Category | Goods |
|---|---|
| Raw food | wheat, vegetables, fish, meat (pork), olives, grapes |
| Processed food | flour, bread, olive oil, wine, salt, garum |
| Building materials | wood, planks, clay, bricks, stone, cut stone, limestone, lime, pozzolana, concrete, marble, sand |
| Metals | iron ore, iron, silver ore, silver, gold (trade only) |
| Crafted | tools, weapons, armor, pottery, amphorae, wool, clothing, dye, dyed clothing, furniture, glassware, papyrus, scrolls, jewelry |
| Special | denarii (minted), war captives (people, not goods) |

## Production chains

| Chain | Steps | M |
|---|---|---|
| Bread | Wheat farm → **wheat** → Mill → **flour** → Bakery → **bread** | 1–2 |
| Vegetables | Vegetable garden → **vegetables** | 1 |
| Fish | Fishing wharf → **fish** | 2 |
| Salt | Salt works (coast) → **salt** | 2 |
| Meat | Pig farm (+wheat feed) → **meat** | 3 |
| Pottery | Clay pit → **clay** → Potter → **pottery / amphorae** | 2–4 |
| Olive oil | Olive grove → **olives** → Olive press (+amphorae) → **olive oil** | 4 |
| Wine | Vineyard → **grapes** → Winery (+amphorae) → **wine** | 4 |
| Garum | Fish + salt → Garum workshop (+amphorae) → **garum** | 5 |
| Timber | Lumber camp → **wood** → Sawmill → **planks** | 1–3 |
| Bricks | Clay + wood → Brick kiln → **bricks** | 3 |
| Stone | Quarry → **stone** → Stonemason → **cut stone** | 3 |
| Clothing | Sheep farm → **wool** → Weaver → **clothing** | 3 |
| Metal | Iron mine → **iron ore** → Smelter (+wood) → **iron** → Blacksmith → **tools**; Armory → **weapons, armor** | 6 |
| Silver | Silver mine → **silver ore** → Smelter → **silver** → (export / mint) | 6 |
| Concrete | Limestone → Lime kiln (+wood) → **lime**; Pozzolana pit → **pozzolana**; lime + pozzolana + stone → Concrete works → **concrete** | 7 |
| Furniture | Planks → Carpenter → **furniture** | 8 |
| Marble | Marble quarry → **marble** → Sculptor → statues / décor | 10 |
| Dye | Dye works (coastal murex / plants) → **dye**; clothing + dye → Dyer → **dyed clothing** | 10 |
| Scrolls | Papyrus (import or marsh reeds) → Scriptorium → **scrolls** | 10 |
| Coins | Silver → Mint → **denarii** to the treasury | 11 |
| Glass | Sand + wood → Glassworks → **glassware** | 12 |
| Jewelry | Gold/silver → Jeweler → **jewelry** | 12 |

## Consumption by class and housing tier

| Tier | Food | Goods | Services | Reachable from |
|---|---|---|---|---|
| Tent / Hut | 1 food | — | Water (well), shrine | M1 |
| Casa | 2 foods | Pottery | Market, shrine/temple, latrina | M2 |
| Small Insula | 2 foods incl. bread | Pottery, clothing, oil | Well water, balneum, tavern, physician | M4 |
| Large Insula | 3 foods | + wine | Fountain water, sewer, temple, theater, school, guard | M7 |
| Domus | 3 foods incl. meat | Oil, wine, clothing | Well or fountain water, balneum, theater | M4 |
| Grand Domus | 4 foods | + furniture, dyed clothing | Piped water, amphitheater, library, academy | M10 |
| Villa Urbana | 4 foods incl. garum | + glassware, jewelry, marble décor | Thermae, all culture, prestige monuments nearby | M12 |
| Slaves | Basic food (bread/porridge) | — | Water; rest; treatment policy | M1 |

Housing tiers are gated by unlocks on purpose: early missions cap housing (Hut in M1, Casa in M2–M3, Small Insula in M4–M6), which keeps early cities simple. "Reachable from" is the first mission in which every requirement can be met.

## Buildings

### Housing and civic
| Building | M | | Building | M |
|---|---|---|---|---|
| Housing plot (plebeian) | 1 | | Forum (taxes, admin) | 1 |
| Domus plot (equites/patrician) | 4 | | Governor's palace | 4 |
| Slave quarters | 6 | | Basilica (courts, commerce) | 8 |
| Plaza | 4 | | Curia (council, laws) | 9 |
| Gardens | 2 | | Mint | 11 |
| Statue | 4 | | Triumphal arch | 11 |

### Infrastructure and water
| Building | M | | Building | M |
|---|---|---|---|---|
| Dirt road | 1 | | Aqueduct (+ arches) | 7 |
| Paved road | 3 | | Reservoir | 7 |
| Bridge | 2 | | Pipes | 7 |
| Well | 1 | | Fountain | 7 |
| Latrina | 2 | | Sewer (cloaca) | 7 |
| Engineers' post | 3 | | Harbor / dock | 5 |

### Food, industry, storage
The workplaces listed in the chains above, plus:
| Building | M | | Building | M |
|---|---|---|---|---|
| Stockpile (open) | 1 | | Market (macellum) | 1 |
| Warehouse | 1 | | Trade post (land) | 2 |
| Granary | 1 | | Cart depot | 8 |

### Services
| Building | M | | Building | M |
|---|---|---|---|---|
| Shrine | 1 | | Tavern (popina) | 2 |
| Temple (per god) | 3 | | Theater | 4 |
| Great temple | 8 | | Amphitheater | 9 |
| Physician | 2 | | Odeon | 10 |
| Hospital (valetudinarium) | 9 | | Circus / hippodrome | 12 |
| Balneum (small baths) | 4 | | School | 4 |
| Thermae (great baths) | 7 | | Library | 8 |
| Vigiles post (fire) | 2 | | Academy | 10 |
| Guard post (urban cohort) | 4 | | | |

### Military
| Building | M | | Building | M |
|---|---|---|---|---|
| Militia post | 3 | | Fort (castrum) | 7 |
| Walls, gates, towers | 3 | | Stables (cavalry) | 7 |
| Barracks | 6 | | Siege workshop | 13 |
| Training ground | 6 | | | |

### Monuments (multi-stage, materials + labor + time)
| Monument | Effect | M |
|---|---|---|
| Temple of Jupiter (Capitoline-style) | Huge Jupiter mood, Favor | 8 |
| Lighthouse | Sea trade and safety | 12 |
| Grand aqueduct arcade | Water capacity, prestige | 7 |
| Triumphal arch | Favor, desirability | 11 |
| Pantheon | All-gods mood, culture | 16 |
| Colosseum | Massive entertainment, Favor | 16 |
| Circus Maximus | Entertainment, Favor, trade fairs | 16 |
