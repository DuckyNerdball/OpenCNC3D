# G520 to G524: the health bars and the post chain, the cloaking shimmer up close, a
# building's health bar at the nearest zoom, and the rim on the player's own cloaked tank, run by game/gates.sh. Scenes and claims in gate_cloakbars.py.
#
# STANDALONE: run from inside a play folder that has TiberianDawn.dylib, SCG01EA.pack,
# missions/SCM01EA, ra3d/RedAlert.dylib and ra3d/3d/SCG01EA.pack staged:
#
#   cd playable && sh ../game/gate_cloakbars.sh
#
# It defines ok/bad/skip/gbegin/grun itself when game/gates.sh has not, prints one PASS
# or FAIL line per gate and exits non-zero if any failed. Every run is headless
# (--script); nothing raises a window. About a minute.

if ! command -v ok >/dev/null 2>&1; then
  OUT=${OUT:-/dev/null}
  PASS=0; FAIL=0; SKIPPED=0
  ok()  { echo "PASS  $1"; PASS=$((PASS+1)); }
  bad() { echo "FAIL  $1"; FAIL=$((FAIL+1)); }
  skip() { echo "SKIP  $1"; SKIPPED=$((SKIPPED+1)); }
  gbegin() { GRC=0; }
  grun() {
    ge_log="$1"; shift
    ./cnc_eyes "$@" >"$ge_log" 2>&1; ge_rc=$?
    [ "$ge_rc" = "0" ] || GRC="$ge_rc"
    return "$ge_rc"
  }
  CB_STANDALONE=1
  GATEDIR="$(cd "$(dirname "$0")" && pwd)"
fi

CB_TD="--cameos cameos.pack --dospack dossidebar.pack --dosinf dosinfantry.pack --dylib TiberianDawn.dylib --content content/ --dist 3000 --scen SCG99EA --pack SCG01EA.pack --dir gcb/td/ --noshroud --nosound --w 1280 --h 720 --texset 3"
CB_RA="--ra3d --dylib ra3d/RedAlert.dylib --content ra3d/data --dosdata ra3d/audio --scen SCG88EA --pack ra3d/3d/SCG01EA.pack --dir gcb/ra/ --noshroud --nosound --w 1280 --h 720 --seed 7 --texset 3"

# cb_pair GAME SCRIPT LOGNAME FLAGS...: run one script twice, into gcb/GAME/hb and
# gcb/GAME/nohb, the second time with no health bars
cb_pair() {
  cb_g=$1; cb_s=$2; cb_l=$3; shift 3
  for cb_k in hb nohb; do
    mkdir -p "gcb/$cb_g/$cb_k"
    sed "s#SHOTS/#gcb/$cb_g/$cb_k/#" "gcb/$cb_g/$cb_s" > "gcb/$cb_g/$cb_k-$cb_s"
    if [ "$cb_k" = nohb ]; then cb_x=--nohb; else cb_x=; fi
    if [ "$cb_g" = td ]; then cb_a="$CB_TD"; else cb_a="$CB_RA"; fi
    grun "gcb/$cb_g/$cb_k-$cb_l.log" $cb_a --enhanced $cb_x "$@" --script "gcb/$cb_g/$cb_k-$cb_s"
  done
}

rm -rf gcb && python3 "$GATEDIR/gate_cloakbars.py" maps gcb 2>>"$OUT"
if [ ! -s gcb/td/SCG99EA.INI ]; then
  bad "G520 no shadow beside a health bar: the test mission was not made (missions/SCM01EA is not in the run folder)"
  bad "G521 no health bar colour in a cloaked tank: the test mission was not made"
  bad "G522 cloak shimmer grid at the nearest zoom: the test mission was not made"
  bad "G524 the rim on the player's own cloaked tank: the test mission was not made"
