# 12 — Story

The campaign tells one story: a Roman career, from a land grant by a river in Latium to the rebuilding of Rome. It is told in letters, advisors' messages and the city's own events, never in cutscenes. History is stylized (docs/01): real places and events, loose with dates. One career spans centuries, and nobody remarks on it.

## The player

The player is **you**, never named. You are a *novus homo*, a "new man": a farmer's son with no ancestors in the Senate, raised by merit and by a patron's favor. The nobles never let you forget it. Ranks follow `data/campaign.json`.

## The cast

**The advisors** are a household that follows you from post to post. Their names are in `src/game/advisors.hpp`, their portraits in the atlas, and their numbers are used by `urbs.message`.

| # | Advisor | Voice |
|---|---|---|
| 1 | **Vitruvius**, chief engineer | Exact and proud. Thinks in paces, gradients and courses of brick, and despises cheap work. Water is his passion; he reveres Archimedes. Starts writing a book on architecture at Aquileia and is still revising it in Rome. |
| 2 | **Marcus Calidius**, quaestor | Dry and careful. Counts everything, never wastes a word, and his rare jokes are about money. Brings the post: letters reach you through him. |
| 3 | **Spurinna**, haruspex | Reads livers, lightning and birds. Means every word, even the cryptic ones. At the founding he reads an omen: the road that starts at the river ends in Rome. He once warned a man in Rome about the Ides of March. |
| 4 | **Quintus Labienus**, legate | An old soldier, blunt and loyal. Says he is too old for this in every province. In the end he retires to a farm by the river in Latium. |
| 5 | **Gnaeus Lollius**, aedile | The people's man: markets, games, gossip. Knows what the street thinks before the street does. The conscience of the household; he was against the gladiator school at Capua. |

**Your patron, Aulus Postumius**, an old senator, wrote the land grant that began it all. His letters appoint you to each new post until the Republic breaks apart. His last letter comes before Corinth. After that the letters come from the Emperor's secretary (*ab epistulis*), in the polite, cold style of the palace.

**Your rival, Lucius Aurunculeius Mela**, is well born, clever, and certain that men like you should carry the luggage. He wanted Beneventum. He commands the army you supply at Narbo, and becomes a friend of the palace under the Empire. In Antioch the earthquake buries him and your engineers dig him out. His letter recommends you for Rome.

**The first stone.** At the founding Spurinna ploughs the furrow and sets a boundary stone by the river. The hamlet in Latium comes back at the end.

## Where the text lives

- **`data/campaign.json`**, per mission:
  - `blurb`: one line for the province map.
  - `letter`: the appointment letter, shown on the province map before the mission begins.
  - `victory`: the text on the victory screen.
  - Mission 16 also has `epilogue`, the end of the campaign.
- **`data/scenarios/mXX/script.lua`**: the arrival briefing (`on_start`), events, choices, requests and replies to them.
- **`data/defs/gods.json`**: what the gods' blessing and wrath look like (`blessing_text`, `wrath_text`).
- **The defeat screen** (`src/game/app_ui.cpp`) uses the simulation's loss reason, framed as a recall.

## How to write it

- **Concrete beats abstract.** Amphorae, modii of wheat, the smell of the garum vats, the number of crosses on the Appian Way. Avoid "bustling", "nestled", "rich tapestry", "testament to", "journey", "embark", "delve", and grand openings like "In the heart of".
- **Characters speak like themselves.** Calidius would not say "exciting opportunity"; Labienus does not explain his feelings.
- **Short sentences, varied rhythm.** One idea per sentence in instructions. No rhetorical triplets as a habit.
- **Instructions stay exact.** When a message teaches, name the building and the panel exactly as the game does (Build, Rome, Trade, Labor, Religion, Military, Research, Finance, Goods). Story never hides what to do.
- **Requests say what is wanted in words**, not data ids ("40 amphorae of olive oil", not "40 olive_oil").
- **Keep mechanics and text in step.** If a message promises something (a raid from the north, a grant of 4,000), the script must do exactly that.
