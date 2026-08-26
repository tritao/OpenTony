# Runtime helper: parser input pump

Status: observed

`0x004fa990` now has an exact VC6 reconstruction. It advances the parser
window, flushes pending input through the stream helper, handles short-input
and end-mode paths, and returns the original mode-dependent status values.

The loop branches, cursor arithmetic, helper call targets, cleanup paths, and
status epilogues match under `vc6-coff-text`.
