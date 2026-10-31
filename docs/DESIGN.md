# Capo di Tutti Capi — design

*Working title. Draft 1: everything here is up for change; numbers are starting values to be tuned (they are meant to end
up in the economy sheet), and the questions still open are collected at the end.*

A lair builder in the spirit of Evil Genius, where the lair is a mob family's hideout under a trucking warehouse and the
world outside is a city split between rival families. You start as a small-time boss with a basement and a handful of
associates, and win by making every other family either dead or yours: the boss of all bosses.

The game has two views the player switches between at any time, sharing one clock:

- **The lair** (isometric): build and run the hideout, where your men live, plan, and store the goods.
- **The city** (flat top-down map): send your men on missions, take over rackets, and deal with the police and the other
  families.

## The loop

1. Missions on the city map earn **money** and **goods**, and cost **heat** and **vendetta**.
2. Goods are stored in the lair and shipped out by the front company's van, which turns them into money.
3. Money builds the lair; the lair decides how many men you can keep, how good they are and how much you can store.
4. Rackets taken on the map pay money over time; a whole ring pays double.
5. Heat and vendetta push back. Lay low, bribe, or settle a feud before it settles you.

## Time

Real time on a day clock, as in Evil Genius: pause, 1×, 2× and 3×. One in-game day is about 2 minutes at 1×. Missions
take hours to days, construction takes hours, rackets pay out once a day at midnight, heat decays every hour.

## Resources

| Resource | Range | Goes up by | Goes down by |
|---|---|---|---|
| **Money** | 0 + | missions, rackets, van shipments | construction, mission investment, wages, bribes |
| **Heat** | 0–100, one city-wide value | missions (each lists its heat), police noticing goods, violence | decays every hour with no mission running, bribe missions |
| **Vendetta** | 0–100, one per rival family | missions against that family, taking its rackets | campaigns, tribute, time (very slowly) |
| **Goods** | crates, by kind | hijacks, smuggling, racket side-income | van shipments, police raids |

Wages: every man is paid daily (associates a little, made men more). Missing payday costs loyalty — see *Your family*.

### Heat

Heat is the police's interest in you. It falls by a fixed amount each hour while none of your men are on a mission
(laying low), and slower while they are.

| Heat | Effect |
|---|---|
| 0–24 | Unnoticed |
| 25–49 | **Watched**: missions lose 10% success chance; an unmarked car parks outside the warehouse |
| 50–74 | **Investigated**: −20% success; men arrested on a failed mission are held longer; inspections of the warehouse (ground floor only) |
| 75–99 | **Task force**: −30% success; raids on the whole lair (see *Raids*) |
| 100 | **Indictment**: a short countdown — get heat under 75 before it runs out, or the lair is busted (see *Busts and moving house*) |

**Bribe missions** (a cop, a judge, a councilman) cost money and drop heat at once; better bribes cost more and give
lasting effects (a bought judge frees arrested men sooner, a bought captain warns you before a raid).

### Vendetta

Bad blood with each family, shown on its panel and as the colour of its border on the map.

| Vendetta | The family … |
|---|---|
| 0–24 | ignores you |
| 25–49 | leans on your rackets (their income drops while it lasts) |
| 50–74 | attacks: hijacks your van, shoots up a racket, beats up associates on missions in its turf |
| 75–99 | puts a hit on one of your made men, and may now and then **assault the warehouse** (see *Assaults*) |
| 100 | **open war**: assaults come every few days, aimed at the boss, until the vendetta is settled |

## The lair

An isometric grid of tiles, built on two levels joined by a staircase.

### The warehouse (ground floor)

The front: a trucking company. Its layout is fixed — offices, a loading bay, the van's parking spot and the staircase
down — but the player can place front-company furniture here (desks, forklifts, legal crates) which makes inspections
go easier.

- **The van.** Every so often (once a day, or on demand from the loading bay for a fee) the van comes in, workers carry
  crates from the lair's storage up to the bay, and it drives out. When it gets back the goods are sold. Each run has a
  small chance of being stopped, which raises heat and loses the cargo; the chance grows with heat and with how many
  crates it carries.
