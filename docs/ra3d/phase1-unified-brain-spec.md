# RA3D Phase 1: unified brain specification

Status: working specification

Date: 22 September 2026

This document is normative for the RA3D brain work. It defines the simulation boundary
that OpenCNC3D and RA3D must share. The implementation may reveal details that require a
revision, but it must not silently create a second simulation model.

The companion evidence inventory is `docs/ra3d/td-ra-delta.md`. The overall schedule and
current phase are tracked in `docs/RA3D_PHASES.md`.

## 1. Product contract

One installed brain must run GDI, Nod, Allied and Soviet participants in the same match.
OpenCNC3D and RA3D are presentation and content modules over that brain.

The frontend selected by a player may control:

- menu and HUD presentation;
- default faction choices;
- campaigns, music, movies and speech;
- local visual quality and optional visual effects;
- which compatible maps are highlighted.

The frontend may not control:

- simulation rules;
- object statistics or prerequisites;
- random-number generation;
- pathfinding results;
- authoritative visibility;
- weapon, armor or resource behavior;
- network-event interpretation;
- victory and defeat.

Every participant in a match runs the same brain build, rules profile, gameplay catalog
and map. An OpenCNC3D client and an RA3D client may look different locally, but they must agree
on every simulated state transition.

## 2. Foundation and source layout

The leading implementation direction is to use the Red Alert engine lineage as the
starting point for the new shared brain because it already contains vessels, naval
factories, ore and gems, more production categories, country houses and the later
special-unit systems.

This is not permission to replace TD semantics with RA semantics. The shared brain is the
union of both engines:

- Red Alert capabilities are retained and generalized.
- TD-only movement classes, weapons, warheads, theaters, missions and special weapons are
  retained.
- Original behavior is selected by a versioned rules profile.
- Game-specific enum ordinals never cross an ABI, network, map, save or replay boundary.

The existing patched TD brain remains the reference implementation until the unified
brain passes TD regression gates. It is not a permanent second multiplayer authority.

### 2.1 Relationship to Standard, XL and Win98 builds

Module compatibility and simulation-capacity compatibility are separate concerns.

- The new unified standard brain is the required authority for normal OpenCNC3D and RA3D
  mixed matches.
- The current TD brain remains available only as a migration reference and legacy
  fallback until parity is proven. It cannot join a unified-brain match.
- An XL brain may continue to raise map, player or heap limits, but it must use the same
  canonical identities, module data and event meanings.
- Standard and XL clients may join each other only if their negotiated simulation version,
  limits and state behavior are proven wire compatible. Otherwise the handshake refuses
  them before the match starts.
- Win98 may use reduced presentation, but it must run the same authoritative simulation
  version to enter a mixed match. A visual fallback never creates a gameplay fork.

Once the unified brain reaches TD parity, `td_classic_v1` means a rules profile inside the
unified brain. It does not mean returning to the old TD-only library.

## 3. Initial content scope

The first complete RA3D release targets base Red Alert content.

Counterstrike, Aftermath, ant missions and carrier support are reserved extension
namespaces. Their source hooks must not be deleted or made impossible, but their gameplay
content and balancing are not on the critical path to the first complete base-game
release.

This scope has three effects:

1. Base RA objects receive stable IDs now.
2. Expansion IDs use a separate namespace and may be added without renumbering base IDs.
3. Expansion code that affects base-game serialization or ABI layout must still be
   accounted for during the brain migration.

## 4. Identity model

The original engines overload house enums with faction, country, player slot and color.
The unified brain separates them.

### 4.1 Identities

`ModuleId`
: Local presentation identity, initially `opencnc3d` or `ra3d`. Not simulation state.

`PlayerSlotId`
: Stable match seat in the range 0 through the negotiated player limit minus one. This
  determines command ownership and network routing, not faction.

`HouseId`
: Runtime simulation owner. Human players, AI players, neutral civilians and scripted
  mission houses all receive a HouseId. A HouseId is serialized explicitly and is never
  inferred from array position.

`FactionId`
: Technology-tree and strategic identity. Initial playable values are GDI, Nod, Allied
  and Soviet. Neutral and scripted houses have no playable faction unless the scenario
  assigns one.

`CountryId`
: Optional faction subtype. Base RA countries are England, France, Germany, Greece,
  Spain and Turkey under Allied, plus USSR and Ukraine under Soviet. TD houses normally
  have no country subtype.

`ColorId`
: Player remap color. It is independent of faction, country and player slot.

`AllianceSet`
: Runtime diplomacy state keyed by HouseId. Faction does not imply an alliance.

