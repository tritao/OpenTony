# PC input initialization and binding load

Status: observed
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004e4690`, `0x004e4d10`

## Observation

- `0x004e4690` contains the diagnostic `InitDirectInput`, creates/acquires DirectInput keyboard, mouse, and joystick devices, and calls `0x004e4d10` during initialization.
- `0x004e4d10` contains the exact diagnostic `PCINPUT_Load`.
- Its decompilation reads ten configured control values, including the entries corresponding to grind, jump, spin, nollie, switch, pause, and camera. It resolves `JOYPAD` and `KEYBOARD` bindings, validates duplicates and key ranges, then calls the input-device setup helpers.
- The current `TH2_OPT.CFG` runtime configuration matches this path: movement uses arrow/WASD bindings and trick actions use the configured keyboard keys.

## Interpretation

`PCInput_LoadBindings` is a verified naming anchor for configuration parsing. It is not the per-frame input polling function yet. The next static step is to follow the input functions called from `Game_MainLoop` and identify which fields represent pressed, held, and released states.

## Open questions / falsifiers

- Identify the per-frame DirectInput polling function and its output buffers.
- Match the configured action indices to the game input structure used by the skater update.
- Confirm input transitions dynamically with one held key and one edge-triggered key.
