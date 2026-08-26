# Runtime helper cluster: sound-channel cleanup

Status: observed

`0x004f2da0` is now split and byte-exact. It checks the sound system, releases
all eight interfaces in the selected bank, clears the bank's pointer table,
and preserves the logger/fatal tail when an interface release fails.

The bank index arithmetic, vtable release call, loop count, fixed diagnostics,
and non-returning failure transfer match under `vc6-coff-text`.
