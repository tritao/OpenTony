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
  signals, kill/visible, object flags, resources, timer-reset tracing, fog, paths, reverb,
  level state, global words, career/goal conditions, and gap callbacks;
- raw node payload callbacks for object, pickup, and type-12/type-14 creation;
- a deterministic `LevelTriggerState` service that registers linked nodes,
  tracks object flags/visibility/active/alive state, records event order,
  stores timer-reset/resource/fog/path state, preserves the verified first
  `0x9e` level-event initialization, and exposes gap/career/goal state;
- a native PSX reader for fixed-point scene objects, model geometry,
  model-name hashes, texture metadata/palettes, tags, and blockmaps; and
- the retail general/Warehouse 44-byte gap table plus ordinary/deferred `0xc9`
  completion behavior;
- strict synthetic tests and native inspectors that load all 32 extracted
  retail TRGs and all 282 extracted PSXs without structural diagnostics;
  `opentony_trg_inspect --dispatch-all` now executes all 3,244 extracted
  type-6 command-point streams, including retail unknown-opcode fallthrough.

## Next C++ services, in dependency order

1. **Complete the level object registry.** The state layer now provides a
   stable `node index -> runtime object` map and binds type-2/type-12 keys to
   PSX model-name hashes and scene instances. `LevelSceneRegistry` now
   composes those static entities and creates explicit trigger entities for
   unresolved type-1/type-5/type-7/type-12/type-14 records. Still recover the
   object payload ownership and final name/id wiring, then replace the
   renderer-independent factory records with actual runtime object instances.
   Type-1/type-7 option-byte inputs are now retained, including the option-4
   flag clear, type-7 flag set, and option-2 environment-registration branch.
   The factory family is now explicit:
   `0xcb -> FUN_00403000`, `0x192 -> FUN_0049f250`, `0xd5..0xdc ->
   FUN_00412640`, and type-5 -> `FUN_004a8e50` pickup construction. Factory
   PSX resources are now lazily parsed and expose object/model counts.
   Type-5 pickup records also resolve the recovered subtype -> resource ->
   model-name table (`SKWARE_T.TRG` node 17, subtype 6 -> `ITEMS.PSX` model
   5). The native lifecycle now preserves the confirmed `0x100`-byte
   allocation, vtable `0x519684`, and pickup-list ownership at
   `DAT_0056b830`, together with visual byte `+0xd1`, raw motion bytes
   `+0xd2/+0xd3`, the verified per-tick 16-bit update of `+0x14/+0x16/+0x18`
   from `+0x70/+0x72/+0x74`, update-call count, and lazy glow transition at
   the `0x004a8ac0` tick boundary. The render snapshot carries the
   alternate resource/model namespace and geometry. Remaining pickup work is
   the frame/random producer, collection caller, and gameplay effects. The
   verified `FUN_004a8620` lifecycle is now represented when its raw
   constructor words are supplied: countdown sentinel/decrement, final-60
   phase, global fade gates, and zero-timer destruction are explicit; the
   constructor-side producer of `+0xf0` remains unresolved. Generic
   factory runtime metadata is now also
   retained: `0xcb` is `0x1f4`/vtable `0x5183b0` on the common list,
   `0x192` is `0x218`/vtable `0x5194f8` on its separate list, and
   `0xd5..0xdc` is `0x1e8`/vtable `0x5184e0` on the common list. The remaining
   object work is constructor payload ownership, baddy/vehicle update and
   destruction behavior, collision/AI integration, and final name/id wiring.
   The compact type-10/11 and type-12/14 records now retain their confirmed
   allocation/vtable/list boundaries (`0x28`/`0x5196a4` and
   `0x18`/`0x51982c` respectively); type-12/14 raw runtime link lists are now
   retained and traversed by the runtime when the explicit game-mode-8 policy
   is enabled, with the retail recursion guard. The remaining type-10/11
   update and type-12/14 live-asset policies remain separate services.
   Type-10/11 source-node Q12 bounds are
   also retained in `TriggerSpatialBounds`; the second pass now preserves raw
   alias links, applies the recovered mode-mask/high-bit filters, selects the
   last eligible target, and folds its Q12 position into those bounds. The
   `FUN_004aa4b0` alias-group table is also represented: eligible nodes retain
   the ordered 16-bit group field written to the selected target entry. The
   later geometry/update behavior still needs retail correlation.
   Type-12/14 `FUN_004bdd00` palette and compact color-wave arithmetic is now
   available through an explicit animation mode and is synchronized into scene
   entities; the live heap-object/player selection policy and palette-mask
   producer remain caller-owned seams.

2. **Connect event state to the game loop.** `LevelTriggerState` now records
   deterministic event order and retail timer-reset trace, and `LevelRuntime` supplies the
   first load/tick/pulse/restart facade. Connect it to the actual frame loop
   and real object callbacks while preserving pulse count/state, recursive link
   delivery, and retail ordering.

3. **Gameplay objective state.** The state layer now parses the retail 44-byte
   gap records and implements ordinary versus deferred `0xc9` completion,
   score/name capture, deferred completion/award transitions, and the
   one-shot source pulse. Still connect table
   selection for career/editor modes, player-position gap detection, score
   presentation, and persistent checklist state.

4. **Level services.** Connect resource loading, music/sound, fog, reverb,
   path records, and level-event state to the native asset/resource managers.
   `LevelRuntime` now exposes catalog-backed bindings for TRG resource
   requests, parses each resolved PSX request through the catalog cache, and
   retains object/model counts. The native PSX catalog resolves factory
   resource names such as `c_taxi`, and the native PRE catalog loads
   `LEVEL.PRE` and player/UI packages. Still connect resource lifetime,
   streaming/flush semantics, and renderer/audio ownership; Warehouse
   autoexec and `Ho_SkWare_HPGap` are the first acceptance cases.

5. **Asset-backed scene/object creation.** Feed the native PSX object/model
   data into the object registry and renderer. The type-12/type-14 path now
   joins Warehouse node 120 through its PSX model-name key, preserves the
   verified asset flag/marker writes plus the raw owner/control context when
   supplied by the player service, and reproduces the recovered color-wave
   update. The actual live asset pointer, player selection policy, palette-mask
   producer, and final object behavior remain. Keep trigger node IDs
   as the stable join key between TRG, PSX, and runtime objects.

6. **Player/restart integration.** Apply the recovered fixed-point restart
   positions, restart-derived facing matrix, and auxiliary fields to the player runtime, including the
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
- per-frame traces for timer-reset ordering, pulse ordering, and restart execution.

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
