# PC WAV sound-effect runtime path

Status: confirmed/live WAV resource load, PCM extraction, DirectSound-buffer creation, and sound-bank handoff
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004ad020`, `0x004ad180`, `0x004f2960`, `0x004f2b40`, `0x004ed9b0`, `0x004f2c70`, `0x004f2e20`

This is the PC sound-effect path. It is distinct from CD-audio authentication
and from the music/movie path: level and front-end VAB selections ultimately
populate runtime sound slots from ordinary `audio/*.wav` resources.

## Bank selection and WAV names

`0x004ad020` maps the front-end VAB names `shell.vab`, `skate2.vab`,
`skate2g.vab`, and `skate2p.vab` to sound-bank numbers 0 through 3. The level
variant at `0x004ad180` maps names including `school2`, `hangar`, `bullring`,
`philly`, `ss`, `venice`, `ny`, and `mar` to bank numbers 4 through 0x11.
Both functions finish by calling `0x004f2960(bank)`.

`0x004f2960` walks the selected bank's 0x42 sound-description entries. For
each populated entry it builds the PC resource name from the static strings:

```text
audio/ + sound-description-name + .wav
```

It calls `0x004f2b40` for the resulting path and stores the returned runtime
sound slot back into the bank table. The loader supports up to 0x80 runtime
sound slots and rejects a new load after that limit.

The table stride is `0x34` bytes (`0x0d` words). The fields independently
consumed by the loader and playback path are:

```text
description +0x00  sound ID; -1 terminates the bank table
description +0x0c  inline WAV resource-name bytes (32-byte field)
description +0x2c  post-start flag copied to runtime slot state bit 1
description +0x30  runtime sound-slot index returned by the WAV loader
```

`DAT_0054ca80 + bank * 0xd68` is the selected bank cursor; the first
description begins 0x30 bytes before that cursor and each subsequent
description advances by 0x34 bytes. The loader tests the description ID before
using the inline name, so a `-1` record terminates the bank before the 0x42
iteration safety bound. The 0x42-entry limit and stride are runtime facts
rather than assumptions about the VAB format. The remaining description words
are not assigned a public meaning because the proven consumers do not read
them on this path.

## WAV file-to-sample conversion

`0x004ed9b0(path, format_out, sample_size_out)` is the exact disk/package
boundary used by the sound loader:

```text
path
  -> 0x004e75b0 abstract resource open
  -> 0x004e79f0 seek-to-end / seek-to-start
  -> 0x004e7ad0 read the complete resource
  -> memory-backed mmioOpenA / RIFF-WAVE parsing
  -> format_out receives 0x12 bytes of WAVEFORMATEX
  -> sample_size_out receives aligned `data` chunk size
  -> return newly allocated PCM data bytes
```

The parser descends the `RIFF`/`WAVE` container, reads the `fmt ` chunk, and
accepts only `wFormatTag == 1` (PCM). It then descends the `data` chunk,
allocates `(data_size + 3) & ~3` bytes, reads the sample payload, closes the
memory-backed MMIO object, frees the complete-file buffer, and returns the
sample allocation. Unsupported/open/read failures return null.

The local extracted corpus independently matches this contract: 312 WAV files
are present under `ALL.PKR/audio`, and all are PCM, 44.1 kHz, 16-bit; 311 are
mono and `THPSSHELL4.WAV` is stereo. For example,
`BULLROAR2.WAV` is 100806 bytes with a 100664-byte `data` chunk and a
`1-channel/44100 Hz/16-bit` format.

## Runtime buffer allocation and fill

`0x004f2b40` finds the first free 0x28-byte sound-bank slot, calls
`0x004ed9b0`, and submits a DirectSound-style buffer descriptor to the audio
device object at `DAT_029d8360`:

```text
descriptor +0x00 = 0x24                 descriptor size
descriptor +0x04 = 0xe2                 creation flags
descriptor +0x08 = decoded sample size
descriptor +0x10 = pointer to WAVEFORMATEX
destination      = DAT_029d6920 + slot * 0x28
```

The descriptor is a 0x24-byte zeroed structure; the nine zeroed dwords are
written before the three observed values and the format pointer. The
`+0x10` location is the field actually passed to the audio-device vtable, not
the end-of-structure padding.

The created buffer is initialized by `0x004f2c70`. It locks the buffer through
the DirectSound object vtable, retries after a lost-buffer result by restoring
the buffer, copies the returned one or two regions from the decoded sample,
unlocks it, and frees the temporary sample allocation. The loaded sound is
therefore a normal runtime buffer, not a pointer into the PKR/PRE file.

The native asset layer in `src/assets/wav_asset.*` now implements the
file-to-sample half of this boundary. It validates RIFF/WAVE, walks `fmt ` and
`data` chunks, preserves the observed WAVEFORMATEX fields, rejects non-PCM
formats, and owns a four-byte-aligned sample allocation. The eventual audio
device adapter can submit those values to a platform buffer without retaining
a pointer into the package or PRE source.

`src/assets/sound_runtime.*` implements the adjacent game-owned table seam.
`SoundBankRuntime` preserves the 0x42 description bound and 0x34 stride
contract, resolves `audio/<name>.wav`, publishes at most 0x80 0x28-byte sound
slots, and applies the proven description `+0x2c` to runtime state bit 1 when
a buffer is marked started. Device-specific buffer pointers and voice mixing
remain adapter-owned.

The surrounding runtime tables are supported at the following boundaries:

```text
DAT_029d6920  0x80 records, 0x28 bytes each; first eight words are buffer pointers
record +0x20   bank/voice state flags
record +0x24   source sound-description pointer
DAT_029d7d20  sound-id to sound-description pointer map
DAT_029d8360  active audio-device object
```

`0x004f2e20(sound_id, volume_a, volume_b)` selects a sound description through
`DAT_029d7d20[sound_id]`, reads its `+0x30` runtime slot base, chooses a free
voice from the eight-slot runtime bank record, sets volume/pan-related state,
and starts playback through the buffer vtable. The flag at description `+0x2c`
is copied to runtime state bit 1 after a successful start. `0x004f37c0` stops/releases the
voices for one sound slot; `0x004f28a0` performs the global sound-buffer and
audio-device cleanup.

## Proven recreation boundary

```text
ALL.PKR/audio/<name>.WAV
  -> FileIO_BackendOpen/Seek/Read
  -> 0x004ed9b0 RIFF/WAVE + PCM extraction
  -> 0x004f2b40 DirectSound buffer creation
  -> 0x004f2c70 buffer lock/copy/unlock
  -> 0x004f2e20 sound-id/voice consumer
```

The remaining opaque part is the initialization of the audio-device object
and the exact data-table schema for every VAB sound-description entry. Those
are not needed to recreate the proven resource and buffer path.

The extracted `.SFX` files should not be substituted for this proven PC input:
the corpus contains 25 small fixed-record `.SFX` files, but the executable has
no `.sfx` open/name cross-reference. The PC loader's source names come from
hardcoded sound-description tables and are converted directly to
`audio/<name>.wav`. `.SFX` therefore remains legacy or build-time metadata
until a producer or consumer is independently identified.

A controlled level-12 launch reached the live level-audio phase and printed
`VAB OPENED: skate2.vab, 1` followed by `VAB OPENED: ware.vab, 15`. Breakpoints
at the WAV parser, buffer creator, and fill routine then captured successful
Warehouse-bank buffer construction, for example:

```text
AUDIO_BUFFER_CREATE path=audio/rollconcrete2.wav
AUDIO_WAV_OPEN      path=audio/rollconcrete2.wav
AUDIO_BUFFER_FILL   rec=06655338 dsound=766c0140 flags=00000000 state=00000000
```

The same run reached the known missing-resource path for
`audio/drip3.wav`, where the open returned null and the sound loader emitted
its error. The heap and DirectSound pointers are run-specific; the ordered
VAB selection, WAV name, parser entry, and successful buffer-fill boundary are
the reusable evidence.

## Confidence and limits

- `confirmed`: `audio/<name>.wav` naming policy, common resource backend use,
  RIFF/WAVE parsing, PCM gate, format/sample outputs, aligned sample allocation,
  buffer descriptor values, lock/copy/unlock handoff, sound-slot capacity, and
  the sound-id playback boundary.
- `observed`: the extracted WAV corpus, the four front-end/level VAB family
  selections, live Warehouse VAB/name selection, and successful WAV-parser to
  DirectSound-buffer fill records.
- `inferred`: public DirectSound class names, the complete sound-description
  record fields, and the audio-device initialization call.
