set pagination off
set confirm off
set print pretty on
set disassembly-flavor intel
python
import sys
from pathlib import Path
root = Path.cwd()
sys.path.insert(0, str(root / "build/gdb"))
sys.path.insert(0, str(root / "re/gdb"))
end
source re/gdb/bootstrap.py
