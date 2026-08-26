# Faithful C++ recreation boundary

The trigger runtime is now a real, portable C++ vertical slice. It is not yet
the game. The important boundary is that the bytecode reader and dispatcher
are no longer the blocker; the next work is to supply the services called by
the dispatcher and to connect them to the other level systems.

## Already implemented

- bounded little-endian `_TRG` loading with relative node offsets;
- type-6/type-2/type-9 command-point records and checksum buckets;
- type-8 restart views, autoexec selection, named restart selection, and
  position-application callbacks;
- exact variable-width command cursor, including string alignment, fixed path
  tables, and aligned `0xc9` gap operands;
- verified command services for pulses, pulse budgets, suspend/activate,
  signals, kill/visible, object flags, resources, timers, fog, paths, reverb,
  level state, global words, career/goal conditions, and gap callbacks;
- raw node payload callbacks for object, pickup, and type-12/type-14 creation;
- a deterministic `LevelTriggerState` service that registers linked nodes,
  tracks object flags/visibility/active/alive state, records event order,
  stores timers/resources/fog/path/restart state, and exposes gap/career/goal
  state;
- a native PSX reader for fixed-point scene objects, model geometry,
  model-name hashes, texture metadata/palettes, tags, and blockmaps; and
- the retail general/Warehouse 44-byte gap table plus ordinary/deferred `0xc9`
  completion behavior;
- strict synthetic tests and native inspectors that load all 32 extracted
  retail TRGs and all 282 extracted PSXs without structural diagnostics.

## Next C++ services, in dependency order

1. **Complete the level object registry.** The state layer now provides a
   stable `node index -> runtime object` map and binds type-2/type-12 keys to
   PSX model-name hashes and scene instances. `LevelSceneRegistry` now
   composes those static entities and creates explicit trigger entities for
   unresolved type-1/type-5/type-7/type-12/type-14 records. Still recover the
   object payload ownership and final name/id wiring, then replace the
   renderer-independent factory records with actual runtime object instances.
   The factory family is now explicit:
   `0xcb -> FUN_00403000`, `0x192 -> FUN_0049f250`, `0xd5..0xdc ->
   FUN_00412640`, and type-5 -> `FUN_004a8e50` pickup construction. Factory
   PSX resources are now lazily parsed and expose object/model counts; the
   remaining work is constructor payload wiring and live object behavior.

2. **Connect event state to the game loop.** `LevelTriggerState` now records
   deterministic event order and timer expiry, and `LevelRuntime` supplies the
   first load/tick/pulse/restart facade. Connect it to the actual frame loop
   and real object callbacks while preserving pulse count/state, recursive link
   delivery, and retail ordering.

3. **Gameplay objective state.** The state layer now parses the retail 44-byte
   gap records and implements ordinary versus deferred `0xc9` completion,
   score/name capture, and the one-shot source pulse. Still connect table
   selection for career/editor modes, player-position gap detection, score
   presentation, and persistent checklist state.

4. **Level services.** Connect resource loading, music/sound, fog, reverb,
   path records, and level-event state to the native asset/resource managers.
   `LevelRuntime` now exposes catalog-backed bindings for TRG resource
   requests, the native PSX catalog resolves factory resource names such as
   `c_taxi`, and the native PRE catalog loads `LEVEL.PRE` and player/UI
   packages. Still connect resource lifetime, streaming/flush semantics, and
   renderer/audio ownership; Warehouse autoexec and `Ho_SkWare_HPGap` are the
   first acceptance cases.

5. **Asset-backed scene/object creation.** Feed the native PSX object/model
   data into the object registry and renderer. Then implement pickups and the
   type-12/type-14 special-node path. Keep trigger node IDs as the stable join
   key between TRG, PSX, and runtime objects.

6. **Player/restart integration.** Apply the recovered fixed-point restart
   positions and facing/auxiliary fields to the player runtime, including the
   one-player/two-player restart names and the post-restart script stream.

## Evidence needed before calling it faithful

- live Warehouse node 141 pulse with a watch on each linked object's `+0x04`
  field, to resolve the exact visible/active mask semantics;
- live execution of `Ho_SkWare_HPGap`, including player position writes and
  the side effects of `0x68`, `0x9d`, `0xa6`, `0xa9`, and `0x80`;
- one completed gap through `0xc9`, correlating checksum, argument, score,
  checklist state, and the resulting pulse;
- one object/pickup creation trace correlating TRG payload fields with PSX
  object/model records;
- per-frame traces for timer expiry, pulse ordering, and restart execution.

## Full-game dependencies outside this trigger slice

A faithful playable recreation also needs the input/state loop, animation and
trick systems, collision/blockmaps, skater physics, camera, renderer, audio,
menus, save/career state, and the remaining PC resource formats. Those should
consume the trigger services above; they should not be folded into the TRG
decoder.

The C++ acceptance gate for this slice is:

```sh
cmake -S . -B build/native
cmake --build build/native
ctest --test-dir build/native --output-on-failure
build/native/opentony_trg_inspect /path/to/SKWARE_T.TRG
```
