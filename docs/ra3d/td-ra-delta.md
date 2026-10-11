# RA3D Phase 1 evidence: Tiberian Dawn and Red Alert delta

Date: 22 September 2026

This is the source-driven feature inventory behind the unified brain specification. It
records the meaningful differences that must have a destination in the combined model.
It is not an asset checklist. Visual asset coverage remains tracked separately by the
RA3D roadmap and later asset manifests.

Source anchors:

- TD core enums: `brain/vanilla/tiberiandawn/defines.h`
- RA core enums: `brain/vanilla/redalert/defines.h`
- TD remaster ABI: `brain/vanilla/tiberiandawn/dllinterface.cpp` and `.h`
- RA remaster ABI: `brain/vanilla/redalert/dllinterface.cpp` and `.h`
- OpenCNC3D patch: `brain/patches/vanilla-opencnc3d.patch`
- Renderer brain binding: `game/cnc_eyes.cpp`

## Architectural differences

| Concern | Tiberian Dawn | Red Alert | Unified destination |
|---|---|---|---|
| Runtime object kinds | Infantry, unit, aircraft, building and world objects | Adds vessel and vessel-type RTTI plus explicit house, factory, team and trigger RTTI | Union of all kinds with stable canonical TypeIds |
| Factions and houses | GDI, Nod, neutral, scripted and multiplayer houses | Allied and Soviet countries, GDI/Nod compatibility houses, neutral, scripted and eight multiplayer houses | Separate HouseId, PlayerSlotId, FactionId, CountryId and ColorId |
| Production | Infantry, unit, aircraft and building production | Adds vessel production and naval factories | Six shared production domains |
| Resources | Twelve Tiberium overlay families | Four ore and four gem families, while much internal code retains Tiberium naming | Typed resource definitions over one economy interface |
| Terrain classes | Clear, road, water, rock, wall, resource and beach | Adds rough and river | Union land-type table |
| Locomotion | Foot, track, dedicated harvester, wheel, winged, hover and float | Foot, track, wheel, winged and float | Preserve union and add explicit submarine behavior |
| Theaters | Desert, Jungle, Temperate and Winter | Temperate, Snow and Interior | Canonical theater IDs, native template catalogs kept distinct |
| Missions | Includes Timed Hunt | Adds queued move and harmless, removes Timed Hunt | Union mission model selected by capability and rules |
| Special weapons | Ion Cannon, Nuclear Strike and Air Strike | Sonar, Nuclear Strike, Chronosphere, Parabombs, Paratroopers, Spy Plane, Iron Curtain and GPS | Shared data record plus deterministic effect handlers |
| Armor | None, wood, aluminum, steel and concrete | Same semantic set | One core armor registry |
| Maps and scripts | TD scenario assumptions and enum names | RA scenario assumptions, naval and expanded triggers | Source-specific importers into one canonical scenario |
| Multiplayer | OpenCNC3D patches extend TD for current internet play | RA source has its own later remaster ABI but none of the OpenCNC3D project exports | One versioned canonical event ABI |

## Object-system union

Red Alert's RTTI adds categories not present in TD's RTTI list:

- cell;
- factory;
- house and house type;
- team type;
- trigger and trigger type;
- vessel and vessel type.

TD retains abstract/object RTTI labels that do not appear in RA's public list. Internal
class hierarchy details may remain engine-specific, but no RTTI ordinal becomes a public
type identity.

## Factions, countries and houses

TD defines GDI and Nod directly as houses. RA defines England, Spain, Greece, Germany,
France and Turkey as Allied countries, USSR and Ukraine as Soviet countries, and also
retains GDI and Nod compatibility entries.

This proves that the original `HousesType` cannot be the combined player identity. The
unified model separates runtime ownership, faction technology, country bonus, seat,
alliance and color.

## Resources

TD exposes Tiberium overlays 1 through 12. RA exposes ore overlays 1 through 4 and gem
overlays 1 through 4. RA's implementation still names many methods and fields Tiberium,
but selects GoldValue or GemValue from the overlay type.

The combined model therefore generalizes the existing harvest pipeline rather than
creating independent wallets or independent harvester classes.

Required semantic differences:

- Tiberium, ore and gems retain independent values and visuals.
- Growth and spread policy is per resource and per rules profile.
- Gems are higher-value and do not use normal ore spreading under the base RA rules.
- Mixed matches need a policy that prevents faction-specific resource starvation.
- Resource identity must appear in map, editor, renderer dump and save state.

## Production and prerequisites

Both engines use factories and buildable RTTI categories. RA's remaster interface handles
`RTTI_VESSELTYPE` alongside infantry, unit, aircraft and building types.

The combined design replaces category enums and structure bit masks at external
boundaries with canonical production domains, factory tags and prerequisite expressions.
This is required because TD and RA StructType positions disagree even where names match.

## Movement and terrain

