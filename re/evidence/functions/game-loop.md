# Startup, frontend, and gameplay loops

Status: Warehouse gameplay loop observed
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004f7e30`, `0x00452ff0`, `0x004544a0`, `0x0046a3a0`, `0x004f7ce0`, `0x0041c2d0`

## Observation

Ghidra's decompiler identifies the following static path in the recorded PE32/i386 executable:

- `0x004f7e30` is the WinMain-shaped entry path. After the CD check, window and DirectInput setup, it calls `0x0046a770`.
- `0x0046a770` initializes the shell and calls `0x00452ff0`.
- `0x00452ff0` contains the frontend state machine and embeds the screen names `MAIN_MENU`, `LEVEL_SELECT`, `PLAY_GAME`, `OPTIONS`, and others. Its switch path calls `0x004544a0` to launch a selected level.
- `0x004544a0` logs `Launch The Game Level: %d, Mode: %d`, prepares the game mode, and calls `0x0046a8d0`.
- `0x0046a8d0` calls `0x004524a0` for level/player loading and then invokes `0x0046a3a0` while the level session remains active.
- `0x0046a3a0` contains a loop guarded by `DAT_0056a8d0 == 0`. Each iteration pumps Windows messages at `0x004f7ce0`, updates timing, polls input through `0x004699f0`, then performs object, camera, and render-related calls before testing the session state.
- `0x0041c2d0` is a second, outer game loop. It initializes input once, repeatedly performs timing and subsystem updates, invokes a virtual callback at `(*piVar3 + 4)`, presents a frame, and exits on a session callback result.

The movie helper at `0x004e7090` formats the exact `PCMOVIE_PlayGameFMV` diagnostic before starting and finishing a Bink movie. It is part of startup/frontend flow, not the gameplay frame loop.

Under the recorded `640x480x16` headless profile, bypassing the blocking movie routine at `0x004e5ec0` led to a runtime breakpoint hit at `0x00452ff0`, returning through `0x00503054`. This dynamically confirms the frontend anchor; the subsequent Hangar run confirmed the level-loop candidate as well.

The same profile was then driven through Free Skate → Tony Hawk → Hangar. Runtime breakpoints confirmed the authentic launch chain: `0x004544a0` was called with level index `0` and mode `2`, `0x004524a0` performed the Hangar resource load, and `0x0046a3a0` was reached after selecting PLAY on the Free Skate loading screen. At the first level-loop hit, the registers included `EDX=0x05f34100`, `EBP=1`, and `EDI=7`; `EDX` is a candidate session/player root for follow-up memory tracing, not yet a confirmed type.

The Warehouse override experiment dynamically confirmed the same loop after loading level index `12`. At entry, `DAT_0056a858` held the first gameplay skater pointer `0x05f39530`, while `EDX` held `0x05f39500`, exactly 0x30 bytes before that object. `DAT_0056a898` remained `12`. Static creation shows that `DAT_0056a858` is entry zero of a two-entry player-object table and that entry one is at `DAT_0056a85c`; these addresses are allocation-dependent observations, while the table and relative-offset relationships are stable.

The repeatable raw collector is `tony-player-sample COUNT FILE [--force]`. It arms a temporary breakpoint at the repeated message-pump call site `0x004f7ce0` inside `Game_LevelLoop`, records registers plus the owner/player byte ranges as NDJSON, and automatically continues until `COUNT` samples are collected. The `0x0046a3a0` breakpoint remains the one-time level-session entry.

A five-sample Warehouse baseline produced five consecutive `0x004f7ce0` hits with level `12`, stable player pointer `0x05f39530`, and stable owner/header address `0x05f39500`. The initial samples were byte-identical in the captured player range after startup settled, establishing a usable stationary baseline. A second controlled Warehouse run held Left during live gameplay; 80 samples retained the same player/owner pointers and level 12 while the position-like fields changed over the capture, confirming that the sampler observes live player state rather than only loader data.

## Interpretation

The conservative current names are `FrontEnd_Main`, `Front_LaunchGameLevel`, `Game_LevelLoop`, and `Game_MainLoop`. The level loop is the best first runtime breakpoint for a Warehouse experiment; the outer loop is the better candidate for global frame timing.

## Open questions / falsifiers

- Confirm which repeated inner-loop hits correspond to rendered frames; `0x004f7ce0` itself is the Windows message pump, not the physics function.
- Trace both entries of `DAT_0056a858`/`DAT_0056a85c` and their `-0x30` owner/header relationship across loop iterations and compare them with the skater-state layout.
- Determine whether `0x0041c2d0` is the main gameplay loop or a broader shell/session loop invoked for a different mode.
- Resolve the meanings of `DAT_0056a8d0`, `DAT_0056a898`, and the callback object at `DAT_0056a858` with GDB observations.

## Native reconstruction boundary

`src/runtime/level_frame.*` now represents the confirmed inner ordering at a
portable boundary: an externally supplied elapsed interval and action mask are
recorded by `InputState`, then `LevelRuntime::tick()` advances trigger timers
and scene state, then a downstream observer receives the frame for player,
camera, renderer, and audio work. This does not claim to replace the Windows
message pump, DirectInput device polling, timing source, or virtual gameplay
callback; those remain the next application-layer services.
