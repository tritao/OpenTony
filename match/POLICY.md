# Matching module policy

The byte-preserving split is a permanent verification oracle, not the final
architecture of OpenTony.

## Ownership

- A function module owns instruction bytes from its entry through its final
  instruction. It does not own trailing alignment.
- Padding, jump tables, defined data, and unknown bytes are separate explicit
  fragments.
- Every raw section byte has exactly one owner; virtual zero-fill has no file
  fragment.

## Status

Classify a module by its implementation body, not its filename, compiler, or
build strategy. These definitions are canonical for documentation, manifests,
tooling, and progress reports:

- `raw`: the complete module is preserved with `incbin`.
- `hybrid`: a documented prefix or region is readable assembly and the
  remainder is preserved. `reconstructed_size` records only understood source
  bytes, not the full module size.
- `asm`: every byte is expressed as reviewed assembly/data directives.
- `vc6_asm`: every byte is expressed as a VC6 `__declspec(naked)` inline
  assembly block. This is matching assembly, even though its container is a
  `.cpp` file.
- `cpp`: the module has a genuine higher-level C or C++ implementation, contains
  no naked inline assembly, and has an explicit matching strategy.

Promotion requires exact module bytes and a byte-identical full PE rebuild.
Promoting `vc6_asm` to `cpp` additionally requires replacing the naked assembly
body with typed higher-level code while retaining an exact-byte comparison
strategy.

## Exact encodings

Use ordinary NASM when it emits retail bytes. Use explicit sizes, `strict`, and
`short` where they select the retail form. When equivalent x86 encodings are
canonicalized differently, preserve the retail form with a small commented
`db` sequence. Encoding directives are evidence, not a failure of
reconstruction.

## Subsystem work

Leaf helpers are reconstructed when they support a subsystem, not merely
because they are adjacent. A subsystem slice should record:

- calling convention and argument ownership;
- object fields and global state read or written;
- calls and branch decisions;
- unresolved semantics;
- behavioral or differential tests needed before higher-level replacement.

Matching assembly remains the oracle beneath later C/C++ reconstruction.
Progress reports distinguish all byte-matching source (`hybrid`, `asm`,
`vc6_asm`, and `cpp`) from semantic C/C++ progress (`cpp` only).