The two engines share most land semantics, but RA adds rough and river. TD retains
dedicated harvester and hover speed classes that base RA's public SpeedType omits. RA
adds a true vessel hierarchy, including submarines, rather than treating every watercraft
as a TD unit.

The combined engine must preserve:

- TD hovercraft and harvester path behavior;
- RA surface vessel behavior;
- RA submarine occupancy, visibility and targeting;
- aircraft behavior from both games;
- road, rough, river, beach, water and resource movement costs;
- bridge transitions between land and water contexts.

## Weapons, warheads and armor

Both games share the five armor categories, making them a stable core concept.

TD-only warhead concepts include laser, particle beam, chemical/spore and dinosaur melee
families. RA adds Tesla, dog, nuclear and mechanical warheads. Weapon lists also diverge
substantially, particularly for naval, Tesla, healing, repair, air-delivered and expansion
weapons.

The unified model keeps every weapon and warhead as a separately named definition. A
matching short name or source enum position is never treated as proof of identical
behavior.

## Unit capabilities present in the union

The shared brain needs deterministic support for:

- cloak and detection;
- surface and submerged naval states;
- transport of infantry and vehicles;
- deployable MCVs;
- harvest and unload;
- crush and crushability;
- engineer capture and repair;
- spy infiltration;
- theft;
- infantry healing;
- mechanical repair;
- dog targeting and attack;
- minelaying;
- radar jamming and gap generation;
- chrono movement;
- temporary invulnerability;
- aircraft docking, rearming and airfield occupancy;
- fixed-wing attack runs;
- paratrooper and parabomb delivery.

TD-only campaign creatures and RA expansion units use reserved content namespaces even
when they are outside the first base-game release scope.

## Structures and world objects

Shared concepts include Construction Yard, Weapons Factory, Refinery, Silo, Helipad, SAM,
Airfield, Power, Advanced Power, Hospital, Repair Facility, Bio Lab, mission structures,
civilian buildings, common walls and crates.

TD-specific gameplay structures include Guard Towers, Advanced Guard Towers, Obelisk,
Hand of Nod, Temple and Communications Center/Eye concepts.

RA-specific gameplay structures include Allied and Soviet technology centers, Iron
Curtain, Chronosphere, Pillboxes, Radar Dome, Gap Generator, AA Gun, Flame Tower, Barracks,
Tent, Kennel, Shipyard, Sub Pen, Missile Silo, Forward Command, Tesla Coil, fake buildings,
fence, mines and barrels.

Matching source names do not imply matching art or statistics. Shared identity is assigned
only when behavior is truly shared. TD and RA versions of visually or mechanically
different objects receive separate canonical keys.

## Aircraft and vessels

TD aircraft include transport helicopter, A-10, Apache, cargo plane and Orca. RA aircraft
include transport helicopter, Badger, U-2, MiG, Yak, Longbow and Hind.

RA vessels include submarine, destroyer, cruiser, transport and patrol boat. Expansion
hooks add missile submarine and carrier.

The current OpenCNC3D renderer has no vessel object kind. The unified ABI and renderer must
add one rather than folding vessels into ground units.

## Superweapons and information systems

TD's sidebar-controlled special set is Ion Cannon, Nuclear Strike and Air Strike.
RA's set is Sonar Pulse, Nuclear Strike, Chronosphere, Parabombs, Paratroopers, Spy Plane,
Iron Curtain and GPS.

Related information systems include normal radar, radar jamming, Gap Generator concealment,
GPS reveal and sonar submarine reveal. These must modify authoritative simulation state or
visibility through shared handlers. A frontend-only visual implementation would desync
targeting and player knowledge.

## Campaign and scenario differences

Both engines use teams, triggers, missions, waypoints and scenario INI data, but their
available objects, missions, theater catalogs, effects and special actions differ. RA also
depends on naval placement and Interior maps.

The canonical scenario must represent the union. TD and RA importers translate source
names and ordinals. Export to an older source format refuses unsupported features.

## Existing OpenCNC3D-specific gap

The project patch currently modifies common and Tiberian Dawn sources, not Red Alert.
The renderer dynamically resolves project-specific exports such as object dumping,
selection information, probing, lockstep event exchange, rally points, scripting and ABI
facts. Red Alert's stock remaster interface does not provide the complete OpenCNC3D contract.

Phase 2 proves that RA can boot through the infrastructure. Phase 3 must port and then
generalize the OpenCNC3D exports before the unified brain can replace the current TD brain.

## Inventory conclusion

Every major source-level difference has a destination:

- unique content becomes canonical data;
- shared mechanics become core definitions;
- divergent classic behavior becomes a rules-profile override;
- later RA mechanics become shared deterministic capabilities;
- source enums remain inside legacy importers and engine adapters;
- presentation differences remain module-owned;
- all multiplayer-visible behavior belongs to one brain.
