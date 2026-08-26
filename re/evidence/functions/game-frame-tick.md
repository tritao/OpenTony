# Game frame message pump

Status: observed

`0x004f7ce0` (`Game_FrameTick`) now has an exact VC6 reconstruction. It polls
the game window queue, consumes messages until the queue drains, filters the
keyboard character messages, dispatches the remaining messages, and returns
the original quit/message result.

The imported API indirections, message offsets, filter values, loop branch, and
stack cleanup match under `vc6-coff-text`.
