"""Small deterministic action-mask injection helper for runtime experiments."""

from __future__ import annotations

import gdb

from .breakpoint import CountingBreakpoint


# 0x004e4650 publishes the current low-word action mask to its callers.  The
# game has already completed DirectInput polling by this point, so replacing
# the word here drives the real menu/gameplay consumers without synthesizing
# an X or Windows keyboard event.
ACTION_MASK_PUBLISH = 0x004E4650
ACTION_MASK_GLOBAL = 0x006A3F1C


class ActionMaskSequenceProbe(CountingBreakpoint):
    """Write one supplied low-word action mask at each publish boundary."""

    def __init__(self, masks: list[int], writer=None):
        if not masks:
            raise ValueError("at least one action mask is required")
        self.masks = [mask & 0xffff for mask in masks]
        self.writer = writer
        super().__init__(ACTION_MASK_PUBLISH, count=len(self.masks), internal=True)

    def on_count(self, ctx):
        mask = self.masks[self.hits]
        ctx.memory.write_u16(ACTION_MASK_GLOBAL, mask)
        if self.writer is not None:
            self.writer.event({
                "type": "action_mask_injected",
                "frame": ctx.frame,
                "mask": mask,
                "sequence_index": self.hits,
            })
        return True

    def on_complete(self):
        gdb.write(
            f"action mask sequence complete: {len(self.masks)} observations\n"
        )
