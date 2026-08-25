from __future__ import annotations

import shlex
import shutil


def xvfb_command(cfg: dict, env: dict[str, str]) -> list[str]:
    """Build the configured isolated Xvfb wrapper and apply its renderer environment."""

    desktop = cfg.get("virtual_desktop", {})
    width = int(desktop.get("width", 1024))
    height = int(desktop.get("height", 768))
    xvfb = cfg.get("xvfb", {})
    depth = int(xvfb.get("depth", 16))
    if depth not in {8, 16, 24, 32}:
        raise SystemExit(f"invalid Xvfb screen depth: {depth}; expected one of 8, 16, 24, 32")

    server_args = xvfb.get("server_args", ["+extension", "GLX"])
    if not isinstance(server_args, list) or not all(isinstance(arg, str) for arg in server_args):
        raise SystemExit("wine.yml xvfb.server_args must be a list of strings")
    renderer_env = xvfb.get("environment", {})
    if not isinstance(renderer_env, dict):
        raise SystemExit("wine.yml xvfb.environment must be a mapping")
    env.update({str(name): str(value) for name, value in renderer_env.items()})

    screen_args = ["-screen", "0", f"{width}x{height}x{depth}", *server_args]
    xvfb_run = shutil.which("xvfb-run")
    if xvfb_run is None:
        raise SystemExit("xvfb-run is required for headless launches; install the Xvfb package")
    return [xvfb_run, "-a", "-s", shlex.join(screen_args)]
