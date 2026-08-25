from __future__ import annotations

import os
import re
import shutil
import subprocess
from dataclasses import dataclass

from .common import ROOT


@dataclass(frozen=True)
class AudioStart:
    route: MutedAudio | None
    error: str | None = None


@dataclass(frozen=True)
class MutedAudio:
    pactl: str
    module_id: str
    sink_name: str
    pulse_server: str | None


@dataclass(frozen=True)
class AudioCleanup:
    ok: bool
    status: str


def start_muted_audio(env: dict[str, str], session_id: str) -> AudioStart:
    """Create a session-owned silent PulseAudio sink and select it for Wine."""

    pactl = shutil.which("pactl")
    if pactl is None:
        return AudioStart(None, "pactl-unavailable")

    sink_name = "opentony_debug_" + re.sub(r"[^A-Za-z0-9_]", "_", session_id)
    result = subprocess.run(
        [
            pactl,
            "load-module",
            "module-null-sink",
            f"sink_name={sink_name}",
            f"sink_properties=device.description={sink_name}",
        ],
        cwd=ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    module_id = result.stdout.strip()
    if result.returncode != 0 or not re.fullmatch(r"\d+", module_id):
        detail = result.stdout.strip().replace("\n", " ") or f"exit-{result.returncode}"
        return AudioStart(None, f"pactl-load-failed:{detail}")

    env["PULSE_SINK"] = sink_name
    return AudioStart(
        MutedAudio(
            pactl=pactl,
            module_id=module_id,
            sink_name=sink_name,
            pulse_server=env.get("PULSE_SERVER"),
        )
    )


def _pactl_env(data: dict) -> dict[str, str]:
    env = os.environ.copy()
    pulse_server = data.get("audio_pulse_server")
    if pulse_server:
        env["PULSE_SERVER"] = str(pulse_server)
    return env


def cleanup_muted_audio(data: dict) -> AudioCleanup:
    """Unload a recorded sink only after verifying its id and sink name."""

    module_id = data.get("audio_module_id")
    sink_name = data.get("audio_sink")
    if not module_id:
        return AudioCleanup(True, "not-configured")
    if not str(module_id).isdigit() or not sink_name:
        return AudioCleanup(False, "invalid-session-audio-metadata")

    recorded_pactl = data.get("audio_pactl")
    pactl = shutil.which(str(recorded_pactl)) if recorded_pactl else shutil.which("pactl")
    if not pactl:
        return AudioCleanup(False, "pactl-unavailable")
    env = _pactl_env(data)
    listed = subprocess.run(
        [str(pactl), "list", "short", "modules"],
        cwd=ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if listed.returncode != 0:
        return AudioCleanup(False, "pactl-list-failed")

    expected_id = str(module_id)
    expected_sink = f"sink_name={sink_name}"
    module_present = False
    for line in listed.stdout.splitlines():
        fields = line.split(None, 2)
        if not fields or fields[0] != expected_id:
            continue
        module_present = True
        arguments = fields[2] if len(fields) == 3 else ""
        if expected_sink not in arguments.split():
            return AudioCleanup(False, "audio-module-identity-mismatch")
        break

    if not module_present:
        return AudioCleanup(True, "already-removed")

    unloaded = subprocess.run(
        [str(pactl), "unload-module", expected_id],
        cwd=ROOT,
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    if unloaded.returncode != 0:
        return AudioCleanup(False, "pactl-unload-failed")
    return AudioCleanup(True, "removed")