- **Inspections, raids and assaults** all come in through the warehouse. An inspection stays on the ground floor; a raid
  or an assault goes downstairs, unless the staircase is hidden behind a false wall (a late-game build), which the police
  must find first.

### The underground

Starts as a small dug-out basement at the foot of the stairs. The rest of the level is solid earth.

- **Digging.** Mark earth tiles to dig; associates with nothing else to do dig them out. Each tile costs a little money and
  time. Some tiles are rock (slower, dearer) or hold surprises (an old sewer tunnel: a back way out, a forgotten stash).
- **Walls and doors** are built on tile edges. A **room** is a walled area with at least one door, of a type the player
  assigns; it works only once it is big enough and holds the objects it requires.
- **Objects** are placed inside rooms of the right type and each does one thing. Men walk to objects to use them.

#### Rooms

| Room | Min size | Needs | Does |
|---|---|---|---|
| **Bunks** | 3×3 | a bunk | each bunk houses one man; you can't keep more men than you have bunks |
| **Bar** | 3×4 | a counter | men spend off-duty time here; restores loyalty |
| **Recreation room** | 4×4 | one of: pool table, card table, jukebox, boxing bag | restores loyalty; the boxing bag also trains muscle |
| **Kitchen / mess** | 3×3 | a stove, a table | men who eat stay loyal longer; feeds N men per stove |
| **Storage** | 3×3 | shelving | each shelf holds N crates; goods with no shelf stack in the open (and are seen by raids) |
| **Armoury** | 3×3 | a gun rack | needed for any mission marked *armed*; each rack equips N men at once; a workbench adds success chance to armed missions |
| **Computer room** | 3×3 | a terminal | reveals more of each mission's details; a second terminal lets you see the police's next move (heat events a day early) |
| **War room** | 4×4 | a map table | needed to start a campaign; each extra object (corkboard, phone) shortens planning |
| **Office** | 3×3 | the boss's desk | where the boss sits; one only; its decoration raises made men's loyalty |
| **Vault** | 2×2 | a safe | money in the safe can't be confiscated by a raid; money not in it can |
| **Infirmary** | 3×3 | a cot | wounded men heal here instead of being out for days |
| **Hidden storage** | 2×2 | shelving, a false-wall door | holds crates the police can't see unless they find the door |

Rooms and objects are unlocked by growth (see *Growing the family*), not all at once.

#### Defences

Objects that can go in any room or corridor, for raids and assaults:

| Object | Does |
|---|---|
| Guard post | a post for a man on the *Guard* job |
| Reinforced door | replaces a door; takes attackers a while to break and police a while to open |
| Camera | shows the room on the computer room's screens: intruders there are seen, and guards come to them |
| Alarm bell | when it rings, every man in the lair drops what he is doing and takes his place (see *Raids*, *Assaults*) |
| False wall | a door that looks like a wall; police find it only by searching next to it |

## Jobs

Men are not controlled directly, and are not given a job each. The family's work is a **job queue**, ordered by priority,
and every free man — associate or made man — takes the first job in it he can do.

### What a job is

A job is one piece of work in one place: dig this tile, build this wall, carry this crate to storage, stand this guard
post for a shift. It comes into the queue by itself when there is work to do: marking tiles to dig adds dig jobs,
placing a room adds build jobs, a crate landing in the warehouse adds a carry job, and every staffed object (a guard post,
the bar counter, a terminal) adds a **post** that is open whenever nobody holds it.

| Job | Where | Does |
|---|---|---|
| Dig | marked earth | digs out a tile |
| Build | a placed wall, door, room or object | builds it |
| Carry | a crate | carries it from where it landed to storage, or from storage to the van on shipping day |
| Guard | a guard post | stands there; first to fight in an assault, and holds police at his door in a raid |
| Lookout | the warehouse front | gives warning before a raid or an assault arrives; more lookouts, earlier warning |
| Front work | the warehouse | runs the trucking company: a little legal income; slows inspectors by talking to them |
| Bar / kitchen | bar counter, stove | the room restores loyalty twice as fast while it is staffed |
| Technician | a terminal | needed for the computer room to work |
| Planner | the map table | needed for campaigns; each planner shortens the next step's planning |
| Doctor | a cot | needed to heal; without one wounded men only rest |
| Collector | a racket on the map | collects in town: the racket pays more and is harder to lean on |

