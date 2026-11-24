# Public export comment-span review

Reviewed source `83c70ea1ccf22862acf02d37ad85d99fc2791642`. The strict export guard refused stale file hashes after the second-sun, defaults and launcher fixes. The replacement proposal was generated separately and inspected before adoption.

Four source files changed identity: `game/cnc_eyes.cpp`, `game/fx_post.h`, `game/gates.sh` and `launcher/launcher.c`. All 122 selected spans contain exactly the same before/after comment bytes as the approved manifest, including multiplicities. Only source hashes, line numbers and byte offsets changed. The other 48 manifest entries are unchanged. Each selected comment was reviewed; parser and executable-token equivalence checks passed while preparing the proposal. No license text or executable bytes are changed by these edits.

The export must still pass its complete strict guard and tracking checks. This review does not authorize public source publication or remote changes. Public snapshots remain manual user actions only.

## Combined vehicle-effect source

The subsequent current-main runtime `7ff00ea096e948d2b5401d80187ab13bbda88ec5` moved the renderer comments again. The authoritative guard was rerun against its renderer in the local-only source review tree. All 116 selected renderer spans retain exactly the approved before/after comment bytes, with matching multiplicities and parser/token proof. The 51 other manifest entries remain unchanged. This updates identity and offsets only; source publication remains a manual user action.

## Release fixture isolation

The G245 measurement window now excludes an unrelated bright background pixel while retaining its existing geometry and centring assertions. The three previously reviewed shell-comment spans at lines 6085, 6093 and 6341 remain byte-for-byte unchanged, at the same offsets and with the same approved before/after hashes. Only the source identity for `game/gates.sh` changes in the export manifest. The complete strict export guard remains mandatory. No public publication is authorized.
