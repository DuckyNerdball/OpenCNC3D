# Water scenes

Scenes in the format of `../scenes/README.md` that the water's gate (G410) runs beside the
framed set.

M1 and M2 end one mission and start the next in the same process, with the script verb
`remission` (game_shutdown followed by game_boot, the pair the menu shell runs between two
missions). They show what one mission leaves behind for the next: anything a mission's end
frees or keeps is drawn again in the second mission's frames.

Run any of them with `tools/mem/ab.py --scenedir tools/mem/scenes-water`.

E1 and E2 open the map editor, which rewrites the ground in place and so rebuilds the
ground surface and the sea from the sheets on every frame it draws. Each times 20 of those
frames with `framebench` (one FRAMEBENCH| line, the median and 90th percentile in ms), so
the cost of how those rebuilds read the card can be compared between two runs.

| scene | game, mission | modes | what is framed |
|---|---|---|---|
| M1 | Red Alert SCG01EA (snow), twice | Enhanced | the lake and its coast at tick 1 and 300 of the first mission; the lake and the Soviet base at tick 1, the lake at tick 300 of the second |
| M2 | Tiberian Dawn SCG01EA (temperate), twice | Enhanced | the beach and the sea at tick 1 and 300 of the first mission; the beach and the Nod guns at tick 1 and 300 of the second |
| E1 | Tiberian Dawn SCG01EA (temperate), map editor | Enhanced | the beach and the sea, 20 frames timed |
| E2 | Red Alert SCG01EA (snow), map editor | Enhanced | the lake and its coast, 20 frames timed |