### The priorities

The player ranks the job kinds on the **job board** (Dig above Carry, Guard above everything, …), and can pin a single
job to the top of its kind (dig *this* tunnel first). A free man takes the open job that is highest on the board among
those he can do, the nearest of them on a tie. He keeps it until it is done, his shift at a post ends or he is too tired
to go on: a more important job opening up does not pull a man off his work (or men would run back and forth), except
for the emergencies below.

- **Made men** take jobs from the same queue, but some jobs are theirs only: leading a mission or a campaign step, and
  the posts of the war room's planner and the boss's bodyguard. A switch on the job board keeps them off the grunt work
  (digging, carrying) altogether.
- **Missions** are not taken from the queue. The mission popup picks its men (the best free ones by default, or the
  player's choice), and Go takes them off whatever they were doing. When they come back they are free again.
- **Emergencies** — the clean-up before a raid, the alarm bell, the evacuation — set the queue aside: every man in the
  lair drops what he is doing and does his part, then returns to the queue when it is over.
- There are no per-man job settings: every man can take every job his rank allows.
- A man with nothing he can take idles in the recreation rooms.

### Tiredness

Every man gets **tired** while working. When he is worn out he leaves his job by himself to sleep in his bunk, eat and
drink, then goes back to the queue. A lair short of bunks, food or a bar means tired men working worse and losing
loyalty.

## Raids

At *Task force* heat the police raid the lair, and the raid is played out: police officers walk in through the warehouse
and search, room by room.

1. **Warning.** Lookouts on duty (or a bought police captain) give warning before the raid arrives: no lookouts, no warning.
   While the warning lasts the player can sound the **clean-up**: porters run crates into storage shelves and hidden
   storage, men put their guns back in the armoury, and money goes into the vault.
2. **The search.** Officers search the ground floor, then go down the stairs (if they find them). Each officer sees what is
   around him; anything illegal he sees is found: crates in the open (on shelves only in storage that isn't hidden), loose
   money outside the vault, weapons outside the armoury. Front workers and guards at their doors slow them down. Searching
   next to a false wall can find it.
3. **The result.** What was found is confiscated; men caught with a gun are arrested; heat falls a little if little was
   found, and rises if a lot was.

The police are not fought. A guard who is told to *resist* (one switch for the whole lair) turns the raid into a
shootout, and a shootout with the police is a **bust**.

## Busts and moving house

A bust is the police coming not to search the lair but to shut it down. It happens when the indictment countdown runs
out at 100 heat, or when a raid turns into a shootout.

1. **The boss flees, by himself.** Every lair has a **bolt-hole**, a back way out from the start (the first basement
   has one; a sewer tunnel dug open makes another). When the police come in the boss leaves through the nearest one at
   once. The player cannot stop him or slow him down.
2. **The evacuation.** For a short time, before the police reach the underground, every man in the lair drops his job
   and saves what he can: crates and small objects (a terminal, a gun rack, the map table) are carried out to the
   bolt-hole. Men leave last, and those still inside when the police get down are arrested.
3. **Moving house.** The family picks a new front from the empty warehouses on the city map. The new lair has **the
   same layout** as the old one — every dug tile, wall, door and room where it was — but **half of the objects are
   missing**, picked at random from those the men did not carry out. The missing ones have to be bought and placed again.

What a move keeps and what it costs:

| Kept | Lost |
|---|---|
| the layout: dug tiles, walls, doors, rooms | half the objects (never one that was carried out) |
| the made-men slots, and the tier | crates that were not carried out |
| money, less a penalty: 30% of it, or 15% of what is in the vault | men arrested in the bust, held for days (or for good, without a bribed judge) |
| the rackets, and the men collecting at them | a little loyalty from every man |

The move also wipes the slate: **heat and every family's vendetta go back to 0**. The police lose the trail, and the
families take it as the end of the old quarrels. A move is a hard reset that can be worth having, which is why it can
only be done twice: **the third bust catches the boss**, and the game is lost.

Assaults end in a move too, if the attackers reach the boss's office: he flees by the bolt-hole the same way, the men
evacuate what they can, and the rival family takes the warehouse (and adds it to its own turf on the map).

## Assaults

At high vendetta a rival family sends a crew of gunmen against the warehouse, played out in the lair.

- **Warning** works as for raids: lookouts see their cars coming. The alarm bell sends every man to his place: guards to
  their posts, the rest to the armoury to arm, then to the nearest guard post or the boss's office.
- **The attackers** break in through the warehouse (or a sewer tunnel, if one is dug open) and go for one goal, shown
  when they arrive: **steal** crates, **wreck** rooms, **kill** men, or, at open war, **the boss** in his office.
- **Fights** are automatic: men in reach of each other trade shots, decided by Muscle, Nerve and whether they are armed.
  Downed men are wounded or dead; wounded men go to the infirmary.
- **The end.** The assault ends when the attackers have done what they came for or lost half their men. Beating one
  off lowers that family's vendetta a little (they lost face) but adds heat, more the longer the shooting went on. If
  the attackers reach the boss's office, he flees and the lair is lost (see *Busts and moving house*).

## Your family

- **The boss** — you. Never leaves the lair except for the final mission of a campaign (and its risk). If arrested or
  killed, the game is lost. He takes no jobs, and when the lair falls he flees by himself (see *Busts and moving house*).
- **Made men** — a fixed number of slots, starting at 2 and rising as the empire grows. They lead high-profile missions
  (robberies, hits, racket takeovers) and campaigns. They cannot be fired, only lost.
- **Associates** — the rank and file, as many as you have bunks. Hired on the city map through a **Recruit** mission (it
  costs money and a little heat, and turns up 1–3 candidates to choose from). They run most missions and do the lair's
  work (see *Jobs*).

Every man has three stats (1–5) and a loyalty bar:

- **Muscle** — fights, hits, intimidation;
- **Brains** — heists, fraud, planning;
- **Nerve** — keeps his head when it goes wrong: less chance of arrest or death on a failed mission.

Missions use the stats of the men sent. Men gain experience on missions and raise a stat now and then.

**Loyalty** drops with missed wages, failed missions, nights without a bunk, and time without rest; it rises in the bar,
the recreation room and the mess. A man at zero loyalty may walk out — or turn informant, which raises heat sharply.

**Promotion.** When a made-man slot is free, an associate with enough completed missions and high loyalty can be
*made* in a short ceremony in the lair. He keeps his stats and becomes a made man.

### Growing the family

The empire's size is counted in **rackets held**. Every so many rackets unlocks the next tier:

| Tier | Rackets | Made-men slots | Unlocks |
|---|---|---|---|
| Crew | 0 | 2 | bunks, bar, storage, digging, guard posts |
| Outfit | 3 | 3 | recreation room, armoury, kitchen, robberies, reinforced doors, alarm bell |
| Family | 7 | 4 | computer room, war room, campaigns, hits, cameras |
| Syndicate | 12 | 5 | vault, infirmary, judge bribes, false walls (hidden storage and staircase) |
| Commission | 18 | 6 | the last campaigns, against the strongest families |

## The city

A flat, geometric top-down map: grey roads, blocks of buildings in muted colours, districts outlined in the colour of the
family that holds the most of it. Things on the map are icons; hovering one names it, clicking one opens its popup.
The player's warehouse is a fixed point on the map, and men can be seen driving out from it to a mission and back.

### Rival families

Four to six families, each with a home district, a boss, a temperament (greedy, violent, cautious) that shapes how it
answers vendetta, and its own rackets. They act on their own: take free rackets, fight each other, and come for you
when the vendetta is high.

### Missions

Markers appear on the map over time and expire if not taken. Clicking one opens its popup:

- **Requirements**: how many associates and made men, minimum stats, whether it is *armed* (needs the armoury).
- **Investment**: the money it costs to start.
- **Duration**: how long the men are gone.
- **Chance of success**, worked out from the men picked, heat and the lair's bonuses, and updated as you pick them.
- **Rewards if it succeeds**: money, goods, a racket, vendetta lowered or raised.
- **Heat** it will add, and which family's vendetta it will raise.
- **Go**: deducts the money and sends the men. They are away until they come back.

The popup picks the best available men by default; the player can swap them. On failure the investment is lost, heat
rises more than it would have, and each man may be wounded, arrested or killed (Nerve lowers the odds).

| Mission | Crew | Gives | Heat | Notes |
|---|---|---|---|---|
| Shakedown | 1–2 associates | a little money | low | the bread-and-butter early mission |
| Hijack a truck | 2–3 associates | goods | medium | raises vendetta if the truck is a family's |
| Smuggling run | 2 associates | goods, costs investment | low | slow, safe |
| Recruit | 1 associate | 1–3 hiring candidates | low | the only way to hire |
| Bribe | 1 associate or made man | heat −N, maybe a lasting effect | none | costs a lot of money |
| Robbery | 1 made man + 3 associates, armed | a lot of money | high | bank, jeweller, armoured car |
| Racket takeover | 1 made man + 2 associates | a racket | medium | from a family (vendetta) or a free one |
| Hit | 1 made man + 1 associate, armed | a rival's capo dies | very high | vendetta +lots; weakens the family |
| Campaign step | set by the campaign | campaign progress | varies | only from the war room |

### Rackets and rings

A racket is a business on the map — a speakeasy, a card room, a cigarette wholesaler — held by one family (or nobody)
and paying its holder money every day. Rackets belong to **rings** by trade:

| Ring | Rackets | Example businesses |
|---|---|---|
| Booze | 4 | speakeasy, distillery, liquor store, docks bar |
| Cigarettes | 3 | wholesaler, tobacconist, bootleg truck stop |
| Gambling | 5 | card room, numbers bank, bookie, casino, racetrack |
| Vice | 4 | clubs and houses along the waterfront |
| Drugs | 4 | the most money, the most heat each day it is held |
| Protection | 5 | the restaurants and shops of one district |

**Holding a whole ring doubles the income of every racket in it.** Rivals benefit the same way, so breaking a family's
ring by taking one racket from it costs them half that ring's income — a reason to take a racket you don't need.

Some rings add a little heat every day they are held (drugs most of all).

### Campaigns

The way to settle a vendetta for good. Started from the war room against one family; it shows as a chain of 3–5
missions on the map, each unlocking the next (find out where the boss eats, turn one of his men, hit his warehouse …).
A campaign costs money to start and ties up a made man to lead it. Failing a step raises that family's vendetta and may
end the campaign.

The last step is a choice:

- **The hit**: the boss of that family is killed. The family is broken: its rackets become free (or yours if you hold
  the district), its vendetta disappears, but heat spikes and every other family's vendetta rises a little — they are
  frightened.
- **The deal**: a sit-down. The family's vendetta drops to 0 and it **swears to you**: it keeps its rackets but pays you a
  share of their income every day, and will not attack you. A sworn family's vendetta can climb again if you hit its
  business.

### Winning and losing

- **Win**: every other family is either broken or sworn to you. The last sit-down crowns you capo di tutti capi.
- **Lose**: the lair is lost a third time, to a bust or an assault, and the boss is caught.

## Controls

Mouse first. In the lair: the build menu along the bottom (dig, walls, doors, rooms, objects, defences), drag to mark
areas, right click to cancel, scroll to zoom, middle drag or WASD to pan, Q/E switches floor. The job board (J) ranks
the job kinds by dragging them up and down; right clicking a job in the lair pins it to the top. Clicking a man shows
him and what he is doing; the roster (R) lists every man with his current job, stats, loyalty and tiredness. The alarm
bell has a button of its own. Tab switches between the lair and the city. Space pauses, 1/2/3 sets speed.

## Open questions

1. **Scope of the city.** One city map hand-made, or generated per game?
2. **How many families**, and should each have a fixed personality and boss (a cast) or be generated?
3. **Goods.** Are goods kinds tied to rings (booze crates sell better if you hold the booze ring), or just "crates"?
4. **Failure and death.** Is losing a made man permanent and frequent, or rare? Do we want save-scumming prevented
   (roguelike) or a normal save?
5. **Endgame length.** A full game in 4–6 hours? Fewer families and tiers shrink it.
6. **The ring called "women" in the brief** is written here as *Vice*; confirm how explicit the game should be about it.
7. **Moving on purpose.** Since a move clears heat and vendetta, should the player be able to move by choice (for a
   price), or only when busted or driven out? As written, it only happens to you.

## As built (first playable version, 2026-10-25)

How the code answers the open questions, and where it simplifies the design above. Numbers live in
`Model/Tuning.cs` and `Model/Defs.cs` until they move to an economy sheet.

**Open questions, as decided for now**

1. One hand-made city (`CityLayout`): six districts, 25 rackets, five institutions, six warehouses.
2. Four families with a fixed cast: Moretti (cautious, Midtown), Vitale (greedy, Uptown, the strongest: needs
   the Commission tier), Kowalski (violent, Waterfront), O'Rourke (violent, Northside).
3. Goods are kinds (booze, cigarettes, contraband, drugs); a crate sells for 30% more while you hold a racket of
   its ring, double with the whole ring.
4. Normal save: autosave at midnight and on leaving, deleted when the game ends. Deaths are permanent but rare.
5. A scripted player wins in 90-150 game days (3-5 hours at 1x).
6. Vice is "clubs and houses along the waterfront", nothing more explicit.
7. No moving on purpose: only busts and assaults move you.

**Simplifications**

- Walls and doors are not built separately: a room is a dragged rectangle whose walls go up when it is built,
  with one door placed automatically on the side nearest the stairs. Objects are all 1x1 and do not block.
- Not built yet: cameras, reinforced doors, the false-wall staircase, the *resist* switch, collectors on the map,
  hits on the player's boss outside the lair, the evacuation played out (its result is computed: two crates per
  able man and one small object per two men are saved), and inspectors walking the floor (an inspection is one
  roll on the ground floor's loose crates, eased by front workers and front furniture).
- Recruit and bribes are permanent institutions on the map rather than markers; takeovers are started by clicking
  the racket. Campaigns have 4 steps (5 against the Vitales); a sit-down that fails can kill the boss.
- Tier counts sworn families' rackets as yours.

## As built: the law, in the C++ vertical slice (2026-10-31)

This section overrides *Heat*, *Raids* and *Busts and moving house* above for the current game. Numbers are at the top of
`src/game/sim/law_sim.h`; `CLAUDE.md` says how it works.

- **Heat bands bring visits.** Quiet: nobody. Noticed: one investigator about every 30 game hours. Hot: one about every 16. Scorching: up to three, about every 10.
- **Evidence** is a number from 0 to 100, on the HUD with its raid mark, added only when an investigator leaves and reports. Crates count 0.5, 1 or 3 by kind, a
  quarter of that for those that fit on Storage shelves; a gun rack 6, a workbench 2; 1 per $1,000 outside the vault, up to 10; 0.5 per man loafing. A clean report
  takes 3 off. It fades by 0.25 an hour below heat 40, and a Bribe takes 12 off.
- **A raid** comes at heat 60 and evidence 60 together, with six game hours' warning (no lookouts or captain needed: a Tip-off is a possible later feature).
  There is no search result and no clean-up command: every raid is what the design calls a bust.
- **Moving house** keeps the layout and loses all crates, half the built objects at random, 30% of loose money and 15% of what's in the vault. Men in the building are
  arrested at 50% for 24 hours. Nothing is carried out, and no choice of new front is offered: the same map is the new premises. Heat and evidence go to 0.
- **The third raid** ends the game, with a summary and a new game. There's no indictment at heat 100.
- **Contraband** went from heat 9 to 18 a shipment, and each crate not on a shelf adds 0.1 heat an hour, so that high-value goods carry real risk.
- **Not built:** rival families and assaults, inspectors as a separate system, resist switch, clean-up, lookouts, loyalty, wages.
