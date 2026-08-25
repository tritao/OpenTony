from __future__ import annotations

import os
import shlex
import shutil
import subprocess
import tempfile
import time
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

from .common import ROOT

_DISPLAY_READY_SCRIPT = (
    "printf '%s\\n%s\\n' \"$DISPLAY\" \"$XAUTHORITY\" > \"$TONY_DISPLAY_INFO\"; "
    "exec \"$@\""
)


@dataclass(frozen=True)
class DisplayInfo:
    """Connection details for a project-managed Xvfb display."""

    display: str
    xauthority: Path
    width: int
    height: int
    depth: int

    @property
    def environment(self) -> dict[str, str]:
        return {
            "DISPLAY": self.display,
            "XAUTHORITY": str(self.xauthority),
        }


def _screen_config(cfg: dict) -> tuple[int, int, int, list[str]]:
    desktop = cfg.get("virtual_desktop", {})
    width = int(desktop.get("width", 1024))
    height = int(desktop.get("height", 768))
    if width < 320 or height < 200:
        raise SystemExit(f"invalid Xvfb screen size: {width}x{height}")

    xvfb = cfg.get("xvfb", {})
    depth = int(xvfb.get("depth", 16))
    if depth not in {8, 16, 24, 32}:
        raise SystemExit(f"invalid Xvfb screen depth: {depth}; expected one of 8, 16, 24, 32")

    server_args = xvfb.get("server_args", ["+extension", "GLX"])
    if not isinstance(server_args, list) or not all(isinstance(arg, str) for arg in server_args):
        raise SystemExit("wine.yml xvfb.server_args must be a list of strings")
    return width, height, depth, server_args


def _configure_renderer(cfg: dict, env: dict[str, str]) -> None:
    renderer_env = cfg.get("xvfb", {}).get("environment", {})
    if not isinstance(renderer_env, dict):
        raise SystemExit("wine.yml xvfb.environment must be a mapping")
    env.update({str(name): str(value) for name, value in renderer_env.items()})


def xvfb_command(cfg: dict, env: dict[str, str]) -> list[str]:
    """Build the configured Xvfb wrapper and apply its renderer environment."""

    width, height, depth, server_args = _screen_config(cfg)
    _configure_renderer(cfg, env)

    screen_args = ["-screen", "0", f"{width}x{height}x{depth}", *server_args]
    xvfb_run = shutil.which("xvfb-run")
    if xvfb_run is None:
        raise SystemExit("xvfb-run is required for headless launches; install the Xvfb package")
    return [xvfb_run, "-a", "-s", shlex.join(screen_args)]


