from __future__ import annotations

import json
import os
import socket
from datetime import UTC, datetime
from pathlib import Path
from types import SimpleNamespace

from .common import ROOT, capture, load_yaml, resolve
from .native_progress import load_native_progress
from .recovered_types import load_type_definitions

SLICE_ROOT = ROOT / "re/slices"
# Tests and embedding callers may override this. Production claims live in the
# Git common directory so every worktree sees the same leases.
LEASE_ROOT: Path | None = None
LEGACY_LEASE_ROOT = ROOT / "build/slices/leases"
STATUSES = {"planned", "active", "paused", "complete"}


def load_slices(root: Path = SLICE_ROOT) -> dict[str, dict]:
    slices = {}
    for path in sorted(root.glob("*.yml")):
        document = load_yaml(path)
        slice_id = document.get("id")
        if isinstance(slice_id, str):
            document["_path"] = str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path)
            slices[slice_id] = document
    return slices


def slice_for_address(address: int) -> dict | None:
    for document in load_slices().values():
        if address in {int(value) for value in document.get("scope", {}).get("functions", [])}:
            return {"id": document["id"], "subsystem": document.get("subsystem"), "status": document.get("status")}
    return None


def validate_slices(root: Path = SLICE_ROOT) -> tuple[list[str], dict[str, int]]:
    errors = []
    documents = load_slices(root)
    paths = sorted(root.glob("*.yml"))
    if len(documents) != len(paths):
        errors.append(f"{root}: every manifest must have a unique string id")
    functions = {
        int(item["address"]): item["name"]
        for item in load_yaml("re/symbols/functions.yml").get("functions", [])
    }
    types = set(load_type_definitions())
    globals_ = set()
    for path, key in (("re/symbols/globals.yml", "globals"), ("re/symbols/data.yml", "data")):
        globals_.update(item["name"] for item in load_yaml(path).get(key, []))
    active_owners: dict[int, str] = {}

    for slice_id, document in documents.items():
        context = document["_path"]
        if Path(context).stem != slice_id:
            errors.append(f"{context}: filename must match id {slice_id!r}")
        if document.get("version") != 1:
            errors.append(f"{context}: version must be 1")
        if document.get("status") not in STATUSES:
            errors.append(f"{context}: invalid status {document.get('status')!r}")
        if not isinstance(document.get("subsystem"), str) or not document["subsystem"]:
            errors.append(f"{context}: subsystem must be a nonempty string")
        scope = document.get("scope")
        if not isinstance(scope, dict):
            errors.append(f"{context}: scope must be a mapping")
            continue
        shared = {int(value) for value in document.get("shared", {}).get("functions", [])}
        for address in scope.get("functions", []):
            if not isinstance(address, int) or address not in functions:
                errors.append(f"{context}: unknown function address {address!r}")
                continue
            if document.get("status") == "active" and address not in shared:
                previous = active_owners.get(address)
                if previous:
                    errors.append(f"{context}: function 0x{address:08x} also belongs to active slice {previous}")
                active_owners[address] = slice_id
        for name in scope.get("types", []):
            if name not in types:
                errors.append(f"{context}: unknown recovered type {name!r}")
        for name in scope.get("globals", []):
            if name not in globals_:
                errors.append(f"{context}: unknown global/data symbol {name!r}")
        artifacts = document.get("artifacts")
        if not isinstance(artifacts, dict):
            errors.append(f"{context}: artifacts must be a mapping")
        else:
            for key in ("evidence", "native", "tests"):
                values = artifacts.get(key, [])
                if not isinstance(values, list):
                    errors.append(f"{context}: artifacts.{key} must be a list")
                    continue
                for value in values:
                    if not isinstance(value, str) or not resolve(value).is_file():
                        errors.append(f"{context}: missing artifact {value!r}")
        questions = document.get("open_questions")
        completion = document.get("completion")
        if not isinstance(questions, list) or not isinstance(completion, list) or not completion:
            errors.append(f"{context}: open_questions and nonempty completion lists are required")
        if document.get("status") == "active" and not questions:
            errors.append(f"{context}: active slice must list at least one open question")
        if document.get("status") == "complete" and questions:
            errors.append(f"{context}: complete slice must not have open questions")

    for address, item in load_native_progress().items():
        slice_id = item.get("slice")
        if slice_id is None:
            continue
        document = documents.get(slice_id)
        if document is None:
            errors.append(f"re/native/functions.yml:{item['name']}: unknown slice {slice_id!r}")
        elif address not in {int(value) for value in document.get("scope", {}).get("functions", [])}:
            errors.append(f"re/native/functions.yml:{item['name']}: address is outside slice {slice_id}")
    return errors, {"slices": len(documents), "active": sum(item.get("status") == "active" for item in documents.values())}


