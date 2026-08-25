"""Load the OpenTony GDB package.

This file is executed by GDB's embedded Python, not by the OpenTony virtualenv.
The package under ``re/gdb/opentony`` therefore uses only the Python standard
library and GDB's own ``gdb`` module. Run ``tony gdb generate`` first so the
generated symbol module is available on GDB's import path.
"""

from opentony.commands import register_commands

register_commands()
