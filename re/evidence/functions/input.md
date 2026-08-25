# PC input initialization and binding load

Status: observed
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004e4690`, `0x004e4d10`, `0x004e4a90`, `0x004e42c0`, `0x004699f0`, `0x00469de0`

## Observation

- `0x004e4690` contains the diagnostic `InitDirectInput`, creates/acquires DirectInput keyboard, mouse, and joystick devices, and calls `0x004e4d10` during initialization.
- `0x004e4d10` contains the exact diagnostic `PCINPUT_Load`.
- Its decompilation reads ten configured control values, including the entries corresponding to grind, jump, spin, nollie, switch, pause, and camera. It resolves `JOYPAD` and `KEYBOARD` bindings, validates duplicates and key ranges, then calls the input-device setup helpers.
- The current `TH2_OPT.CFG` runtime configuration matches this path: movement uses arrow/WASD bindings and trick actions use the configured keyboard keys.
- `0x004e4a90` is the per-loop DirectInput poll called from `0x00489a10`. It updates the keyboard device, polls the optional joystick, refreshes mouse state, and handles Alt-F4.
- `0x004e42c0` converts the polled bindings into the per-player action mask at `DAT_006a3f1c`; `0x004e4650` exposes that mask to the higher-level action update.
- `0x004699f0` calls the input-history/update chain (`0x00489cd0`, `0x00489a10`, `0x00489e70`) once per level-loop iteration. `0x00469de0` follows it and is a stable post-poll sampling point.
- The repeatable GDB collector `tony-input-sample COUNT FILE [--force]` records the action mask, raw keyboard mask, and 256-byte keyboard state after the poll.
- A controlled Warehouse run held Left while sampling 80 post-poll iterations. The companion player trace showed stable player/owner pointers and movement in the position-like 16.16 fields at offsets `0x08/0x0c/0x10`, `0x90/0x94/0x98`, and `0xbc/0xc0/0xc4`.
- The dynamic input trace now distinguishes idle from held input: 30 idle Warehouse samples had action mask `0x0000`, while 60 samples with Left held had action mask `0x8000`, all at level `12`. This is the first confirmed binding-to-action-bit mapping for the keyboard movement path.

## Interpretation

`PCInput_LoadBindings` is a verified naming anchor for configuration parsing. The per-loop poll and action-mask construction are now identified. The next step is to map the configured movement binding to its action bit and correlate held versus edge-triggered bits with the skater state update.

## Open questions / falsifiers

- Confirm the corresponding bits for Right, Up, and Down, then distinguish held, pressed, and released transitions.
- Confirm input transitions dynamically with one held key and one edge-triggered key.
