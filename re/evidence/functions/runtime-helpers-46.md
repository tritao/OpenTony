# Runtime helper cluster: sound slot teardown and CPUID feature check

Status: observed

Two small sound-path helpers are now exact VC6 reconstructions:

- `0x004f23d0` releases the active sound slot when present, invokes the
  common sound reset, and clears the active flag.
- `0x004f2700` executes CPUID leaf 1 and returns the original feature bit 23.

The conditional release, fixed calls, CPUID sequence, and stack cleanup match
under `vc6-coff-text`.
