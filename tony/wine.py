from __future__ import annotations

import getpass
import os
import re
import subprocess
from collections.abc import Iterator
from contextlib import contextmanager
from fcntl import LOCK_EX, flock
from pathlib import Path

from .audio import cleanup_muted_audio, start_muted_audio
from .common import ROOT, capture, headless_wine_command, headless_wine_env, load_yaml, resolve, wine_env
from .display import HeadlessDisplay, configure_visual_capture, terminate_process
from .identity import recorded_executable
from .nocd import nocd_executable

_LOOP_DEVICE_RE = re.compile(r"(/dev/loop\d+)")


def _normalized_disc() -> Path:
    media_path = resolve(load_yaml("re/config/binaries.yml")["media"]["thps2_pc_disc"]["path"])
    return ROOT / "build" / "disc" / f"{media_path.stem}.iso"


@contextmanager
def _disc_lock(image: Path) -> Iterator[None]:
    """Serialize loop, mount, and Wine mapping updates across worktrees."""

    resolved = image.resolve()
    lock_path = resolved.parent / f".{resolved.name}.mount.lock"
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    with lock_path.open("a+", encoding="utf-8") as stream:
        flock(stream.fileno(), LOCK_EX)
        yield


def _existing_loop_device(image: Path) -> str | None:
    status, output = capture(["losetup", "--list", "--output", "NAME,BACK-FILE"])
    if status != 0:
        return None
    resolved_image = image.resolve()
    devices = []
    for line in output.splitlines():
        match = _LOOP_DEVICE_RE.search(line)
        if not match:
            continue
        backing_text = line[match.end():].strip()
        if not backing_text or backing_text == "BACK-FILE":
            continue
        try:
            same_image = Path(backing_text).resolve() == resolved_image
        except OSError:
            same_image = False
        if same_image:
            devices.append(match.group(1))
    # Prefer the loop already mapped as Wine's raw D: device. This makes a
    # repeated call a genuine no-op even if an older race left several
    # mounted loop devices for the same image.
    raw_drive = Path(wine_env()["WINEPREFIX"]) / "dosdevices" / "d::"
    if raw_drive.is_symlink():
        configured = str(raw_drive.resolve(strict=False))
        if configured in devices:
            mounted, target = capture(
                ["findmnt", "-rn", "-S", configured, "-o", "TARGET"]
            )
            if mounted == 0 and target.strip():
                return configured
    # Otherwise prefer any loop that is already mounted.
    for device in devices:
        mounted, target = capture(["findmnt", "-rn", "-S", device, "-o", "TARGET"])
        if mounted == 0 and target.strip():
            return device
    return devices[0] if devices else None


def _loop_device(image: Path) -> str:
    existing = _existing_loop_device(image)
    if existing:
        return existing

    status, output = capture(["udisksctl", "loop-setup", "--read-only", "--file", str(image)])
    if status == 0:
        match = _LOOP_DEVICE_RE.search(output)
        if match:
            return match.group(1)

    # Some udisks versions return a failure when the image is already mapped.
    existing = _existing_loop_device(image)
    if existing:
        return existing
    raise SystemExit(f"could not create or find a loop device for {image}:\n{output}")


def _mounted_target(loop_device: str) -> Path:
    status, output = capture(["findmnt", "-rn", "-S", loop_device, "-o", "TARGET"])
    if status != 0 or not output:
        status, output = capture(["udisksctl", "mount", "-b", loop_device])
        if status != 0:
            raise SystemExit(f"could not mount {loop_device}:\n{output}")
        status, output = capture(["findmnt", "-rn", "-S", loop_device, "-o", "TARGET"])
    if status != 0 or not output:
        raise SystemExit(f"could not determine the mount point for {loop_device}:\n{output}")
    return Path(output.splitlines()[0].strip())


