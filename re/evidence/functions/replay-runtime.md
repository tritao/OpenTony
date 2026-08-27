# Replay/card asset runtime path

Status: confirmed replay-file read, header/record extraction, replay/video
restart application, highlight selection/advance, and paired replay-buffer
load/save paths
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x0045337a`, `0x00469400`, `0x0047b1a0`, `0x0047b2d0`,
`0x0047b6e0`, `0x0046f490`

Replay files are a separate runtime family from level geometry. The PC image
contains both named file paths and game-owned consumers:

```text
DEMO*.REC
  -> 0x00449030 resource open
  -> 0x0046f490 allocation/read wrapper
  -> 0x0047b2d0 replay header + five-record extraction
  -> replay globals / demo state
```

A second path handles larger card-style replay buffers:

```text
replay resource
  -> 0x0047b6e0
  -> first 0x7fe00-byte runtime buffer
  -> optional second 0x7fe00-byte runtime buffer
```

## Corpus and header extraction

The local corpus contains eleven `.REC` files, all 24064 bytes. `DEMO.REC`
starts with:

```text
51 0a 00 00  3d 01 7f 01  a1 0f 00 00  85 00 ca 00
f0 0e 00 00  a0 09 df 09  65 0a 00 00  46 00 7f 00
```

The exact load function is `0x0047b2d0`. It accepts a resource name, opens it
with `0x00449030`, and uses `0x0046f490(handle, 1, 1, 0)` to obtain a loaded
buffer. The wrapper's allocation path is game-owned and returns the payload
pointer after its internal prefix.

After the read/synchronization calls, `0x0047b2d0` extracts these fields:

```text
buffer +0x00       -> DAT_0056e5f8       u32 GVideoRestart.NumFrames
buffer +0x2c       -> DAT_0056b8d4       u16 GVideoRestart.NumSkaters
buffer +0x2e       -> DAT_0056b8d6       u8  GVideoRestart.Skater
buffer +0x2f       -> DAT_0056b8d7       u8  GVideoRestart.Skater2
buffer +0x32       -> DAT_0056b8da       u16 GVideoRestart.Level
buffer +0x34       -> DAT_0056b8dc       u16 GVideoRestart.GGame
buffer +0x04..+0x2b -> five 8-byte highlight ranges -> DAT_0056e62c/...
```

The five records are copied as two 32-bit words each. A nearby replay-state
consumer prints each pair with the format `GVideoRestart.Highlights = {%d,
%d}`, independently supporting a five-entry start/end highlight table. The
loader then frees the temporary file buffer and returns the `+0x2c` 16-bit
skater-count value to its caller. This establishes the replay header's
disk-to-runtime contract without assigning semantics to the larger playback
buffers.

The video-restart consumer at `0x00469400` applies the same header to the
runtime restart state. It first delegates to `0x004cdc20` when normal replay
mode is active; otherwise it loads the rotating `DemoA.rec` name through
`0x0047b6e0`, copies the first header words into the video-restart globals,
copies the five 8-byte highlight records into `DAT_0056e62c` onward, validates
that the header contains one or two skaters, and publishes the skater-selection
values. In the two-skater case it repeats the header/highlight copy for the
second stream. This is the direct header-to-runtime consumer for the named
video-restart asset, separate from the bit-packed playback reader.

## Header consumer and video-restart state

The replay header has a concrete consumer in the frontend/video-restart state
around `0x0045337a`. That branch loads the literal `DemoA.rec`, calls
`0x0047b2d0`, and immediately applies the extracted globals:

```text
DemoA.rec
  -> 0x0047b2d0 header extraction
  -> DAT_0056b8d4 == 2 -> two-skater video-restart flag DAT_00561c70
  -> 0x004b18a0 -> video-restart manager slots DAT_00561c88/+0x04
  -> 0x004b18d0(header skater/header skater2)
  -> DAT_0056b8da -> current replay level DAT_00561c90
  -> DAT_0056b8dc -> replay game/state DAT_00561c74
  -> DAT_00561990 = 1 (video restart active)
