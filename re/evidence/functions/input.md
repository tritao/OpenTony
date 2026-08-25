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
- The repeatable GDB collector `tony-input-sample COUNT FILE [--force]` records the action mask, held DirectInput scan codes, and the 256-byte keyboard state after the poll. The keyboard wrapper is at `0x006a43e0`; its state buffer is at `0x006a43e4` (`+4`).
- A controlled Warehouse run held Left while sampling 80 post-poll iterations. The companion player trace showed stable player/owner pointers and movement in the position-like 16.16 fields at offsets `0x08/0x0c/0x10`, `0x90/0x94/0x98`, and `0xbc/0xc0/0xc4`.
- The dynamic input trace now distinguishes idle from held input: 30 idle Warehouse samples had action mask `0x0000`, while held-key samples at level `12` produced Left `0x8000` (60/60), Right `0x2000` (20/20), Up `0x1000` (20/20), and Down `0x4000` (20/20). This confirms the complete keyboard movement-to-action-bit mapping.
- The corrected sampler captured DIK_LEFT (`0xcb`) as held byte `0x80` in the DirectInput buffer while the action mask was `0x8000`; idle capture produced no held scan codes.
- The movement action-state records are 16 bytes each at `0x0056b078` (Left), `0x0056b088` (Right), `0x0056b098` (Up), and `0x0056b0a8` (Down). A held-Left trace at the state-update chain observed `byte0` changing `0 -> 1` at `0x00489966` and returning `1 -> 0` at `0x0048996a`; `byte1` changed `0 -> 1` once at `0x00489951`, identifying it as a press-edge latch candidate.
- In the same trace, the first counter word at record `+0x04` reset on activation and incremented once per update while held, while the second counter at `+0x08` reset on activation and incremented during the inactive interval. The `+0x0c` word advanced with the state-update cadence. These counter roles are recorded as behavior, not yet assigned final type names.

## Interpretation

`PCInput_LoadBindings` is a verified naming anchor for configuration parsing. The per-loop poll, action-mask construction, and complete movement mapping are now identified. The next step is to correlate held versus edge-triggered bits with the skater state update and follow the movement consumer.

## Open questions / falsifiers

- Follow the movement consumer that reads these confirmed action-state transitions into acceleration and rotation state.
- Confirm input transitions dynamically with one held key and one edge-triggered key.
