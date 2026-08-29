"""GDB command package for OpenTony's recording and reversing workflow."""

from .registry import register_commands

__all__ = ["register_commands"]
