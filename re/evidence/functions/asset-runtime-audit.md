# PC asset/runtime coverage audit

Status: complete extension-level coverage audit for the extracted PC archive;
runtime boundaries are proven for the gameplay families and negative results
are bounded for console/tool-only families
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Source: `build/assets/all-pkr/files` (`ALL.PKR`, 3,771 extracted files)

This audit checks every extension in the extracted PC package against the
runtime evidence files. Counts come from the read-only `tony assets inventory`
report over the extracted tree; they are corpus coverage, not a claim that the
PC executable opens every file in the extension.

## Extension coverage

| Extension | Corpus count | Runtime classification | Evidence |
| --- | ---: | --- | --- |
| `.PSX` | 282 | common parser; scene, skater, animation, item/medal, vertex-colour/BITS, post-model-tag, and inline-texture variants reach separate runtime consumers | [asset-loading.md](asset-loading.md), [psx-tags-runtime.md](psx-tags-runtime.md), [animation-runtime.md](animation-runtime.md), [items-runtime.md](items-runtime.md), [texture-runtime.md](texture-runtime.md) |
| `.TRG` | 32 | trigger node tables relocate into level/object-manager, script-object, rail, camera-point, resource-selection, gap/objective, linked-restart, level-event, and trick-object paths | [trg-runtime.md](trg-runtime.md), [gap-runtime.md](gap-runtime.md), [restart-runtime.md](restart-runtime.md), [level-event-runtime.md](level-event-runtime.md), [level-load.md](level-load.md), [trick-object-runtime.md](trick-object-runtime.md) |
| `.PRE` | 19 | embedded-resource catalog and common backend fast path; individual payloads feed the same PSX/FNT/WAV/legacy loaders | [pre-runtime.md](pre-runtime.md), [fileio-runtime.md](fileio-runtime.md) |
| `.PKR` | 1 (`ALL.PKR`) | package directory/file records feed the common resource backend | [fileio-runtime.md](fileio-runtime.md), [legacy-assets.md](legacy-assets.md) |
| `.PSH` | 106 | C-header-style player/skater part manifests; 104 same-base PSX pairs plus two explicit alias/editor manifests | [skater-asset-runtime.md](skater-asset-runtime.md), [player-runtime.md](player-runtime.md) |
| `.BMP` | 2,850 | PC bitmap lookup/upload path; four Warehouse hashes and player/UI paths provide concrete witnesses | [texture-runtime.md](texture-runtime.md), [player-runtime.md](player-runtime.md), [font-runtime.md](font-runtime.md) |
| `.FNT` | 26 | font-slot/glyph/image resource loader and text consumer | [font-runtime.md](font-runtime.md), [text-runtime.md](text-runtime.md) |
| `.WAV` | 312 | VAB-selected sound names become PCM buffers and DirectSound resources | [audio-runtime.md](audio-runtime.md) |
| `.REC` | 11 | replay/video header, packed frame stream, and paired card-buffer consumers | [replay-runtime.md](replay-runtime.md) |
| `.PRK` | 50 | custom-park decode, grid/item generation, and finalized runtime region | [custom-park-runtime.md](custom-park-runtime.md) |
| `.TXT` | 22 | `CDPARKS.TXT`, `MUSIC.TXT`, and `CREDITS.TXT` have text consumers; 19 other text files are bounded tool/debug data | [text-runtime.md](text-runtime.md), [legacy-assets.md](legacy-assets.md) |
| `.BIN` | 25 | `TRICKS.BIN` and `CRETEX.BIN` have runtime consumers; 23 other files are console/frontend/editor blobs with no PC filename consumer | [bin-runtime.md](bin-runtime.md), [legacy-assets.md](legacy-assets.md) |
| `.HET` / `.HEP` / `.HED` / `.WAD` | 1 each | correlated legacy metadata tables; the extracted `CD.WAD` is zero-filled and is not a proven PC payload source | [legacy-assets.md](legacy-assets.md), [file-formats.md](../../notes/file-formats.md) |
| `.SFX` | 25 | console/build-time sound metadata; PC playback opens named `audio/*.WAV` resources instead | [audio-runtime.md](audio-runtime.md), [legacy-assets.md](legacy-assets.md) |
| `.SEQ` / `.SBL` / `.TST` / `.TDF` / `.TAG` / `.TS` / `.NT` | 1 each | console/tool/debug/installer data; no filename, magic, or game-owned open boundary in this PC build | [legacy-assets.md](legacy-assets.md) |

## Runtime convergence

The active asset families converge at these stable PC boundaries:

```text
ALL.PKR / PRE
    -> common game-owned open and synchronized buffer
    -> PSX parser / TRG parser / FNT / WAV / replay / legacy text loaders

PSX scene or named model
    -> relocated model/object/material records
    -> collision, animation, player, item, texture, or renderer consumer

TRG node or script
    -> relocated node table / link records / gameplay object
    -> object manager, script, trick, resource-selection, or camera consumer
```

## Executable-referenced external data

The archive extension audit is complemented by a filename-string audit of the
same executable. These inputs live outside `ALL.PKR` and are therefore not
included in the 3,771-file count:

| Input family | Runtime classification | Evidence |
| --- | --- | --- |
| `THPS2_*.SAV` / `*.SAV` | Windows save-game/MMU manager; career, replay, and custom-park buffers with `SC` headers; the career payload has a recovered image/record schema | [save-runtime.md](save-runtime.md), [career-save-runtime.md](career-save-runtime.md) |
| `*.STR`, `GrayMat.dat`, `Intro.dat`, `LTIX30.dat` | startup movie/media and configured setup probes; no gameplay-object handoff | [startup-runtime.md](../startup-runtime.md) |
| `texture.dat` | CD/audio TOC authentication table; not a scene texture source | [cd-check.md](../cd-check.md) |
| `park%d.prk` | Windows saved custom-park input; generated level/runtime region | [custom-park-runtime.md](custom-park-runtime.md) |

This separates packaged asset ownership from user-state and startup-media
ownership. A file-like string alone is not treated as a loader: each row has a
game-owned consumer or is explicitly classified as a setup/authentication
probe.

The runtime evidence is therefore complete at the extension/family level: no
active extension is missing a top-level loader or consumer note. It is not
complete at every semantic field or every rare asset instance. Those are
listed as bounded follow-up questions in [asset-loading.md](asset-loading.md),
especially renderer enum names, surface predicates, uncommon player-selection
rows, and legacy module/SFX producers.

## Negative boundary

The unconnected formats are not silently routed through a proven loader:

- The 23 non-`TRICKS.BIN`/`CRETEX.BIN` `.BIN` files have console/tool entry
  signatures and no case-insensitive filename reference in the PC executable.
- The 25 `.SFX` files have no matching `.sfx` open path; the PC sound loader
  constructs `audio/<name>.wav` from hardcoded/VAB-selected descriptions.
- `SKATE.SEQ`, `SKATE2.SBL`, `SKNY.TST`, `SYMBOLS.TDF`, `SKATE2.TAG`,
  `TRICKS.TS`, and `ANSI.NT` have no filename or format-magic consumer in the
  exact PC image.
- `CD.HET`/`CD.HEP`/`CD.HED` correlate as metadata, but the local `CD.WAD` is
  all zeroes and overlapping HED ranges prevent a faithful raw-payload load.

These negative results are bounded to the executable and extracted corpus
identified above. A future PC caller or nonzero payload source would reopen
only the affected row; it would not change the proven PSX/TRG/PRE paths.
