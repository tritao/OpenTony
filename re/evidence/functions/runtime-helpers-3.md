# Runtime helper cluster: leaf stubs

Status: observed

The instruction-safe split around `0x004e86f0` exposes a repeated group of
compiler-generated leaf stubs. Exact VC6 SP3 reconstructions are tracked for:

- `0x004e86f0`, `0x004e8700`, `0x004e8710`, and `0x004e8720`, which return
  their single integer argument unchanged.
- `0x004e8730`, `0x004e8740`, `0x004e8750`, `0x004e8760`, `0x004e8770`,
  `0x004e8790`, `0x004e87a0`, and `0x004e87c0`, which return zero.
- `0x004e87e0`, an empty void routine that consists only of `ret`.

The manually verified `0x004e8780` gap is also tracked as an identity
function; its five-byte body is instruction-aligned even though Ghidra did
not emit it as a safe function proposal.

VC6 emits the same retail instructions followed by section-alignment NOPs;
the comparator accepts only the function bytes and verifies the suffix as
alignment padding. The additional tiny stubs at `0x004e8840`–`0x004e8b20`
follow the same exact-return patterns and are tracked in the manifest.

The nearby `0x004e8940` initializer is also reconstructed exactly: it clears
four object words and writes the byte marker `0xe0` at offset seven.
