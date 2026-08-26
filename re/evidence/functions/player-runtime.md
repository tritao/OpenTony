# Player/skater asset to gameplay-object runtime path

Status: confirmed player PSX-region handoff, skater-object allocation, camera
ownership, appearance/material selection, bitmap handoff, and
animation/physics/render consumers
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004521c0`, `0x004b5370`, `0x004b5620`, `0x004b2450`, `0x004547f0`,
`0x004691e0`, `0x0046c720`, `0x0040b650`, `0x00468e90`, `0x004b4ad0`,
`0x004b4e70`, `0x004b4e00`, `0x004b2030`, `0x004da260`, `0x00480cd0`,
`0x00469a30`, `0x0049e680`, `0x00467c90`

This closes the player-side runtime boundary around the already recovered PSH/
PSX loader. The gameplay pointer at `DAT_0056a858` is not a camera-only
object: it is the first entry of a two-entry skater/gameplay-object table. A
separate camera object is allocated and stored inside each skater object at
`+0x29b0`.

## Complete path

The normal player path is:

```text
front-end skater/costume selection
    -> 0x004521c0 Skater_LoadPlayerResources
    -> 0x004b5370 / 0x004b5620 player PSH/PSX spool
    -> 0x004b2450 common PSX parser
    -> DAT_0056d428 / DAT_0056d438 / DAT_0056d43c region tables
    -> 0x004691e0 SkaterObjects_Create
    -> 0x0047fd30(0x3538) allocation
    -> 0x0046c720 Skater_Construct
    -> 0x0040b650(0x674) camera construction at skater +0x29b0
    -> 0x00468e90 appearance/model-part setup
    -> 0x00469a30 animation consumer
    -> 0x0049e680 physics-frame consumer
    -> 0x00467c90 / 0x0045f530 renderer consumer
```

`Front_LoadGame` calls `0x004691e0` after the player resource setup and before
the remaining gameplay initialization. The same creation call is repeated by
the level-session restart path in `0x0046a8d0`, so this is a lifecycle boundary,
not a one-off front-end preview object.

## Skater object allocation and table ownership

`0x004691e0` clears `DAT_0056a858` and `DAT_0056a85c`, chooses one or two
players from the current mode, and allocates `0x3538` bytes for each entry.
The returned object is published at:

```text
DAT_0056a858 + player_index * 4
```

The constructor at `0x0046c720` installs the gameplay-object vtable and
initializes the large movement/animation state. Independent field writes in
the creation and consumer paths establish these correspondences:

| Offset | Runtime field | Evidence |
| --- | --- | --- |
| `+0x1f` | PSX region/object slot byte used by the shared object/model path | `0x004b4e70`, renderer and animation evidence |
| `+0x1a` | model index consumed with `+0x1f` by renderer/model-table lookup | shared renderer path at `0x0045f530` |
| `+0x29b0` | pointer to the owned camera object | `0x004691e0` stores the `0x674` allocation here |
| `+0x29bc` | peer skater pointer in two-player mode | `0x004691e0` links the two entries bidirectionally |
| `+0x2cc0` | selected skater-manager resource pointer | `0x004691e0`, `0x00468e90`, `0x004b4e70` |
| `+0x2cc4` | logical player index | `0x004691e0`, `0x004ddf30` |
| `+0x2ccc` | player-relative action-record bank pointer | `0x0046c720`, physics evidence |
| `+0x2cd0/+0x2cd4/+0x2cd8` | appearance resource handles 0x10/0x11/0x12 | `0x00468e90` writes the three `0x0048fbe0` results |
| `+0x30b8` | physics-state dispatch word | `0x00469a30`, `0x0049db80` |
| `+0x3144/+0x3148` | grounded turn accumulator and mirror | `0x00493370`, ground-movement evidence |

The allocation size and these offset writes are stable evidence for a
gameplay skater object. They do not imply that the complete original C++ class
has been recovered.

## Camera is a contained object, not the global player pointer

For each skater, `0x004691e0` allocates `0x674` bytes and calls
`0x0040b650(skater, 0, 0)`. The camera constructor:

- stores its parent skater pointer at camera `+0x3a4`;
- installs the camera vtable;
- initializes camera state from the parent skater and its model/position data.

The caller sets camera `+0x504` to `1`, then stores the camera at skater
`+0x29b0`. The frame update later advances camera `+0x5b4`, while the renderer
and physics paths continue to receive the `0x3538` skater pointer. In
two-player mode, the skater objects link through `+0x29bc`; this is separate
from the camera's parent link.

## Loaded-region handoff into the object

The player spool path is described in
[skater-asset-runtime.md](skater-asset-runtime.md). At the object boundary,
`0x004691e0` selects the manager resource with `0x004b0c60`, writes it to
`skater+0x2cc0`, and records the logical player number at `skater+0x2cc4`.
`0x00468e90` then performs the appearance/model-part setup for that skater.

The helper `0x004b4e70` uses the skater's region/object-slot byte and the
published model-pointer table (`DAT_0056d43c`) to validate and select model
data. It also resolves the selected part/material path used by the appearance
setup. The public costume naming policy is still open, but the runtime part
correspondence itself is name-based and demonstrably operates on the loaded PSX
region tables rather than on an unrelated front-end copy.

## Appearance, material, and player bitmap binding

The appearance helper at `0x00468e90` closes the player-side asset path after
the skater object exists. Its ordinary branch is selected when the manager
record is not marked custom. It loads `sk2anim.PSH` and the selected costume's
PSH, parses their `.spart` records through `0x004b47a0`/`0x004b4940`, and merges
the resulting part-name lists through `0x00480d90`. The merge compares names
byte-for-byte, then records the matching animation/model part indices through
`0x00480cd0`; it is a name-based correspondence, not an assumed positional
index. The helper releases both manifest buffers. The custom/special branch
instead calls `0x00423be0` with the
published custom-skater resource and the gameplay skater; both branches then
publish the three appearance handles at `+0x2cd0`, `+0x2cd4`, and `+0x2cd8`.

The next step is independently material-backed. `0x004b4e70` reads the
gameplay object's region slot at `+0x1f`, bounds-checks the model index from
`0x0048fbe0(0x10)` against the slot's model count, and asks `0x004b4ad0` for
the distinct material checksums referenced by that model's textured faces.
That helper walks the model's face/material records, compares their material
keys against the region's checksum table, and suppresses duplicates in a
caller-provided list. The selector resolves each checksum through
`0x004b2030`, then scores it through `0x004b4e00`/`0x004da260`: the score is
the number of distinct palette entries used by the decoded image. It returns
the candidate with the largest such count. Custom objects use their custom
resource's `+0x6c` material instead. This is the proven bridge from a
selected player model to a runtime `RuntimePsxMaterialRecord`, not merely a
model-index lookup.

`0x00468e90` then derives a player bitmap name from the selected model/material
and calls `0x004674d0(material, name)`. The name derivation is now exact:
`0x004547f0(set, deck)` returns the selected CRETEX name pointer, including
entries such as `s2dCr09u.bmp`; the appearance helper copies that string into
its local filename buffer and changes byte two from `d` to `g`. Thus the
player-side PC resource is the corresponding `s2gCr09u.bmp` name. The same
helper returns the static fallback names for deck values eight and above, and
uses the set/deck pointer table for ordinary entries.

`0x004674d0` opens the resulting image through the common file path, validates
4-bit or 8-bit depth according to the material, and hands the decoded resource
to `0x004d8c60` for PC texture creation and attachment. The player bitmap
therefore follows the same material-to-PC texture ownership contract as scene
textures, with a proven source-name to PC-name transformation.

```text
selected player PSX region/model
    -> 0x004b4e70 candidate material selection
    -> 0x004547f0 CRETEX source name (for example s2dCr09u.bmp)
    -> byte 2 changed d -> g (s2gCr09u.bmp)
    -> 0x004674d0 material + PC bitmap name
    -> file open/read and bit-depth validation
    -> 0x004d8c60 D3D texture decode/attach
    -> RuntimePsxMaterialRecord +0x14 RuntimePcTextureRecord
    -> player renderer
