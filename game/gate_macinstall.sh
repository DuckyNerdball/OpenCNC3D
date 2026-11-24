# G540: the Mac installer's own pages, run by game/gates.sh.
#
# Standalone, from a built play folder (nothing is installed, no window opens):
#   cd playable && sh ../game/gate_macinstall.sh
# Inside game/gates.sh the block runs as it is: it only defines ok/bad/skip/OUT when they
# are not defined already.
#
# G540  THE MAC INSTALLER SHOWS ITS EMBLEM AND SAYS WHERE THE GAME WENT
# (tools/mac/installer/package.py welcome(), emblem_uri(), verify_pages()).
# The welcome and last pages of every Mac installer drew their 160x160 emblem from a
# module.png beside them. productbuild copies into a product only the Resources files the
# Distribution names, so no released product held the picture: Installer drew an empty
# box with the alt text where the emblem belongs. The emblem now travels inside each page
# as a data: URI. The game installs into ~/Applications, which is not the Applications
# folder in the Finder sidebar, so both pages also say it is not the main Applications
# folder and the last page says how to find the app.
# tools/mac/installer/test_product_pages.py builds inert products with package.py's own
# functions and the real productbuild, expands them and reads the pages; package.py runs
# verify_pages() on every product it builds for a release as well.
# PROVEN TO FAIL by the test's own control, the old arrangement built the same way: the
# product comes out without module.png and verify_pages() refuses it. The shipped
# v0.9.11 products (all three) hold only welcome.html and conclusion.html.
# NOT COVERED: the Installer window itself; the pages are read from the product, and were
# looked at in an offscreen WebKit render, not in Installer.

if ! command -v ok >/dev/null 2>&1; then
  OUT=${OUT:-/dev/null}; PASS=0; FAIL=0
  ok()   { echo "PASS  $1"; PASS=$((PASS+1)); }
  bad()  { echo "FAIL  $1"; FAIL=$((FAIL+1)); }
  skip() { echo "SKIP  $1"; }
  GATEDIR="$(cd "$(dirname "$0")" && pwd)"
fi

rm -rf g540 && mkdir -p g540
G540T="$GATEDIR/../tools/mac/installer/test_product_pages.py"
if ! command -v productbuild >/dev/null 2>&1 || ! command -v pkgutil >/dev/null 2>&1; then
  skip "G540 the Mac installer pages: no macOS packaging tools here"
elif [ ! -f "$G540T" ]; then
  bad "G540 the Mac installer pages: $G540T is missing"
else
  python3 "$G540T" > g540/test.txt 2>&1; G540RC=$?
  cat g540/test.txt >> "$OUT"
  G540P=$(grep -c '^MAC-INSTALLER-PAGES|PASS|' g540/test.txt)
  G540OK=$(grep -c '^ok ' g540/test.txt)
  G540F=$(grep -c '^FAIL ' g540/test.txt)
  if [ "$G540RC" = "0" ] && [ "$G540P" = "1" ] && [ "$G540F" = "0" ] && [ "$G540OK" -ge 26 ]; then
    ok "G540 the Mac installer pages carry their emblem inside the page and say the game is in ~/Applications, not the main Applications folder, and how to find it: $G540OK checks, the old module.png arrangement refused"
  else
    bad "G540 the Mac installer pages: rc=$G540RC(0) pass=$G540P(1) ok=$G540OK(>=26) failed=$G540F(0): $(grep -m3 '^FAIL ' g540/test.txt | tr '\n' ' ')"
  fi
fi

if [ -z "${GP_IN_SUITE:-}" ] && [ "$(basename -- "$0")" = "gate_macinstall.sh" ]; then
  echo "----- gate_macinstall: $PASS pass, $FAIL fail -----"
  [ "$FAIL" = "0" ]
fi
