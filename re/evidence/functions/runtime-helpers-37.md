# Runtime helper cluster: media descriptor seeking

Status: observed

`0x004e79f0` is now split and byte-exact. It validates the descriptor,
updates buffered offsets for set/current/end origins, dispatches XA seeks with
the original mode and offset arguments, and reports invalid descriptors.

The indexed state accesses, mode branches, seek callback, position callback,
and diagnostic path match under `vc6-coff-text`.