### 4.2 Required behavior

- Two players may choose the same faction and different colors.
- Allied countries may apply country bonuses without becoming separate technology trees.
- GDI and Nod may use the same eight-player seat model as RA.
- Neutral and scripted houses may own objects without occupying a human player slot.
- The local frontend has no effect on the faction a player may select in Combined rules.

## 5. Stable type and instance identifiers

### 5.1 Canonical type keys

Every networked or serialized definition has an immutable lowercase ASCII canonical key.
Examples:

```text
core.faction.gdi
core.faction.nod
core.faction.allied
core.faction.soviet
td.unit.mammoth_tank
ra.unit.heavy_tank
ra.vessel.submarine
td.weapon.obelisk_laser
ra.weapon.tesla_zap
core.armor.steel
core.resource.tiberium
ra.resource.ore
ra.resource.gems
```

The prefixes reserved by the project are:

- `core.*` for concepts shared by the engine;
- `td.*` for Tiberian Dawn definitions;
- `ra.*` for base Red Alert definitions;
- `raexp.*` for Counterstrike, Aftermath and other expansion definitions;
- `map.*` for definitions embedded by a scenario;
- `mod.<owner>.*` for future third-party content.

Canonical keys are permanent identifiers. Display names can change. Canonical keys cannot
be renamed after release; aliases may point an old key to a replacement.

### 5.2 Binary TypeId

The ABI and network representation is `uint64_t TypeId`, calculated as FNV-1a 64 over the
canonical key's ASCII bytes.

Requirements:

- Manifests and source maps store the canonical key, not an enum ordinal.
- Generated C/C++ headers may expose named TypeId constants for core content.
- The loader recalculates every TypeId and refuses duplicate canonical keys.
- The loader refuses a hash collision between different canonical keys.
- A sorted list of canonical key plus gameplay definition is part of the gameplay hash.
- Missing keys are fatal for simulation content and diagnostic for optional visual data.

The 64-bit value is deliberately larger than the original enums. Stable identity and safe
extension matter more than saving four bytes in an event.

### 5.3 ObjectInstanceId

Live objects use a separate `uint32_t ObjectInstanceId` allocated monotonically from 1 at
match start. Zero means no object. IDs are not reused during a match.

The original heap index may remain an internal implementation detail, but it may not be
sent as the authoritative identity. This prevents a destroyed object slot from later
referring to a different object in a delayed event.

Saves serialize the next instance ID. Replays obtain IDs from deterministic creation
order. Exhaustion is a fatal deterministic error, not wraparound.

## 6. Rules profiles

The brain supports versioned rules profiles rather than trying to make one balance table
serve every purpose.

Initial profiles:

`td_classic_v1`
: GDI and Nod using faithful TD rules. RA factions and RA-only maps are unavailable.

`ra_classic_v1`
: Allied and Soviet using faithful base RA rules. TD factions are unavailable.

`combined_v1`
: GDI, Nod, Allied and Soviet in one deliberately balanced ruleset. This is the required
  cross-module multiplayer profile.

The profile selects definition records and global constants, not alternate engine code.
A bug fix in shared deterministic behavior applies to every profile unless preserving a
documented classic quirk is necessary for compatibility.

## 7. Resources and economy

### 7.1 Resource definitions

The shared resource model supports at least:

- `core.resource.tiberium`;
- `ra.resource.ore`;
- `ra.resource.gems`.

Each resource definition contains:

- value per harvested density unit;
- maximum cell density;
- growth and spread policy;
- legal terrain and germination rules;
- damage or environmental effects, if any;
- visual overlay family;
- minimap and editor presentation;
- cargo presentation;
- classic-profile availability.

Tiberium and ore may grow or spread according to the active rules. Gems do not spread in
`ra_classic_v1` or `combined_v1` unless a later explicit rule changes that behavior.

### 7.2 Combined-match policy

In `combined_v1`, every standard faction harvester can collect Tiberium, ore and gems.
Every standard refinery accepts those resources. This prevents a faction from becoming
unplayable because a mixed map contains the other game's economy.

Harvested cargo retains its resource identity until unloading. A refinery converts it to
stored value using the active resource definition. The house then holds:

- liquid credits;
- stored harvested value;
- storage capacity.

This preserves the original engines' spendable-credit behavior without requiring three
incompatible wallets. Silo loss and capacity rules operate on stored harvested value.
The resource mix in storage need not affect spending after conversion.

Classic profiles retain their original resource availability and values. A classic map
that contains an unsupported resource is rejected at load rather than silently converted.

