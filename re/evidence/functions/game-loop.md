# Startup, frontend, and gameplay loops

Status: inferred
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004f7e30`, `0x00452ff0`, `0x004544a0`, `0x0046a3a0`, `0x0041c2d0`

## Observation

Ghidra's decompiler identifies the following static path in the recorded PE32/i386 executable:

- `0x004f7e30` is the WinMain-shaped entry path. After the CD check, window and DirectInput setup, it calls `0x0046a770`.
- `0x0046a770` initializes the shell and calls `0x00452ff0`.
- `0x00452ff0` contains the frontend state machine and embeds the screen names `MAIN_MENU`, `LEVEL_SELECT`, `PLAY_GAME`, `OPTIONS`, and others. Its switch path calls `0x004544a0` to launch a selected level.
- `0x004544a0` logs `Launch The Game Level: %d, Mode: %d`, prepares the game mode, and calls `0x0046a8d0`.
- `0x0046a8d0` calls `0x004524a0` for level/player loading and then invokes `0x0046a3a0` while the level session remains active.
- `0x0046a3a0` contains a loop guarded by `DAT_0056a8d0 == 0`. Each iteration performs timing/input, object, camera, and render-related calls before testing the session state.
- `0x0041c2d0` is a second, outer game loop. It initializes input once, repeatedly performs timing and subsystem updates, invokes a virtual callback at `(*piVar3 + 4)`, presents a frame, and exits on a session callback result.

The movie helper at `0x004e7090` formats the exact `PCMOVIE_PlayGameFMV` diagnostic before starting and finishing a Bink movie. It is part of startup/frontend flow, not the gameplay frame loop.

## Interpretation

The conservative current names are `FrontEnd_Main`, `Front_LaunchGameLevel`, `Game_LevelLoop`, and `Game_MainLoop`. The level loop is the best first runtime breakpoint for a Warehouse experiment; the outer loop is the better candidate for global frame timing.

## Open questions / falsifiers

- Confirm at runtime that `0x0046a3a0` is reached after the Warehouse level is selected and that repeated hits correspond to frames.
- Determine whether `0x0041c2d0` is the main gameplay loop or a broader shell/session loop invoked for a different mode.
- Resolve the meanings of `DAT_0056a8d0`, `DAT_0056a898`, and the callback object at `DAT_0056a858` with GDB observations.
