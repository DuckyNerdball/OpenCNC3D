# River Raid - Cinematic Bases

Open OpenCNC3D from the Dock, choose User Maps, then River Raid - Cinematic Bases.
The player controls GDI. Nod remains an enemy. This is a filming sandbox with
no scripted victory, defeat, production waves or base assaults.

The original River Raid terrain tiles, rivers, bridges and crossings are intact.
There are two main compounds and two smaller outposts, 41 buildings, four
harvesters, parked armour, sentries and three looping Nod squads of three soldiers.
Concrete wall ends use four-cell square posts. Sandbags leave every bridge clear.
Gates and patrol lanes remain open, and the lanes are cleared of tiberium.
The fitted building plots use level ground in the supplied corner heightmap.

For filming, turn Fog Of War off in Options > Cheats. F5 > Free Cam provides
camera movement. Main GDI is northwest, its outpost is southwest of the upper
river, main Nod is southeast beside the bridge, and its fenced outpost is northeast.
Camera centres in map cells are approximately (17,16), (22,30), (46,51), (55,12).

Source files are USER60.INI, USER60.BIN and USER60.HGT. Rebuild them with
`python3 tools/maps/make-river-showcase.py`. The BIN is byte-identical to SCM04EA.BIN.
The source map is singleplayer and appears in the User Maps list, rather than the
skirmish lobby. Both packagers include its three runtime files.

Hidden runtime verification covered 2,400 engine ticks: all four harvesters moved
and entered refineries, stored GDI tiberium increased, all nine patrol soldiers
moved on their loops and retained full health, and all scripted counts passed.
The installed map was also opened through the launcher's User Maps menu harness.
