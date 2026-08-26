# Runtime helper cluster: frontend predicate

Status: observed

`0x004e73e0` is split and reconstructed exactly under VC6 SP3. It returns
whether the frontend-state byte at `0x6a6ce0` is zero; VC6 emits the original
`xor eax,eax` / `sete al` sequence followed by its alignment suffix.

The movie/input table accessor at `0x004e7070` is also exact: it indexes the
fixed record pointer at `0x52d4a8` with a nine-word stride.

The adjacent `0x004e7c50` leaf is an exact empty `void` stub.