## 8. Production and technology

Production uses these shared domains:

1. structure;
2. infantry;
3. ground vehicle;
4. aircraft;
5. vessel;
6. special weapon or ability.

Every factory definition declares the domains it can produce, its exit rules, docking
rules, build-speed contribution and whether it may become the primary factory for a
domain.

Every buildable definition declares:

- canonical TypeId;
- faction and optional country availability;
- required and forbidden prerequisite expressions;
- producing domain and compatible factory tags;
- tech level and scenario restrictions;
- cost and build time per rules profile;
- build limit and uniqueness rules;
- power and storage effects;
- granted abilities or superweapons.

Prerequisites are expressions over canonical building tags, not bit masks over StructType
ordinals. Fake buildings may satisfy only the prerequisite tags explicitly assigned to
them.

The HUD may group or hide domains differently, but it sends canonical production commands
to the same brain.

## 9. Terrain and movement

The shared land-type union is:

- clear;
- road;
- water;
- rock;
- wall;
- resource field;
- beach;
- rough;
- river.

The shared locomotor union is:

- foot;
- tracked;
- wheeled;
- harvester;
- hover;
- surface vessel;
- submarine;
- winged aircraft.

Harvester and hover remain distinct even though base RA removed their TD SpeedType enum
entries. Submarine is distinct from surface vessel because occupancy, targeting and
visibility differ even where path costs are shared.

Movement cost and passability are data tables selected by rules profile and locomotor.
No serialized value uses the original SpeedType or LandType ordinal.

Theater identity is separate from land type. Initial canonical theaters are TD Desert,
TD Jungle, TD Temperate, TD Winter, RA Temperate, RA Snow and RA Interior. A presentation
alias such as OpenCNC3D's current Snow repaint cannot impersonate a native RA template catalog
in simulation data.

## 10. Combat model

The common armor set remains none, wood, aluminum, steel and concrete. Both source engines
already share these five semantic categories.

Weapons and warheads use canonical TypeIds. A weapon definition includes:

- projectile or instantaneous delivery handler;
- warhead;
- damage, range, reload and burst behavior;
- valid target domains;
- anti-air, anti-ground, anti-surface and anti-submarine capability;
- arcing, homing, ballistic and line-of-sight policy;
- visual and audio references that do not affect simulation;
- per-profile overrides.

A warhead includes a fixed-point effectiveness table against every armor type plus flags
for infantry death behavior, wall damage, resource destruction, splash, fire, nuclear,
Tesla, dog and mechanical repair or damage behavior.

All calculations that can affect the match use fixed-point or integer arithmetic. Visual
interpolation may use floating point but does not feed results back into the brain.

TD-only warheads and weapons, including laser, particle beam, chemical and dinosaur
attacks, remain valid. RA-only Tesla, dog, nuclear, mechanical, naval and expansion
weapons occupy their own canonical keys.

## 11. Special units and superweapons

Special behavior is exposed through deterministic capabilities attached to canonical
types. Capabilities include:

- cloak;
- submarine submerge and surface;
- detect cloak or submarine;
- crush or be crushable;
- heal infantry;
- repair mechanical objects;
- spy infiltration;
- steal credits;
- plant or clear mines;
- transport passengers or vehicles;
- deploy into a structure;
- chrono movement;
- temporary invulnerability;
- radar jam or gap generation.

Capabilities may select a named deterministic handler implemented by the brain. They are
not arbitrary client scripts.

The shared superweapon model contains a TypeId, owner HouseId, source object or granted
state, charge duration, suspend policy, target policy, visibility policy and deterministic
effect handler.

The initial union includes:

- TD Ion Cannon, Nuclear Strike and Air Strike;
- RA Sonar Pulse, Nuclear Strike, Chronosphere, Parabombs, Paratroopers, Spy Plane, Iron
  Curtain and GPS;
- any support abilities required to reproduce the base campaigns.

## 12. Maps and source compatibility

The unified runtime uses one canonical in-memory scenario model. Importers translate TD
and RA source formats into it.

A canonical map records:

- format version;
- map and theater TypeIds;
- rules profile;
- required gameplay namespaces and content hash;
- dimensions and playable bounds;
- terrain/template keys and height data;
- overlays and typed resources;
- object TypeIds, HouseIds, health, facing and mission state;
- player starts and waypoints;
- triggers, teams and scripts using canonical keys;
- deterministic map options.

Existing OpenCNC3D TD maps remain supported through a TD importer. Native RA maps use an RA
importer. Original local enum numbers are translated at import time and never stored in a
new canonical map.