```

This independently confirms that `+0x2c` is the skater-count selector, `+0x2e`
and `+0x2f` are the two skater selections, `+0x32` selects the level, and
`+0x34` selects the replay game/state. The five highlight ranges remain
available through `DAT_0056e62c` and are printed by the nearby replay UI; their
playback application is not claimed here.

The larger card-buffer path is also called from the same replay state machine
at `0x00469400`: it passes two destination pointers and `DemoA.rec` to
`0x0047b6e0`, which copies the two `0x7fe00` blocks into the replay runtime
buffers before advancing the replay-resource state. This ties the header and
large-buffer paths to one named replay asset without conflating their sizes.

## Internal stream storage and card-file layout

The replay manager allocates its internal storage in `0x004cba30`. It requests
`0x80000` bytes for the first active skater and, in two-player mode, a second
`0x80000`-byte buffer. The stream-state globals expose the layout:

```text
state 0 +0x04  buffer base                 DAT_0056e5dc
state 0 +0x08  recording stream/header area DAT_0056e5e0 = base + 0x200
state 0 +0x0c  internal stream boundary    DAT_0056e5e4 = base + 0x4800

state 1 +0x04  buffer base                 DAT_0056e454
state 1 +0x08  recording stream/header area DAT_0056e458 = base + 0x200
state 1 +0x0c  internal stream boundary    DAT_0056e45c = base + 0x4800
```

`0x004cbce0` initializes one state block for record or playback mode. Record
mode clears the full 0x80000-byte buffer, writes the header/custom-skater
metadata through `0x004cb810`, and starts the bit writer at the reserved
stream area. `0x004cdc20` performs the inverse playback setup: it validates
and loads the one- or two-player buffers through `0x0047b400`, points the
playback state at the loaded data, extracts the level/game/header fields, and
copies the highlight ranges into the active playback globals.

The saved file relationship is exact. `0x0047b1a0` calls `0x004cb810` for
each active state, then writes the first `0x7fe00` bytes of the first buffer
and, when present, the next `0x7fe00` bytes for the second player. The load
side accepts the same `0x80000` transfer contract and validates the package
entry/complete-read result before playback starts. Thus the 24,064-byte
extracted demo files are only the small corpus examples; the runtime card
format is a pair of fixed-size stream chunks, with the final internal 0x200
bytes reserved by the recorder rather than serialized.

`0x004cb810` is the header writer used by both save and record setup. It
copies the fixed replay state prefix, writes the game mode at header `+0x34`,
level at `+0x32`, player count/skater selectors, custom-skater records when
needed, and the money/stat masks. `0x004cc190` then appends one variable-size
bit-packed frame, using `0x004ccc70` as the matching writer for the reader
already recovered above. This closes the disk-buffer ↔ bitstream ↔ gameplay
object relationship without pretending that the individual optional event
fields are fixed-width.

After the level session starts, the video code consumes the replay state per
skater. `0x004cd750` selects the first playback block at `DAT_0056e5d8` or the
second at `DAT_0056e450`. Its `+0x24` word is the remaining-frame gate: a
nonzero value enters ordinary replay input/frame processing, while zero enters
the highlight transition path and prints `Replay: No More Frames`.

The highlight transition is a real runtime consumer, not just UI metadata:

```text
0x004cd750 no frames
    -> current highlight cursor at playback state +0x50
    -> selected highlight start/end slot at +0x54 + index * 8
    -> 0x004cbc40 choose next skip distance
    -> 0x004cbbb0 advance one or more simulation frames
    -> 0x004ccd70 replay-input/frame application for each skater