def _wine_capture(command: list[str], *, timeout: float = 20.0) -> tuple[int, str]:
    try:
        result = subprocess.run(
            command,
            cwd=ROOT,
            env=wine_env(),
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        output = (
            exc.stdout.decode(errors="replace")
            if isinstance(exc.stdout, bytes)
            else exc.stdout
        )
        return 124, (output or f"timed out after {timeout:g}s")
    return result.returncode, result.stdout.strip()


def _set_disc_drive(prefix: Path, mount_point: Path, raw_device: str) -> bool:
    dosdevices = prefix / "dosdevices"
    dosdevices.mkdir(parents=True, exist_ok=True)
    drive = dosdevices / "d:"
    raw_drive = dosdevices / "d::"
    expected_mount = mount_point.resolve(strict=False)
    expected_raw = Path(raw_device).resolve(strict=False)
    if (
        drive.is_symlink()
        and raw_drive.is_symlink()
        and drive.resolve(strict=False) == expected_mount
        and raw_drive.resolve(strict=False) == expected_raw
    ):
        return False
    if drive.is_symlink():
        drive.unlink()
    elif drive.exists():
        raise SystemExit(f"refusing to replace existing Wine D: mapping: {drive}")
    drive.symlink_to(mount_point)
    if raw_drive.is_symlink():
        raw_drive.unlink()
    elif raw_drive.exists():
        raise SystemExit(f"refusing to replace existing Wine raw D: mapping: {raw_drive}")
    raw_drive.symlink_to(raw_device)

    status, output = _wine_capture(
        ["wine", "reg", "add", r"HKCU\Software\Wine\Drives", "/v", "d:", "/d", "cdrom", "/f"]
    )
    if status != 0:
        raise SystemExit(f"could not configure Wine D: as a CD-ROM:\n{output}")
    return True


def wine_init(_args) -> int:
    env = wine_env()
    prefix = Path(env["WINEPREFIX"])
    prefix.mkdir(parents=True, exist_ok=True)
    print(f"Initializing Wine prefix: {prefix}")
    return subprocess.run(["wineboot", "-u"], cwd=ROOT, env=env, check=False).returncode


def wine_mount_disc(_args) -> int:
    image = _normalized_disc()
    if not image.is_file():
        raise SystemExit(f"normalized disc image not found: {image}\nRun: tony media extract")

    with _disc_lock(image):
        loop_device = _loop_device(image)
        mount_point = _mounted_target(loop_device)
        prefix = Path(wine_env()["WINEPREFIX"])
        mapping_changed = _set_disc_drive(prefix, mount_point, loop_device)

    print(f"Mounted read-only: {image}")
    print(f"Loop device:      {loop_device}")
    print(f"Mount point:      {mount_point}")
    print(f"Wine D:            {mount_point}")
    if not Path(loop_device).exists() or not os.access(loop_device, os.R_OK):
        print(f"WARNING: Wine cannot read the raw device {loop_device} as {getpass.getuser()}.")
        print(f"Run once, while this mapping is active: sudo setfacl -m u:{getpass.getuser()}:r {loop_device}")
    print("Note: loop devices provide the ISO filesystem but not optical CD-ROM TOC ioctls.")
    print("tony run/play use the generated no-CD executable, so physical CD TOC emulation is not required.")

    if not mapping_changed:
        print("Wine D: mapping already configured.")
        return 0
    status, output = _wine_capture(["wine", "cmd", "/c", "vol", "D:"])
    if status != 0:
        raise SystemExit(f"Wine cannot read D::\n{output}")
    print(output)
    return 0


def wine_unmount_disc(_args) -> int:
    image = _normalized_disc()
    status, output = capture(["losetup", "--list", "--output", "NAME,BACK-FILE"])
    if status != 0:
        raise SystemExit(output or "losetup is required to find the mounted disc")

    devices = []
    for line in output.splitlines():
        if str(image) in line:
            match = _LOOP_DEVICE_RE.search(line)
            if match:
                devices.append(match.group(1))
    if not devices:
        dosdevices = Path(wine_env()["WINEPREFIX"]) / "dosdevices"
        for drive in (dosdevices / "d:", dosdevices / "d::"):
            if drive.is_symlink() and not drive.resolve(strict=False).exists():
                drive.unlink()
                print(f"Removed stale Wine mapping: {drive}")
        print(f"No loop device found for {image}")
        return 0

    prefix = Path(wine_env()["WINEPREFIX"])
    drive = prefix / "dosdevices" / "d:"
    raw_drive = prefix / "dosdevices" / "d::"
    for device in devices:
        status, mount_output = capture(["findmnt", "-rn", "-S", device, "-o", "TARGET"])
        mount_point = Path(mount_output.splitlines()[0].strip()) if status == 0 and mount_output else None
        status, output = capture(["udisksctl", "unmount", "-b", device])
        harmless_unmount = ("not mounted", "no such device", "no such file or directory")
        if status != 0 and not any(message in output.lower() for message in harmless_unmount):
            raise SystemExit(f"could not unmount {device}:\n{output}")
        status, output = capture(["udisksctl", "loop-delete", "-b", device])
        harmless_delete = ("no such device", "no such file or directory", "no such address")
        if status != 0 and not any(message in output.lower() for message in harmless_delete):
            raise SystemExit(f"could not delete {device}:\n{output}")
        if drive.is_symlink() and mount_point and drive.resolve(strict=False) == mount_point.resolve(strict=False):
            drive.unlink()
        if raw_drive.is_symlink() and raw_drive.resolve(strict=False) == Path(device).resolve(strict=False):
            raw_drive.unlink()
        print(f"Unmounted and removed loop device: {device}")
    return 0


def _recorded_exe():
    return recorded_executable()


def _virtual_desktop_command() -> list[str]:
    config = load_yaml("re/config/wine.yml")["wine"].get("virtual_desktop", {})
    if not config.get("enabled", True):
        return []
    name = str(config.get("name", "OpenTony"))
    width = int(config.get("width", 1024))
    height = int(config.get("height", 768))
    if width < 320 or height < 200:
        raise SystemExit(f"invalid Wine virtual desktop size: {width}x{height}")
    return ["explorer", f"/desktop={name},{width}x{height}"]


def run_game(args) -> int:
    exe = nocd_executable()
    headless = getattr(args, "headless", False)
    env = headless_wine_env() if headless else wine_env()
    command = ["wine", *_virtual_desktop_command(), str(exe), *args.game_args]
    print(" ".join(command))
    if not headless:
        if getattr(args, "screenshot", None) or getattr(args, "record", None):
            raise SystemExit("visual capture requires --headless")
        return subprocess.run(command, cwd=exe.parent, env=env, check=False).returncode

    audio_start = start_muted_audio(env, f"run_headless_{os.getpid()}")
    if audio_start.route is None:
        raise SystemExit(
            f"could not mute headless game audio: {audio_start.error}"
        )
    audio_route = audio_start.route
    cfg = load_yaml("re/config/wine.yml")["wine"]
    display = HeadlessDisplay(cfg, env)
    process = None
    try:
        process = display.popen(headless_wine_command(command), cwd=exe.parent, env=env)
        configure_visual_capture(display, args)
        return process.wait()
    except BaseException:
        if process is not None and process.poll() is None:
            terminate_process(process)
        raise
    finally:
        display.stop_recording()
        subprocess.run(["wineserver", "-k"], cwd=ROOT, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        display.close()
        cleanup_muted_audio(
            {
                "audio_pactl": audio_route.pactl,
                "audio_module_id": audio_route.module_id,
                "audio_sink": audio_route.sink_name,
                "audio_pulse_server": audio_route.pulse_server,
            }
        )
