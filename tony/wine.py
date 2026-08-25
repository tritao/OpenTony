from __future__ import annotations

import getpass
import os
import re
import subprocess
from pathlib import Path

from .common import ROOT, capture, load_yaml, resolve, wine_env
from .identity import recorded_executable
from .nocd import nocd_executable

_LOOP_DEVICE_RE = re.compile(r"(/dev/loop\d+)")


def _normalized_disc() -> Path:
    media_path = resolve(load_yaml("re/config/binaries.yml")["media"]["thps2_pc_disc"]["path"])
    return ROOT / "build" / "disc" / f"{media_path.stem}.iso"


def _existing_loop_device(image: Path) -> str | None:
    status, output = capture(["losetup", "--list", "--output", "NAME,BACK-FILE"])
    if status != 0:
        return None
    for line in output.splitlines():
        if str(image) in line:
            match = _LOOP_DEVICE_RE.search(line)
            if match:
                return match.group(1)
    return None


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


def _set_disc_drive(prefix: Path, mount_point: Path, raw_device: str) -> None:
    dosdevices = prefix / "dosdevices"
    dosdevices.mkdir(parents=True, exist_ok=True)
    drive = dosdevices / "d:"
    if drive.is_symlink():
        drive.unlink()
    elif drive.exists():
        raise SystemExit(f"refusing to replace existing Wine D: mapping: {drive}")
    drive.symlink_to(mount_point)
    raw_drive = dosdevices / "d::"
    if raw_drive.is_symlink():
        raw_drive.unlink()
    elif raw_drive.exists():
        raise SystemExit(f"refusing to replace existing Wine raw D: mapping: {raw_drive}")
    raw_drive.symlink_to(raw_device)

    status, output = capture(
        ["wine", "reg", "add", r"HKCU\Software\Wine\Drives", "/v", "d:", "/d", "cdrom", "/f"],
        env=wine_env(),
    )
    if status != 0:
        raise SystemExit(f"could not configure Wine D: as a CD-ROM:\n{output}")


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

    loop_device = _loop_device(image)
    mount_point = _mounted_target(loop_device)
    prefix = Path(wine_env()["WINEPREFIX"])
    _set_disc_drive(prefix, mount_point, loop_device)

    print(f"Mounted read-only: {image}")
    print(f"Loop device:      {loop_device}")
    print(f"Mount point:      {mount_point}")
    print(f"Wine D:            {mount_point}")
    if not Path(loop_device).exists() or not os.access(loop_device, os.R_OK):
        print(f"WARNING: Wine cannot read the raw device {loop_device} as {getpass.getuser()}.")
        print(f"Run once, while this mapping is active: sudo setfacl -m u:{getpass.getuser()}:r {loop_device}")
    print("Note: loop devices provide the ISO filesystem but not optical CD-ROM TOC ioctls.")
    print("tony run/play use the generated no-CD executable, so physical CD TOC emulation is not required.")

    status, output = capture(["wine", "cmd", "/c", "vol", "D:"], env=wine_env())
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


def run_game(args) -> int:
    exe = nocd_executable()
    env = wine_env()
    command = ["wine", str(exe), *args.game_args]
    print(" ".join(command))
    return subprocess.run(command, cwd=exe.parent, env=env, check=False).returncode