def slice_verify(_args: SimpleNamespace | None = None) -> int:
    errors, counts = validate_slices()
    print(f"reconstruction slices: {counts['slices']} total, {counts['active']} active")
    for error in errors:
        print(f"FAIL {error}")
    if errors:
        return 1
    print("reconstruction slices: VALID")
    return 0


def slice_list(_args) -> int:
    documents = load_slices()
    for slice_id, document in documents.items():
        lease = _read_lease(slice_id)
        owner = lease.get("owner", "-") if lease else "-"
        print(f"{slice_id:<24} {document['status']:<9} {document['subsystem']:<16} {owner}")
    return 0


def _lease_summary(lease: dict | None) -> tuple[str, str, str]:
    if not lease:
        return "-", "-", "unclaimed"
    owner = str(lease.get("owner", "-"))
    age = "unknown"
    try:
        started = datetime.fromisoformat(str(lease["started_at"]))
        seconds = max(0, int((datetime.now(UTC) - started).total_seconds()))
        if seconds < 60:
            age = f"{seconds}s"
        elif seconds < 3600:
            age = f"{seconds // 60}m"
        elif seconds < 86400:
            age = f"{seconds // 3600}h"
        else:
            age = f"{seconds // 86400}d"
    except (KeyError, TypeError, ValueError):
        pass
    state = "remote"
    if lease.get("host") == socket.gethostname() and isinstance(lease.get("pid"), int):
        try:
            os.kill(lease["pid"], 0)
            state = "live"
        except ProcessLookupError:
            state = "stale"
        except PermissionError:
            state = "live"
    return owner, age, state


def _native_slice_progress(document: dict, progress: dict[int, dict]) -> tuple[int, int, str]:
    addresses = [int(value) for value in document.get("scope", {}).get("functions", [])]
    entries = [progress[address] for address in addresses if address in progress]
    statuses: dict[str, int] = {}
    for entry in entries:
        status = str(entry.get("status", "unknown"))
        statuses[status] = statuses.get(status, 0) + 1
    detail = ",".join(f"{name}:{count}" for name, count in sorted(statuses.items())) or "none"
    return len(entries), len(addresses), detail


def slice_status(args) -> int:
    documents = load_slices()
    if args.slice_id:
        documents = {args.slice_id: _require_slice(args.slice_id)}
    progress = load_native_progress()
    branch_code, branch = capture(["git", "branch", "--show-current"])
    dirty_code, dirty = capture(["git", "status", "--short"])
    branch = branch.strip() if branch_code == 0 and branch.strip() else "detached"
    dirty_count = len(dirty.splitlines()) if dirty_code == 0 and dirty else 0
    print(f"worktree {ROOT}  branch {branch}  {'clean' if dirty_count == 0 else f'dirty:{dirty_count}'}")
    print(f"{'slice':<28} {'manifest':<9} {'native':<9} {'lease':<18} {'age':<7} {'state':<9} next")
    for slice_id, document in documents.items():
        lease = _read_lease(slice_id)
        owner, age, lease_state = _lease_summary(lease)
        done, total, detail = _native_slice_progress(document, progress)
        questions = document.get("open_questions", [])
        next_question = questions[0] if questions else "completion criteria"
        print(
            f"{slice_id:<28} {document['status']:<9} {f'{done}/{total}':<9} "
            f"{owner[:18]:<18} {age:<7} {lease_state:<9} {next_question}"
        )
        if args.slice_id:
            print(f"native-status {detail}")
            if lease:
                print(
                    f"lease-host {lease.get('host', '-')}  pid {lease.get('pid', '-')}  "
                    f"base {str(lease.get('base_commit', '-'))[:12]}"
                )
    return 0


def slice_show(args) -> int:
    document = _require_slice(args.slice_id)
    printable = {key: value for key, value in document.items() if key != "_path"}
    printable["lease"] = _read_lease(args.slice_id)
    printable["workflow"] = [
        "inspect target and confirm its exact boundary/ABI",
        "recover accessed fields and sync Ghidra",
        "record a semantic model and tests",
        "attempt matching C/C++; use the documented assembly fallback when needed",
        "implement portable native behavior and update native progress",
        "run tony verify --all and pytest -q",
    ]
    print(json.dumps(printable, indent=2))
    return 0