```

`0x004cbbb0` advances the requested number of frames by applying the replay
input to the active skater(s), clearing the game-object list between steps,
and restoring the transient list state. `0x004cbc40` selects the next range
from the compacted highlight table and subtracts the current range start from
its end. `0x004cb6e0` prepares that table before playback: it clamps range ends
to `NumFrames - 1`, repeatedly selects the earliest pending range, clears its
source marker, and compacts the surviving ranges. This proves the five header
ranges feed playback control rather than being decorative records.

## Bit-packed replay frame stream

The low-level frame consumer is also recoverable. Each per-skater playback
block embeds a little-endian, least-significant-bit-first reader:

```text
playback +0x14  current u32 stream-word pointer
playback +0x18  buffered stream word
playback +0x1c  bits remaining in buffered word
playback +0x24  replay frames remaining
```

`0x004cd5a0` reads 1..32 bits and carries reads across word boundaries;
`0x004cd690` sign-extends a requested field; and `0x004cd710` consumes one
flag bit. `0x004ccd70` decrements the frame counter and decodes one skater
frame from this stream. The frame contains eight 20-bit signed channels,
shifted by 12 into fixed-point runtime values, followed by eight 16-bit
channels. The decoded position is copied into the gameplay object's
`+0x08/+0x0c/+0x10` coordinates and the previous position into its
`+0xbc/+0xc0/+0xc4` fields before optional state/animation/event fields are
applied. The exact semantic label of every channel is not required to
reproduce the bitstream or the object handoff; only the final few event fields
remain open.

This is the first direct disk/runtime contract for replay payload data beyond
the header: the runtime does not consume a fixed C struct per frame. It
consumes a variable bit-packed stream through the state block's reader and
writes the same gameplay-object fields used by ordinary physics/rendering.

Replay exit/reset is also explicit. `0x004cbf30` flushes the current skater's
replay input block through `0x004cbf70`, clears its active word, and resets the
global highlight cursor. `0x004cbc90` clears both per-skater playback blocks.
The frontend clears the video-restart flag after `Front_LaunchGameLevel`
returns and restores both manager slots through `0x004b18d0`, so replay asset
state does not leak into the next ordinary session.

The debug shortcut must use the ordinary game result, not merely a result that
eventually displays the `PLAY_GAME` screen.  In the result dispatch after
`0x004532aa`, `0x26` enters the normal game path.  `0x2a` instead loads
`DemoA.rec`, sets the video-restart flag, and then enters the same screen with
replay mode active.  Its next level loop dispatches replay mode (playback state
`+0x00 == 2`) through `0x004cd750`; the frame decoder can then read a stale
9-bit trick-object index and walk past the live `TrickObjectListHead` chain at
`0x0056db90`, faulting at `0x004cd257` when the link becomes null.  Clearing
the replay mode words before writing `0x2a` does not prevent the fault because
the replay transition recreates that state.  The helper now writes `0x26` so
the normal frontend teardown and game-launch path remain active.

## Paired replay buffers

`0x0047b6e0` is the companion load path used by the replay/card state code. It
opens the supplied resource with `0x00449030`, reads it through
`0x0046f490`, synchronizes the file layer, copies the first `0x7fe00` bytes
into the caller's destination, and—when a second destination is non-null—
copies the next `0x7fe00` bytes there. It frees the temporary input buffer
after the copies. The lower `0x0047b400` path performs the equivalent package
offset/size validation for the active replay stream and rejects incomplete or
wrong-type entries.

`0x0047b1a0` is the matching write path. It opens a destination stream with
the resource backend, writes one `0x7fe00` block and an optional second block,
then closes the stream. The executable references the strings `DemoA.rec` and
`demo.rec` from this replay/card state machine, so these are operational file
names rather than corpus-only labels.

The buffer size is an observed transfer contract. It does not prove that each
24064-byte extracted `.REC` contains two complete blocks; the card path may
target a larger device-backed resource or use a different resource variant.

## Recreation boundary

```text
DEMO*.REC
  -> FileIO_OpenResource (0x00449030)
  -> FileIO_AllocateLoaded (0x0046f490)
  -> Replay_LoadHeader (0x0047b2d0)
  -> header fields + five 8-byte runtime records
  -> replay/demo state consumers
```

And for the card/buffer variant:

```text
named replay resource
  -> Replay_LoadCardBuffers (0x0047b6e0)
  -> one or two 0x7fe00-byte caller-owned buffers
```

## Confidence and limits

- `confirmed`: resource open/read/allocation calls; extracted offsets and
  `GVideoRestart` field meanings; the `DemoA.rec` header consumer and
  two-skater/level/game application; five-record highlight copy shape; paired
  `0x7fe00` load and save transfers; highlight clamping, compaction, playback
  selection, frame skipping, reset/exit, and the per-skater no-more-frames
  consumer; per-skater 0x80000 stream allocation; 0x200/0x4800 internal
  boundaries; header serialization; variable bit-packed frame writing; one- or
  two-player 0x7fe00 save/load chunks; package validation; corpus count, sizes,
  and sample bytes.
- `observed`: the replay/card state-machine references to `DemoA.rec` and
  `demo.rec`, the exact bit-reader state, and the event/animation fields after
  the decoded position channels.
- `open`: the semantics of optional replay event fields after the decoded
  position/state/animation channels.

## Native recreation boundary

`src/assets/replay_asset.*` owns the fixed `0x38`-byte replay header and
exposes the variable stream through an independent least-significant-bit-first
reader. It preserves the five highlight ranges, skater selectors, level/game
words, the `0x200` stream origin, and the proven eight signed 20-bit channels
followed by eight signed 16-bit channels. `ReplayAsset::card_transfer()` also
models the two-player `0x7fe00` transfer size. Synthetic bitstream coverage and
the real `DEMOA.REC` header both pass.