```

The costume-specific part names and the underlying image/palette record layout
remain data/implementation details; the material candidate set, duplicate
suppression, palette-entry score, and selected-material handoff are confirmed.

## Consumers

The object table has three independent downstream consumers:

1. `0x00469a30` walks `DAT_0056a858` through `DAT_0056a85c`, checks
   `skater+0x30b8`, `+0xf6`, and `+0x107`, and calls `0x00480730` to start
   idle/step-off/board animation indices.
2. `0x0049e680` receives the same skater pointer each frame, runs the action,
   collision, and state-specific physics path, and eventually reaches the
   shared position commit at `0x00496060`.
3. `0x00467c90` renders `DAT_0056a858` and, when present, `DAT_0056a85c` through
   `0x0045f530`. The renderer reads the common object/model fields and follows
   the PSX slot/model tables already documented for Warehouse geometry.

This gives a faithful recreation boundary:

```cpp
struct RuntimeGameplaySkater {
    // allocation size: 0x3538
    uint16_t model_index;           // +0x1a, paired with the region slot
    uint8_t  psx_region_slot;       // +0x1f, shared object/model path
    Camera*  camera;                // +0x29b0
    RuntimeGameplaySkater* peer;    // +0x29bc, two-player only
    void*    skater_resource;       // +0x2cc0
    int32_t  player_index;          // +0x2cc4
    ActionStateBank* actions;       // +0x2ccc
    void*    appearance_10;         // +0x2cd0
    void*    appearance_11;         // +0x2cd4
    void*    appearance_12;         // +0x2cd8
    uint32_t physics_state;         // +0x30b8
};
```

The field names above are operational names backed by the cited reads/writes;
the object still contains substantial animation, physics, and rendering state
that should be added only as each consumer proves it.

## Confidence and limits

- `confirmed`: two-entry skater table, `0x3538` allocation, constructor,
  camera allocation and ownership, manager/player-index fields, action-bank
  handoff, animation/physics/render consumers, and the common PSX region-table
  boundary; appearance handles; model-to-material candidate selection; player
  bitmap name lookup and d->g source/PC mapping; bitmap validation and handoff
  into the PC texture manager; model textured-face
  material extraction, checksum resolution, duplicate suppression, and
  distinct-palette-entry scoring.
- `observed`: appearance setup and model-part selection use the loaded region
  tables; the complete CRETEX set/deck table contents, underlying image/palette
  record layout, and custom object internals remain open.
- `inferred`: the public C++ class names and the complete object layout.