class HeadlessDisplay:
    """Run a child inside Xvfb while retaining access to its display.

    ``xvfb-run`` normally hides the temporary ``DISPLAY`` and ``XAUTHORITY``
    values from the parent process. The wrapper used here records those values
    before executing the child, allowing screenshots and recordings to share
    the same isolated display without making Xvfb a project-wide daemon.
    """

    def __init__(self, cfg: dict, env: dict[str, str], *, ready_timeout: float = 10.0):
        self.cfg = cfg
        self.base_env = dict(env)
        self.ready_timeout = ready_timeout
        self.info: DisplayInfo | None = None
        self._metadata_path: Path | None = None
        self._recorder: subprocess.Popen | None = None

    @property
    def environment(self) -> dict[str, str]:
        if self.info is None:
            raise RuntimeError("headless display has not been started")
        environment = dict(self.base_env)
        environment.update(self.info.environment)
        return environment

    def popen(
        self,
        command: Sequence[str | Path],
        *,
        cwd: Path | None = None,
        env: dict[str, str] | None = None,
        **kwargs,
    ) -> subprocess.Popen:
        """Start ``command`` in a new Xvfb display and wait for its metadata."""

        launch_env = dict(self.base_env)
        if env is not None:
            launch_env.update(env)
        prefix = xvfb_command(self.cfg, launch_env)
        width, height, depth, _ = _screen_config(self.cfg)

        fd, metadata_name = tempfile.mkstemp(prefix="opentony-display-", suffix=".env")
        os.close(fd)
        metadata_path = Path(metadata_name)
        metadata_path.unlink()
        self._metadata_path = metadata_path
        launch_env["TONY_DISPLAY_INFO"] = str(metadata_path)

        child_command = [str(value) for value in command]
        wrapped = [
            *prefix,
            "sh",
            "-c",
            _DISPLAY_READY_SCRIPT,
            "opentony-headless",
            *child_command,
        ]
        process = None
        try:
            process = subprocess.Popen(wrapped, cwd=cwd, env=launch_env, **kwargs)
            self._wait_for_metadata(process, width, height, depth)
            return process
        except BaseException:
            if process is not None and process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    process.kill()
            metadata_path.unlink(missing_ok=True)
            self._metadata_path = None
            raise

    def _wait_for_metadata(self, process: subprocess.Popen, width: int, height: int, depth: int) -> None:
        if self._metadata_path is None:  # pragma: no cover - internal invariant
            raise RuntimeError("headless display metadata path was not initialized")
        deadline = time.monotonic() + self.ready_timeout
        while time.monotonic() < deadline:
            try:
                values = self._metadata_path.read_text(encoding="utf-8").splitlines()
            except FileNotFoundError:
                values = []
            if len(values) >= 2 and values[0] and values[1]:
                self.info = DisplayInfo(values[0], Path(values[1]), width, height, depth)
                print(f"Headless display: {values[0]} ({width}x{height}x{depth})")
                print(f"X authority:    {values[1]}")
                return
            if process.poll() is not None:
                raise SystemExit("Xvfb child exited before publishing its display metadata")
            time.sleep(0.05)
        raise SystemExit("timed out waiting for Xvfb display metadata")

    def screenshot(self, output: str | Path, *, window: str = "root") -> Path:
        """Capture one frame from the isolated display."""

        self._require_started()
        target = Path(output).expanduser()
        if target.exists():
            raise SystemExit(f"refusing to overwrite screenshot: {target}")
        target.parent.mkdir(parents=True, exist_ok=True)

        ffmpeg = shutil.which("ffmpeg")
        if ffmpeg is not None and window == "root":
            command = [
                ffmpeg,
                "-loglevel",
                "error",
                "-f",
                "x11grab",
                "-video_size",
                f"{self.info.width}x{self.info.height}",
                "-i",
                self.info.display,
                "-frames:v",
                "1",
                "-y",
                str(target),
            ]
        else:
            image_tool = shutil.which("import")
            if image_tool is None:
                raise SystemExit("screenshot capture requires ffmpeg or ImageMagick import")
            command = [image_tool, "-window", window, str(target)]
        result = subprocess.run(command, cwd=ROOT, env=self.environment, check=False)
        if result.returncode != 0:
            raise SystemExit(f"could not capture screenshot: {target}")
        print(f"Screenshot:      {target}")
        return target

    def start_recording(self, output: str | Path, *, framerate: int = 10) -> None:
        """Start an ffmpeg recording until :meth:`stop_recording` is called."""

        self._require_started()
        if self._recorder is not None:
            raise RuntimeError("headless display recording is already active")
        ffmpeg = shutil.which("ffmpeg")
        if ffmpeg is None:
            raise SystemExit("video recording requires ffmpeg")
        target = Path(output).expanduser()
        if target.exists():
            raise SystemExit(f"refusing to overwrite recording: {target}")
        target.parent.mkdir(parents=True, exist_ok=True)
        command = [
            ffmpeg,
            "-loglevel",
            "error",
            "-f",
            "x11grab",
            "-framerate",
            str(framerate),
            "-video_size",
            f"{self.info.width}x{self.info.height}",
            "-i",
            self.info.display,
            "-c:v",
            "libx264",
            "-pix_fmt",
            "yuv420p",
            str(target),
        ]
        self._recorder = subprocess.Popen(command, cwd=ROOT, env=self.environment)
        print(f"Recording:       {target}")

    def stop_recording(self) -> None:
        if self._recorder is None:
            return
        recorder = self._recorder
        self._recorder = None
        if recorder.poll() is None:
            recorder.terminate()
            try:
                recorder.wait(timeout=3)
            except subprocess.TimeoutExpired:
                recorder.kill()
                recorder.wait(timeout=3)
        if recorder.returncode not in (0, 255):
            print(f"WARNING: ffmpeg recording exited with status {recorder.returncode}")

    def close(self) -> None:
        self.stop_recording()
        if self._metadata_path is not None:
            self._metadata_path.unlink(missing_ok=True)
            self._metadata_path = None
        self.info = None

    def _require_started(self) -> None:
        if self.info is None:
            raise RuntimeError("headless display has not been started")


def configure_visual_capture(display: HeadlessDisplay, args) -> None:
    """Apply common screenshot/record options to a running headless display."""

    if getattr(args, "record", None):
        display.start_recording(args.record)
    if getattr(args, "screenshot", None):
        display.screenshot(args.screenshot)
