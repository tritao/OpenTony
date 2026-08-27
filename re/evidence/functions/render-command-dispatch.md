# Render command-list dispatch slice

Status: tested native backend-neutral reconstruction of the command consumer;
Direct3D device calls remain an adapter boundary

Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`

Question: what does the linked polygon/command consumer select from each
record, and what ordering reaches the primitive handlers before presentation?

## Selected chain

```text
0x004d1d40  M3D_BuildD3DPolygon
    -> 0x004d20f0  M3D_ClipAndBucketPolygon
       -> per-view linked command/bucket list
          -> 0x004d3160  D3D_DrawPolygonList
             -> opcode-base handler
                -> Direct3D primitive submission
0x004d0c30  D3D_PresentFrame
    -> 0x004d0ca4  IDirectDrawSurface7::Flip
```

This slice starts at the list-consumer record. The upstream visibility/depth
and bucket-link contract is recorded separately in
[`render-packet-submission.md`](render-packet-submission.md). It stops before
the platform device call and keeps the actual display boundary separate from
command processing.

## Inputs

`0x004d3160` follows the `+0x00` next pointer of each linked record. The
fields that affect this reconstruction are:

```text
command +0x04  enabled/payload word
command +0x07  opcode and format byte
command +0x08  packed render flags
polygon +0x14  vertex count for variable polygon records
polygon +0x18  transformed vertex/color/UV stream
```

The native `RenderCommandRecord` represents the same fields without creating
process-local links. Its input span is already in the order produced by
following `next`. `polygon_index` is an optional stable identity used by
tests and trace adapters.

The consumer masks the opcode with `0xfc` before indexing the 256-entry table
initialized by `0x004d41e0`. The low opcode bits remain state inputs:

```text
bit 0  current geometry-format mode
bit 1  alpha/texture state selection
bit 2  texture/blend setup selection
bit 4  alternate per-record texture word selection
```

## Output records

The native `RenderDispatchRecord` emits exactly one record per input command:

```text
command_index       input/list position
polygon_index       carried packet identity
opcode/opcode_base  original byte and `opcode & 0xfc`
handler_address     installed retail handler, or `0x004d7420` no-op
primitive           portable primitive family
vertex_count        fixed handler count, or the packet count for `0xb0`
textured            handler/packet texture mode
active              `+0x04 != 0`
state               decoded low opcode state bits
```

The installed primitive map is:

| Base | Retail handler | Native result | Vertices |
| ---: | ---: | --- | ---: |
| `0x20` | `0x004d42f0` | solid triangle | 3 |
| `0x24` | `0x004d45f0` | textured triangle | 3 |
| `0x28` | `0x004d49e0` | solid quad | 4 |
| `0x2c` | `0x004d4bf0` | textured quad | 4 |
| `0x30` | `0x004d5040` | Gouraud triangle | 3 |
| `0x34` | `0x004d5280` | textured Gouraud triangle | 3 |
| `0x38` | `0x004d56c0` | Gouraud quad | 4 |
| `0x3c` | `0x004d5960` | textured Gouraud quad | 4 |
| `0x40` | `0x004d6560` | line | 2 |
| `0x48` | `0x004d6120` | line strip | 4 |
| `0x4c` | `0x004d6320` | closed line strip | 5 |
| `0x50` | `0x004d66f0` | colored line | 2 |
| `0x60` | `0x004d5e40` | solid rectangle | 4 |
| `0x68` | `0x004d6090` | unit rectangle | 4 |
| `0x70` | `0x004d60c0` | 8-unit rectangle | 4 |
| `0x78` | `0x004d60f0` | 16-unit rectangle | 4 |
| `0xb0` | `0x004d68b0` | general polygon | packet count |

An inactive record or an opcode base without an installed handler remains in
the output at its original position but resolves to the no-op address. This
preserves diagnostics without treating skipped work as a draw.

## Ordering and presentation boundary

The consumer does not reorder its input: `dispatch()` walks the span from
index zero to the end and returns the same number of records. Any bucket
ordering or list reversal is therefore represented by the caller that builds
the input span. The upstream target's per-bucket insertion is a head prepend;
the final bucket-head iteration order remains separate.

The dispatch result is a submission description, not a displayed frame. The
normal frame reaches the game-owned `0x004d0c30` wrapper and the
`IDirectDrawSurface7::Flip` callsite at `0x004d0ca4`; that boundary is already
modeled separately by the camera/frame contract.

## Native reconstruction and test

[`render_command_dispatch.hpp`](../../src/trg/render_command_dispatch.hpp) and
[`render_command_dispatch.cpp`](../../src/trg/render_command_dispatch.cpp)
implement the portable table and state decode. They intentionally do not
call Direct3D, allocate retail polygon records, or infer the bucket classifier;
the latter is covered by the packet-submission native seam.
The test in
[`render_command_dispatch_test.cpp`](../../src/trg/render_command_dispatch_test.cpp)
covers a textured triangle with low state bits, a textured Gouraud quad, the
variable-count `0xb0` path, a disabled command, and an uninstalled opcode
slot while asserting source/list order and preserved raw fields.

Confirmed from the existing renderer evidence: linked-record consumption,
opcode masking, no-op initialization, the handler addresses and primitive
families, fixed handler vertex counts, and the separation from the `Flip`
boundary. Native confidence is tested for this semantic adapter. Open are
the exact Direct3D enum/state names, variable `0xb0` stride/flag semantics,
final bucket-head iteration order, and the near-clipping/state-resolution
seams upstream.