Map export must include an explicit source format. Features that cannot be represented in
that source format cause a clear refusal, not silent data loss.

## 13. Saves, replays and scripts

### 13.1 New save envelope

Unified-brain saves use a versioned envelope containing:

- magic and save-schema version;
- brain ABI and simulation version;
- rules-profile TypeId;
- gameplay-catalog hash;
- map hash and canonical map identity;
- simulation tick and speed-independent clock state;
- deterministic RNG state;
- next ObjectInstanceId;
- player, house, alliance and object state;
- trigger, team, factory, resource and superweapon state;
- renderer metadata needed to restore the local presentation.

Raw pointers, vtables and compiler-dependent struct dumps are prohibited in the new
format.

Existing OpenCNC3D binary saves are not guaranteed to migrate into the unified brain. Phase 3
must either provide a tested TD legacy importer for a specific old save version or reject
it with a precise message. Silent or partial loading is forbidden. Existing maps are a
required compatibility target; existing binary saves are not.

### 13.2 Replays

Replays store the same compatibility header, initial map identity, player setup and the
ordered canonical command stream. They do not store presentation events. A replay with a
different simulation version or gameplay hash is rejected unless an explicit compatible
reader exists.

### 13.3 Scripts

New scripts address types, factions, houses, weapons and resources by canonical key.
Legacy TD script names are translated by the TD importer. RA script names are translated
by the RA importer. Numeric source enums may be accepted only inside a versioned legacy
importer.

## 14. Multiplayer contract

The simulation remains deterministic lockstep. Before a seat becomes ready, every client
must agree on:

- protocol magic and protocol version;
- brain ABI version;
- simulation version;
- rules-profile TypeId and rules hash;
- gameplay-catalog hash;
- map hash;
- enabled expansion namespaces;
- tick rate and deterministic options;
- player-seat, HouseId, faction, country and color assignments;
- initial RNG seed;
- command delay and event-format version.

The protocol transmits explicitly sized little-endian fields. It never sends a compiler
struct, pointer, C++ enum width or native `bool`.

Every gameplay command contains:

- event-format version;
- execute tick;
- issuing PlayerSlotId and HouseId;
- monotonically increasing command sequence for that player;
- canonical command kind;
- ObjectInstanceIds and TypeIds as required;
- fixed-width command payload;
- integrity check as selected by the transport layer.

Clients periodically compare a canonical simulation-state digest. A mismatch stops the
match with a diagnostic bundle containing the last agreed tick, first divergent tick,
per-system hashes and recent canonical commands.

Gameplay and presentation hashes are separate. Missing enemy visuals prevent a normal
player from joining because the match would be unusable, but they do not alter the
simulation hash. A future headless server may legally omit presentation content.

## 15. Compatibility policy

The compatibility levels are:

1. **Wire compatible:** may join the same match.
2. **Replay compatible:** may read the same command recording.
3. **Save compatible:** may load the same complete state.
4. **Map compatible:** may import the same source map.
5. **Presentation compatible:** has the assets needed to display the content.

These claims are independent and versioned. Matching marketing version strings are not
evidence of compatibility.

Rules for refusal:

- A wire, rules, map or gameplay hash mismatch refuses before the match starts.
- A missing simulation TypeId is fatal.
- A missing optional visual may use an explicit diagnostic placeholder only in developer
  mode.
- A save or replay schema mismatch gives a precise unsupported-version message.
- No mismatch falls back to a nearby enum, object name or local definition.

## 16. Determinism and tests required by later phases

The shared brain needs tests at three levels:

- golden single-player scripts for representative TD and RA scenarios;
- mirrored mixed-faction command streams with per-tick state hashes;
- long-running multiplayer and AI stress matches.

The state digest must cover at least RNG state, houses, alliances, credits and storage,
objects, missions, targets, cargo, factories, resources, shroud-authoritative state,
triggers, teams, superweapons and victory state. It must not cover camera position,
animation interpolation, audio, particles or other presentation-only state.

## 17. Phase 1 decisions still open

The architecture above is implementable without resolving these balance details now, but
they must be settled before `combined_v1` is declared playable:

- exact combined resource values and growth rates;
- whether country bonuses remain active in Combined rules;
- final four-faction cost and timing balance;
- mixed-map default resource distribution;
- whether classic quirks are reproduced exactly or only behaviorally;
- whether a legacy TD save importer is worth its maintenance cost.

These are rule-content decisions, not reasons to fork the simulation architecture.
