# Native macOS release products —25 October2026

The design review requested the same five product names on macOS and Windows. This helper
builds the macOS half from the vetted, current-source Mac base stage and the
same-version public module ZIPs/catalog:

```sh
tools/mac/make-release-packages-mac.sh --version 0.9.0 \
  --base-stage /path/to/vetted/no-RA/mac-stage \
  --module-dir /path/to/verified/module-assets --out /path/to/new/output
```

It emits `OpenCNC3DInstaller-macos-vVERSION.pkg`,
`OpenCNC3DComplete-macos-vVERSION.pkg`, `RA3DComplete-macos-vVERSION.pkg`,
`OpenCNC3DBinaries-macos-vVERSION.zip` and `RA3DBinaries-macos-vVERSION.zip`,
plus a source/input/artifact SHA256 receipt. Existing outputs are never silently
replaced. The two binary ZIPs are patches for an otherwise complete matching
module, with its own frontend/brain/SDL layout, the native launcher/bundle and
licence notices; they carry no game data packs, saves or CFG files.

All three native products offer only the current-user home domain and install
under `~/Applications/OpenCNC3D`. Bundle relocation is disabled, so native
Installer does not find and replace a different copy behind an existing Dock
shortcut. Existing settings/saves/custom maps are absent from payload ownership.
Preflight refuses links in every named payload path, unfinished journals and
processes using the destination. The exact source base stage stays unchanged.

The empty installer contains only launcher/bundle, SDL, bootstrap menu/cursor art
and notices. Add Game owns game selection and game-specific optional-media
choices afterward. It performs no game or media download during installation.

Each Complete product carries a SHA256-verified offline module cache. Native
Installer's Customize page offers independent Music and Movies checkboxes,
initially unchecked. Its evaluated choice matrix selects exactly one hidden
script component for none/music/movies/both. That component first invokes:

```text
opencnc3d-launcher --dir INSTALL --install-module td|ra3d \
  --module-file ZIP --module-sha SHA256
opencnc3d-launcher --dir INSTALL --media-install none|music|movies|movies,music \
  --media-game td|ra --media-add-only
```

The native controller verifies and installs the offline game under its existing
lock and rollback journal before any media download. Once the game is in, the
launcher removes that package and its notes from `.opencnc3d-modules/` (about 4 GB
a game); a failed install keeps it, and the launcher installs from it offline. The additive installer flag
leaves unchecked components and their previous preferences untouched; on a fresh installation the controller records honest
unchecked defaults. Settings retains its separate intentional-removal action.
A failed base install stops; optional network/media failure keeps the validated game installed and its scoped partial
state available for retry in game content settings. Scripts drop to the recipient
before executing any installed controller. They never open a game window.

The welcome/conclusion pages embed the unchanged 160×160 accepted v05 emblem:
`art/interface/launcher-enhanced-v05/CNC-native.png` for platform/Dawn and
`RA-native.png` for RA3D. These are the approved shared shiny ring and established
RA battle artwork. No generation, palette replacement or model work occurs.
The native system controls use their platform typography. Actual visual approval
of a new Installer window is not inferred from package/container checks.

`launcher/mac_launch.c` now recognizes a valid launcher/menu bootstrap without a
Dawn brain, so Empty and RA-only installations launch their own platform. The
old full-Dawn direct fallback and interrupted-update `.old` recovery remain.

Verification commands (no GUI or actual user installation):

```sh
python3 tools/mac/installer/test_wrapper.py
python3 tools/mac/installer/test_scripts.py
python3 tools/mac/installer/test_additive_media.py /path/to/new/native/launcher
python3 tools/mac/installer/test_packages.py
python3 tools/mac/installer/test_os_floor.py
```

The macOS floor has one source, `OPENCNC3D_MACOS_MIN` in
`tools/mac/deployment-target.sh`, read through `tools/mac/os_floor.py`. Every native
product carries an `installation-check` that refuses an older macOS with a sentence
naming the macOS it found and the one needed, before any location is offered;
`allowed-os-versions` stays as the backstop. Without the check, an older macOS
disables the startup disk with an empty reason and, the home folder being the only
allowed location, Installer shows its generic "cannot install in this location" line
for every location. The welcome page and the release notes state the floor, and
`bootstrap()` and the module reader refuse any Mach-O slice that asks for a newer
macOS. `test_os_floor.py` proves the refusal text by building the same inert product
with the floor raised to 99.0 and only querying Installer about it.