def slice_prompt(args) -> int:
    document = _require_slice(args.slice_id)
    returncode, branch = capture(["git", "branch", "--show-current"])
    if returncode:
        raise SystemExit("could not determine current Git branch")
    branch = branch.strip()
    if not branch:
        returncode, branch = capture(["git", "rev-parse", "--short", "HEAD"])
        if returncode:
            raise SystemExit("could not determine current Git revision")
        branch = f"detached@{branch.strip()}"

    questions = document.get("open_questions", [])
    priorities = "\n".join(f"- {question}" for question in questions[:3]) or "- Follow the slice completion criteria."
    print(
        f"""Work on the OpenTony reconstruction slice `{args.slice_id}`.

Repository: {ROOT}
Branch: {branch}

Read `AGENTS.md` and `docs/SLICE_WORKFLOW.md`. Claim the slice with
`tony slice claim {args.slice_id}`. Run `tony worktree verify`; do not download
inputs, run setup, or rebuild Ghidra during slice work. If readiness fails,
report it instead. Inspect with `tony slice show {args.slice_id}`, then select
one scoped target with `tony ghidra gaps --slice {args.slice_id}`.

Current priorities:
{priorities}

Complete one coherent evidence, ABI/type, semantic model/test, matching, and
native-progress unit. Do not guess semantics or classify naked assembly as C++.
Before finishing, run `tony verify --all` and `pytest -q`, commit only coherent
slice changes on this branch, then run `tony slice release {args.slice_id}`."""
    )
    return 0


def _owner(explicit: str | None) -> str:
    owner = explicit or os.environ.get("CODEX_SESSION_ID") or os.environ.get("TONY_SLICE_OWNER")
    if not owner:
        raise SystemExit("slice owner is unknown; pass --owner or set TONY_SLICE_OWNER")
    return owner


def _lease_root() -> Path:
    if LEASE_ROOT is not None:
        return LEASE_ROOT
    returncode, git_common_dir = capture(["git", "rev-parse", "--path-format=absolute", "--git-common-dir"])
    if returncode:
        raise SystemExit("could not determine Git common directory for slice claims")
    path = Path(git_common_dir.strip())
    if not path.is_absolute():
        path = (ROOT / path).resolve()
    return path / "opentony/slice-leases"


def _lease_path(slice_id: str) -> Path:
    return _lease_root() / f"{slice_id}.json"


def _read_lease(slice_id: str) -> dict | None:
    path = _lease_path(slice_id)
    legacy = LEGACY_LEASE_ROOT / f"{slice_id}.json"
    if not path.is_file() and legacy.is_file() and path != legacy:
        path.parent.mkdir(parents=True, exist_ok=True)
        legacy.replace(path)
    return json.loads(path.read_text(encoding="utf-8")) if path.is_file() else None


def _require_slice(slice_id: str) -> dict:
    document = load_slices().get(slice_id)
    if document is None:
        raise SystemExit(f"unknown reconstruction slice: {slice_id}")
    return document


def slice_claim(args) -> int:
    document = _require_slice(args.slice_id)
    owner = _owner(args.owner)
    existing = _read_lease(args.slice_id)
    if existing and existing.get("owner") != owner and not args.force:
        raise SystemExit(f"slice {args.slice_id} is claimed by {existing.get('owner')}; use --force to replace")
    returncode, base_commit = capture(["git", "rev-parse", "HEAD"])
    if returncode:
        raise SystemExit("could not determine current Git commit")
    lease = {
        "slice": args.slice_id,
        "owner": owner,
        "started_at": datetime.now(UTC).isoformat(),
        "base_commit": base_commit.strip(),
        "host": socket.gethostname(),
        "pid": os.getpid(),
    }
    _lease_root().mkdir(parents=True, exist_ok=True)
    _lease_path(args.slice_id).write_text(json.dumps(lease, indent=2) + "\n", encoding="utf-8")
    owned = {
        path
        for key in ("evidence", "native", "tests")
        for path in document.get("artifacts", {}).get(key, [])
    }
    status_code, status = capture(["git", "status", "--short"])
    if status_code == 0:
        dirty = [line for line in status.splitlines() if line[3:] in owned]
        for line in dirty:
            print(f"WARN owned artifact already modified: {line}")
    print(f"Claimed {args.slice_id} for {owner} at {lease['base_commit'][:12]}")
    return 0


def slice_release(args) -> int:
    _require_slice(args.slice_id)
    existing = _read_lease(args.slice_id)
    if existing is None:
        print(f"Slice {args.slice_id} is not claimed")
        return 0
    owner = _owner(args.owner)
    if existing.get("owner") != owner and not args.force:
        raise SystemExit(f"slice {args.slice_id} is claimed by {existing.get('owner')}; use --force to release")
    _lease_path(args.slice_id).unlink()
    print(f"Released {args.slice_id}")
    return 0
