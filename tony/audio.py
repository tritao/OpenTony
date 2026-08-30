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


def _sink_input_records(pactl: str, env: dict[str, str]) -> tuple[list[dict[str, str]], str | None]:
    """Return the small identity subset needed to move a Pulse stream."""

    listed = subprocess.run(
        [pactl, "list", "sink-inputs"],
        cwd=ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if listed.returncode != 0:
        return [], "pactl-sink-input-list-failed"

    records: list[dict[str, str]] = []
    current: dict[str, str] | None = None
    for line in listed.stdout.splitlines():
        match = re.match(r"^Sink Input #(\d+)$", line)
        if match:
            if current is not None:
                records.append(current)
            current = {"index": match.group(1)}
            continue
        if current is None:
            continue
        match = re.match(r"^\s*Sink:\s+(\S+)", line)
        if match:
            current["sink"] = match.group(1)
            continue
        match = re.match(r'^\s*application\.process\.id\s*=\s*"(\d+)"', line)
        if match:
            current["process_id"] = match.group(1)
    if current is not None:
        records.append(current)
    return records, None


def move_process_audio_to_sink(
    route: MutedAudio,
    process_id: int,
) -> tuple[tuple[tuple[str, str], ...], str | None]:
    """Move only *process_id*'s existing streams to the session null sink."""

    pactl_env = _pactl_env({"audio_pulse_server": route.pulse_server})
    records, error = _sink_input_records(route.pactl, pactl_env)
    if error is not None:
        return (), error
    matches = [
        record
        for record in records
        if record.get("process_id") == str(process_id) and record.get("sink") != route.sink_name
    ]
    if not matches:
        return (), "no-target-audio-stream"

    moved: list[tuple[str, str]] = []
    for record in matches:
        result = subprocess.run(
            [route.pactl, "move-sink-input", record["index"], route.sink_name],
            cwd=ROOT,
            env=pactl_env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        if result.returncode != 0:
            for input_id, original_sink in reversed(moved):
                subprocess.run(
                    [route.pactl, "move-sink-input", input_id, original_sink],
                    cwd=ROOT,
                    env=pactl_env,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    check=False,
                )
            detail = result.stdout.strip().replace("\n", " ") or f"exit-{result.returncode}"
            return (), f"pactl-move-failed:{detail}"
        moved.append((record["index"], record["sink"]))
    return tuple(moved), None


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
    moved_inputs = data.get("audio_moved_inputs", ())
    if moved_inputs:
        records, list_error = _sink_input_records(str(pactl), env)
        if list_error is not None:
            return AudioCleanup(False, list_error)
        current_inputs = {record.get("index"): record for record in records}
        for value in moved_inputs:
            if not isinstance(value, dict):
                return AudioCleanup(False, "invalid-session-audio-metadata")
            input_id = value.get("input_id")
            original_sink = value.get("original_sink")
            if not str(input_id).isdigit() or not str(original_sink).strip():
                return AudioCleanup(False, "invalid-session-audio-metadata")
            current = current_inputs.get(str(input_id))
            # Input indices can be reused after a stream exits.  Only restore
            # a stream that is still attached to this session's sink; never
            # move an unrelated stream merely because its numeric id matches.
            if current is None or current.get("sink") != str(sink_name):
                continue
            restored = subprocess.run(
                [str(pactl), "move-sink-input", str(input_id), str(original_sink)],
                cwd=ROOT,
                env=env,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=False,
            )
            # A process that exited with the debugger may already have
            # destroyed its stream.  That is a successful restore in effect.
            if restored.returncode != 0:
                records, list_error = _sink_input_records(str(pactl), env)
                if list_error is not None:
                    return AudioCleanup(False, list_error)
                if any(record.get("index") == str(input_id) for record in records):
                    return AudioCleanup(False, "pactl-restore-failed")

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
