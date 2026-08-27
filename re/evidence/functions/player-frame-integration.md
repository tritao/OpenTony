# Player frame integration

Status: integrated native fixture; retail trace anchors recorded

The selected retail build is
`f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669`.
The compact [player-frame-integration fixture](../fixtures/player-frame-integration.json)
keeps the raw trace paths and the deterministic native values together.

## Retail anchors

The ordinary grounded anchor is frame 10324 in the `physics-contact2` session.
The dispatcher at `0x0049db80` selects case 0 and the handler chain
`0x0049dad0 -> 0x00496550 -> 0x00495cc0 -> 0x0049d9c0`. The position commit
call at `0x0049f0e5` receives proposed words
`0x01893682, 0xfffd22d5, 0x02947ff8`; the live position is
`[25761808, 4294845077, 43497789]` and the `+0x4c` response is
`[4662, 4294850065, 4294791555]`.

The air-to-landing anchor is frame 10347. Case 1 reaches `0x00497f40`, whose
collision observation records raw result token `99837432` and separate
material words `{0, 128, 1, 1, 0}`. The accepted-contact position call at
`0x0049917b` receives `0x018af966, 0xffffa587, 0x025b94b3` with pre-call
position `[25853558, 144166, 39500633]` and response
`[3799, 151267, 4294795129]`. The contact then requests state `1 -> 0` with
reason `0x1fd6`; the request reaches the `0x004902bf` writer. A separate
animation trace records the same landing caller chain through outer caller
`0x0049a519` and the range wrapper at `0x00490447`, requesting animation 5,
start 0, end -1, alternate -1.

The raw in-air collision token is not a C++ boolean. The caller compares the
collision-owned local/result token while material/contact flags are separate.
Likewise, the `0x00466090` position-query wrapper clears EAX after forwarding
to the query engine. Native acceptance therefore uses the optional hit record
and preserves the first hit for the collision consumer and landing predicate.

## Native execution boundary

`GameplayFrame` now provides an executable two-step fixture over the real
`LevelRuntime` scheduler using self-contained synthetic TRG/PSX files. The
fixture does not depend on installed retail assets or silently skip. The
callback order is input history, level tick, action
history publication, action stream consumer, dispatcher stage entry, collision
query/commit, collision consumer, accepted air contact, and landing animation
request. The fixture uses Q12 positions and Q8 frame scale `0x100`:

- Grounded state 0 with mask `0x2000` publishes action 4 at timestamp 1,
  moves `[0,8192,0]` by response `[384,0,128]`, queries exactly the desired
  `[384,8192,128]`, selects candidate 1, and emits grounded animation 7
  range `[1,1]`.
- In-air state 1 with neutral input publishes action 4 released at timestamp
  2. Desired `[0,-8192,0]` is rejected three times; old Y candidate 4
  `[0,4096,0]` is accepted. The four query calls and their fixed-point
  endpoints are retained in the fixture; the collision consumer sees hit
  normal `{0,4096,0}`, the standard ground predicate accepts it, state changes
  to 0 with reason `0x1fd6`, and the evidence-backed animation-5 request is
  emitted.

The fixture does not add a writer for skater `+0x3200`. Static matching proves
that a nonzero value selects direct proposed-XYZ stores and the selected
ordinary/landing paths use the collision branch. No writer appears in the
current split modules or selected traces, so the native boundary keeps the
caller-supplied bypass flag explicit. General collision geometry and pose
decoding remain outside this slice.