The package fixture builds three actual native products from inert universal
executables. It expands/re-hashes real payloads and components, asks macOS
Installer to evaluate all media combinations, checks both checkboxes remain
visible/enabled, and verifies the distinct binary ZIPs. Installer metadata
queries require normal access to the macOS Installer services; a restricted
sandbox can list the home domain but fail to resolve it. The script fixture
substitutes only destination resolution/privilege dropping and an inert CLI;
it checks offline-before-media order, all 8 scoped combinations, failed-base stop,
optional retry, existing media/preferences under every choice, a destructive-call
negative control, link/journal refusal and unchanged private CFG. The actual
additive-media CLI test runs the native headless controller against tiny pinned
existing files and all four choices in each game, preserving unchecked
wanted/state fields, the other game and current CFG. No HTTP is needed for those
already-complete files. The wrapper test
compiles its actual predicate without entering app main or calling osascript.
Temporary fixture/payload trees are retired after verification.

These are source/container/controller checks, not a claim of a performed native
user install, media download, first launch or Gatekeeper acceptance. Actual
release artifacts and current-main installed runtime proof remain separately
required. Products are unsigned/unnotarised unless an existing Developer ID
Installer identity is explicitly supplied through
`OPENCNC3D_MAC_INSTALLER_IDENTITY`; the receipt records that distinction.

3 November 2026: welcome and conclusion explicitly identify the user's home
folder and `~/Applications/OpenCNC3D`; the conclusion names the app there.
The unchanged emblem also has HTML width/height 160, in addition to CSS, for
native HTML renderers. The inert native package fixture passes all three
products. All six welcome/conclusion pages now also pass native offscreen WKWebView
rendering at 560×440 logical pixels: unchanged 160×160 emblems, no overflow,
and inspected actual captures. The render manifest is in
`docs/verification/2026-11-03-open-bugs/installer-webkit.json`. This is native
WebKit-content fitting, not a visible Installer-chrome review. The design review confirmed
no Developer ID Application/Installer certificates are available on any Mac;
signing/notarization remain blocked. No security exception was added.


3 November 2026 startup guidance follow-up: all five Mac products now use the
tracked player guide in `tools/launchers/READ-ME.txt` (SHA-256 `07b99f050546c0d50b0074ada106ffd9c6467636d5618c0b56eadd73f7629da9`).
Full ZIPs already copy that master. Native PKGs embed it in
`READ-ME-INSTALLATION.txt`; binary patches include it after their matching-install
warning in `BINARIES-README.txt`. The guide distinguishes empty/Complete/update
products, native Apple Silicon/Intel support, macOS14 minimum, home-folder
installation, full-folder ZIP extraction and the current Add Game/content flow.
Obsolete setup/launcher counts and settings claims were removed. It links Apple's
current security explanation without promising that an unsigned app passes
Gatekeeper. Three actual inert PKGs and two binary ZIP fixtures pass with the
new guide; no visible installer, security exception or public release was made.

Emblem and location pages: productbuild copies into a product only the Resources
files its Distribution names, so a `module.png` beside `welcome.html` never reached
a released product, and Installer drew an empty box with the alt text where the
emblem belongs. The emblem now travels inside each page as a `data:` URI
(`emblem_uri()`), and `verify_pages()` runs on every product `build_installer()`
makes: a page that links to a file the product does not hold, or that does not say
the game goes in `~/Applications` rather than the main Applications folder and how
to find it, stops the build. `test_product_pages.py` (gate G540) proves both against
the real productbuild, and with `--product A.pkg ...` asks any built or released
product the same question. The player guide now gives the System Settings ->
Privacy & Security -> Open Anyway steps that macOS 15 and later require for an
unsigned download, so its SHA-256 above is that of an earlier revision.
