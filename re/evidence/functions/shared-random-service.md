# `0x0048f3a0` shared-service boundary

Status: static body confirmed; PRNG identity not established.

The function currently called `FUN_0048f3a0` in the replay probes is a
`__thiscall` method. The first argument is the object in `ECX`; the stack
argument is a selector and the function returns one 32-bit value with
`ret 0x4`.

The retail body at `0x0048f3a0` does not show a state transition for a random
number generator. It instead:

- validates the selector through the common validation/logging helper at
  `0x004011e0`;
- resolves a table entry from the object's `+0x2cc0` data through
  `0x00416050`;
- selects among object fields and static tables near `0x0053a4fc` and
  `0x00536884`; and
- returns the selected/clamped value at `0x0048f588`.

It writes the diagnostic/global scratch location `0x00568638`, but no global
or object field has been established as PRNG state, and the body has no
stateful PRNG step comparable to an LCG/table advance. The direct probe
therefore records `state_before` and `state_after` as explicit `null` values
with `state_status: "not-established"`. This is intentional: the probe is an
observation boundary and does not inject or replace the result.

The static retail image has 74 direct `call` xrefs to this address, far more
than the existing physics-specific site lists. The current global trace is
therefore valuable: it records the caller, selector, object, per-frame
ordinal, and returned value for every invocation, including consumers not
covered by the older call-site-specific probes. Its event name retains the
historical `shared_random_call` label so existing recordings can be compared
while the function's true role is resolved.

Conclusion: do not implement `RetailRng` from this function yet. First locate
the actual retail routine that advances persistent random state, if one exists,
and then extend the probe with a real state address and before/after samples.