else
  # ===================================================================================
  # G520 UNDER ENHANCED THE GROUND BESIDE A HEALTH BAR IS THE GROUND (fx_overlay_depth_keep
  # and fx_overlay_stamp in fx_post.h, called before draw_health_bars in cnc_eyes.cpp).
  # Seen before: a dark slab on the ground beside every bar, on any unit or building, and
  # a soft halo round it: the bar writes depth (as the cartridge's does, so a building in
  # front hides it) and the contact march and the occlusion took it for a solid object.
  # Tiberian Dawn's solid Stealth Tank selected at the default and the nearest zoom under
  # Enhanced and under N64, Red Alert's Medium Tank selected at the nearest zoom under
  # Enhanced, each shot with and without the bars (--nohb). Beside each bar, over its
  # lower two thirds, every column from two pixels right of it out to a bar's height is
  # on average at most 3 levels darker with the bar than without it.
  # PROVEN TO FAIL on the build before: the columns next to the bar 10 to 20 levels darker
  # under Enhanced in both games.
  # ===================================================================================
  gbegin
  cb_pair td solid_enh.txt solid_enh
  cb_pair td solid_n64.txt solid_n64
  if [ -f ra3d/RedAlert.dylib ] && [ -f ra3d/3d/SCG01EA.pack ]; then
    cb_pair ra solid_enh.txt solid_enh
    G520R=$(python3 "$GATEDIR/gate_cloakbars.py" shadow gcb/ra "Red Alert" 2>>"$OUT")
  else
    G520R="1 Red Alert not staged in this folder, not run"
  fi
  # the cloaked scene's shots join G521's directory; the solid ones are G520's
  mkdir -p gcb/tds/hb gcb/tds/nohb
  for k in hb nohb; do mv gcb/td/$k/enh_*.png gcb/td/$k/n64_*.png gcb/tds/$k/ 2>/dev/null; done
  G520T=$(python3 "$GATEDIR/gate_cloakbars.py" shadow gcb/tds "Tiberian Dawn" 2>>"$OUT")
  echo "$G520T" >> "$OUT"; echo "$G520R" >> "$OUT"
  if [ "$GRC" != "0" ]; then
    bad "G520 no shadow beside a health bar: a run failed (GRC=$GRC)"
  elif [ "${G520T%% *}" = "1" ] && [ "${G520R%% *}" = "1" ]; then
    ok "G520 no shadow beside a health bar: ${G520T#1 }; ${G520R#1 }"
  else
    bad "G520 no shadow beside a health bar: ${G520T#? }; ${G520R#? }"
  fi

  # ===================================================================================
  # G521 THE SHIMMER OVER A CLOAKED STEALTH TANK NEVER TAKES A HEALTH BAR'S COLOUR
  # (fx_cloak_shimmer_apply in fx_post.h, reading fx_overlay_depth_keep's copy). Seen
  # before: the selected cloaked tank carried green blocks of its own bar on the side
  # nearest it, because the shimmer copies each pixel from a few 1995 pixels away and the
  # bar was in the picture it copied from.
  # The player's cloaked tank selected at the default and the nearest zoom under Enhanced:
  # over its selection box right of the bar, no pixel of the bar's green.
  # PROVEN TO FAIL on the build before: 12 and 30 green pixels in the tank.
  # ===================================================================================
  gbegin
  cb_pair td cloak.txt cloak
  G521=$(python3 "$GATEDIR/gate_cloakbars.py" leak gcb/td G521 2>>"$OUT")
  echo "$G521" >> "$OUT"
  if [ "$GRC" != "0" ]; then
    bad "G521 no health bar colour in a cloaked tank: a run failed (GRC=$GRC)"
  elif [ "${G521%% *}" = "1" ]; then
    ok "G521 no health bar colour in a cloaked tank: ${G521#1 G521: }"
  else
    bad "G521 no health bar colour in a cloaked tank: ${G521#? G521: }"
  fi

  # ===================================================================================
  # G522 AT THE NEAREST ZOOM THE SHIMMER'S GRID IS NO COARSER THAN AT THE DEFAULT ZOOM
  # (cloak_shimmer_mask in cnc_eyes.cpp). Seen before: the 1995 pixel the shimmer steps
  # on grew with the zoom, and at the nearest zoom on a large window it moved whole slabs
  # of a cloaking tank sideways, which read as a model drawn in broken chunks.
  # G521's runs: the probe after each shot says the shimmer reached the tank and the size
  # of its 1995 pixel; at the nearest zoom it is at most 1.05 times the default zoom's (the
  # camera tilts a little as it nears, which alone gives 1.02).
  # PROVEN TO FAIL on the build before (with the probe's field): 1.56 times.
  # NOT COVERED: the old camera (cammode old), whose zoom keeps the grid as it was.
  # ===================================================================================
  G522=$(python3 "$GATEDIR/gate_cloakbars.py" grid gcb/td/hb-cloak.log 2>>"$OUT")
  echo "$G522" >> "$OUT"
  if [ "${G522%% *}" = "1" ]; then
    ok "G522 cloak shimmer grid at the nearest zoom: ${G522#1 G522: }"
  else
    bad "G522 cloak shimmer grid at the nearest zoom: ${G522#? G522: }"
  fi

  # ===================================================================================
  # G523 UNDER ENHANCED A BUILDING'S HEALTH BAR IS NO TALLER ON SCREEN AT THE NEAREST ZOOM
  # THAN AT THE DEFAULT ONE (hb_height_scale in cnc_eyes.cpp). Seen before: a selected
  # Airfield's bar, the cartridge's world length of three cells and more, ran at the
  # nearest zoom as a long thin line from the corner of the screen to the building.
  # Tiberian Dawn's Airfield and Weapons Factory under Enhanced and under N64, and Red
  # Alert's Airfield under Enhanced, each selected and framed up and left of the middle as
  # in the report, at the default zoom and the nearest, with and without the bars. Claims:
  # the bar's length in the dump is the cartridge's (dimw/24 cells) at the default zoom and
  # under N64, and that times the distance over the default's at Enhanced's nearest; and in
  # the pictures under Enhanced the bar at the nearest zoom is at most 1.15 times as tall as
  # at the default zoom, its top inside the picture.
  # PROVEN TO FAIL on the build before: the length unchanged at the nearest zoom (an
  # Airfield's 3.21 cells where 2.05 is wanted), Red Alert's Airfield bar 174 pixels from
  # the picture's top edge against 118 at the default zoom, the Weapons Factory's 140
  # against 94. N64 passes both: it keeps the cartridge's bar to its own nearest zoom.
  # ===================================================================================
  gbegin
  mkdir -p gcb/g523/td/hb gcb/g523/td/nohb gcb/g523/ra/hb gcb/g523/ra/nohb
  for cb_m in enh n64; do
    for cb_k in hb nohb; do
      sed "s#SHOTS/#gcb/g523/td/$cb_k/#" gcb/tdb/tall_$cb_m.txt > gcb/g523/$cb_k-td_$cb_m.txt
      if [ "$cb_k" = nohb ]; then cb_x=--nohb; else cb_x=; fi
      grun gcb/g523/$cb_k-tall_td_$cb_m.log --cameos cameos.pack --dospack dossidebar.pack \
           --dosinf dosinfantry.pack --dylib TiberianDawn.dylib --content content/ --dist 3000 \
           --scen SCG98EA --pack SCG01EA.pack --dir gcb/tdb/ --noshroud --nosound --w 1280 \
           --h 720 --texset 3 --enhanced $cb_x --script gcb/g523/$cb_k-td_$cb_m.txt
    done
  done
  if [ -f ra3d/RedAlert.dylib ] && [ -f ra3d/3d/SCG01EA.pack ]; then
    for cb_k in hb nohb; do
      sed "s#SHOTS/#gcb/g523/ra/$cb_k/#" gcb/ra/tall_enh.txt > gcb/g523/$cb_k-ra_enh.txt
      if [ "$cb_k" = nohb ]; then cb_x=--nohb; else cb_x=; fi
      grun gcb/g523/$cb_k-tall_ra_enh.log $CB_RA --enhanced $cb_x --script gcb/g523/$cb_k-ra_enh.txt
    done
  fi
  G523=$(python3 "$GATEDIR/gate_cloakbars.py" tall gcb/g523 gcb/g523 2>>"$OUT")
  echo "$G523" >> "$OUT"
  if [ "$GRC" != "0" ]; then
    bad "G523 a building's health bar at the nearest zoom: a run failed (GRC=$GRC)"
  elif [ "${G523%% *}" = "1" ]; then
    ok "G523 a building's health bar at the nearest zoom: ${G523#1 G523: }"
  else
    bad "G523 a building's health bar at the nearest zoom: ${G523#? G523: }"
  fi

  # ===================================================================================
  # G524 THE PLAYER'S OWN CLOAKED VEHICLE WEARS A FLICKERING PALE RIM; AN ENEMY'S NEVER
  # DOES (fx_cloak_shimmer_apply in fx_post.h, from the mask's blue, which only the
  # player's own ghost lowers). The cloak test map twice, each with one of the two
  # Stealth Tanks moved far off and the camera on the other: the player's tank must show
  # rim pixels while cloaking, once cloaked and at the nearest zoom, and the Nod tank none
  # in the same shots, since a rim on an enemy would give a hidden tank away.
  # Seen red before the rim: no rim pixels round the player's tank at all.
  # -----------------------------------------------------------------------------------
  gbegin
  for cb_s in own nod; do
    mkdir -p gcb/td_$cb_s/shots
    sed "s#SHOTS/#gcb/td_$cb_s/shots/#" gcb/td_$cb_s/rim.txt > gcb/td_$cb_s/rim-run.txt
    grun gcb/td_$cb_s/rim.log --cameos cameos.pack --dospack dossidebar.pack \
         --dosinf dosinfantry.pack --dylib TiberianDawn.dylib --content content/ --dist 3000 \
         --scen SCG99EA --pack SCG01EA.pack --dir gcb/td_$cb_s/ --noshroud --nosound --w 1280 \
         --h 720 --texset 3 --enhanced --script gcb/td_$cb_s/rim-run.txt
  done
  G524=$(python3 "$GATEDIR/gate_cloakbars.py" rim gcb G524 2>>"$OUT")
  echo "$G524" >> "$OUT"
  if [ "$GRC" != "0" ]; then
    bad "G524 the rim on the player's own cloaked tank: a run failed (GRC=$GRC)"
  elif [ "${G524%% *}" = "1" ]; then
    ok "G524 the rim on the player's own cloaked tank: ${G524#1 G524: }"
  else
    bad "G524 the rim on the player's own cloaked tank: ${G524#? G524: }"
  fi
fi
# CB_KEEP=1 keeps the scenes and shots in gcb/ to be looked at
[ -n "$CB_KEEP" ] || rm -rf gcb

if [ -n "$CB_STANDALONE" ]; then
  echo "gate_cloakbars: $PASS passed, $FAIL failed"
  [ "$FAIL" = "0" ]
fi
