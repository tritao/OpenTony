from __future__ import annotations

import hashlib
import os
import re
import shutil
import subprocess
from itertools import pairwise
from pathlib import Path
from types import SimpleNamespace

import pefile

from .common import ROOT, load_yaml, resolve, save_yaml, sha256

MANIFEST = ROOT / "match/manifest.yml"
DEFAULT_CHUNK_SIZE = 64 * 1024


def _source_executable() -> Path:
    spec = load_yaml("re/config/binaries.yml")["executables"]["thps2_pc"]
    source = resolve(spec["path"])
    if not source.is_file():
        raise SystemExit(f"recorded executable not found: {source}")
    expected = spec.get("sha256")
    if expected and sha256(source) != expected:
        raise SystemExit(f"recorded executable SHA-256 mismatch: {source}")
    return source


def _load_manifest() -> dict:
    if not MANIFEST.is_file():
        raise SystemExit("split manifest not found; run: tony split init")
    manifest = load_yaml(MANIFEST)
    if manifest.get("version") != 1:
        raise SystemExit(f"unsupported split manifest version: {manifest.get('version')!r}")
    return manifest


def _section_name(raw_name: bytes) -> str:
    return raw_name.rstrip(b"\0").decode("ascii", errors="replace")


def _module_id(section: str, start_va: int) -> str:
    clean_section = section.lstrip(".").replace("/", "_") or "section"
    return f"{clean_section}_{start_va:08x}"


def _module_source(module: dict) -> Path:
    source = Path(module["source"])
    return source if source.is_absolute() else ROOT / source


def _original_path(module: dict) -> Path:
    return ROOT / "match/original/modules" / f"{module['id']}.bin"


def _built_path(module: dict) -> Path:
    return ROOT / "match/generated/modules" / f"{module['id']}.bin"


def _write_raw_source(module: dict) -> None:
    source = _module_source(module)
    source.parent.mkdir(parents=True, exist_ok=True)
    original = _original_path(module)
    relative_original = Path(os.path.relpath(original, source.parent))
    source.write_text(
        "BITS 32\n"
        f"org 0x{module['start_va']:08x}\n\n"
        f"; Raw bootstrap module: {module['id']}\n"
        f'incbin "{relative_original.as_posix()}"\n',
        encoding="utf-8",
    )


def _address(value: str) -> int:
    try:
        return int(value, 0)
    except ValueError as exc:
        raise SystemExit(f"invalid address: {value}") from exc


def _symbol_name(name: str) -> str:
    symbol = re.sub(r"[^A-Za-z0-9_.$#@~?]", "_", name)
    if not symbol or symbol[0].isdigit():
        symbol = f"symbol_{symbol}"
    return symbol


def split_symbols(_args) -> int:
    symbols: dict[str, int] = {}
    for path, key in (
        ("re/symbols/functions.yml", "functions"),
        ("re/symbols/globals.yml", "globals"),
        ("re/symbols/data.yml", "data"),
        ("re/symbols/strings.yml", "strings"),
    ):
        for item in load_yaml(path).get(key, []):
            if "name" not in item or "address" not in item:
                continue
            name = _symbol_name(str(item["name"]))
            address = int(item["address"])
            previous = symbols.get(name)
            if previous is not None and previous != address:
                raise SystemExit(f"symbol name maps to multiple addresses: {name}")
            symbols[name] = address
    output = ROOT / "match/generated/symbols.inc"
    output.parent.mkdir(parents=True, exist_ok=True)
    lines = ["; Generated from re/symbols/*.yml. Do not edit.", ""]
    width = max((len(name) for name in symbols), default=1)
    lines.extend(f"{name:<{width}} equ 0x{address:08x}" for name, address in sorted(symbols.items()))
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Generated {len(symbols)} symbols: {output}")
    return 0


def split_module(args) -> int:
    manifest = _load_manifest()
    _require_valid_coverage(manifest)
    start_va = _address(args.start_va)
    end_va = _address(args.end_va)
    if end_va <= start_va:
        raise SystemExit("module end must be greater than its start")
    owners = [
        module
        for module in manifest["modules"]
        if int(module["start_va"]) <= start_va and end_va <= int(module["end_va"])
    ]
    if len(owners) != 1:
        raise SystemExit("requested range must be contained within exactly one module")
    owner = owners[0]
    if owner.get("status") != "raw":
        raise SystemExit(f"refusing to split non-raw module: {owner['id']}")
    original = _original_path(owner)
    if not original.is_file():
        raise SystemExit(f"original module not found: {original}; run: tony split extract")
    owner_bytes = original.read_bytes()
    boundaries = [int(owner["start_va"]), start_va, end_va, int(owner["end_va"])]
    children = []
    for child_start, child_end in pairwise(boundaries):
        if child_start == child_end:
            continue
        delta = child_start - int(owner["start_va"])
        child_bytes = owner_bytes[delta : delta + child_end - child_start]
        module_id = _module_id(owner["section"], child_start)
        child = {
            "id": module_id,
            "section": owner["section"],
            "start_va": child_start,
            "end_va": child_end,
            "file_offset": int(owner["file_offset"]) + delta,
            "size": len(child_bytes),
            "status": "raw",
            "source": f"match/modules/{owner['section'].lstrip('.') or 'section'}/{module_id}.asm",
            "sha256": hashlib.sha256(child_bytes).hexdigest(),
        }
        output = _original_path(child)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(child_bytes)
        _write_raw_source(child)
        children.append(child)

    owner_index = manifest["modules"].index(owner)
    manifest["modules"][owner_index : owner_index + 1] = children
    _require_valid_coverage(manifest)
    save_yaml(MANIFEST, manifest)
    child_sources = {_module_source(child) for child in children}
    child_originals = {_original_path(child) for child in children}
    if _module_source(owner) not in child_sources:
        _module_source(owner).unlink(missing_ok=True)
    if original not in child_originals:
        original.unlink(missing_ok=True)
    _built_path(owner).unlink(missing_ok=True)
    print(f"Split {owner['id']} into: {', '.join(child['id'] for child in children)}")
    return 0


def _select_module(manifest: dict, selector: str) -> dict:
    by_id = [module for module in manifest["modules"] if module["id"] == selector]
    if by_id:
        return by_id[0]
    address = _address(selector)
    matches = [
        module for module in manifest["modules"] if int(module["start_va"]) <= address < int(module["end_va"])
    ]
    if len(matches) != 1:
        raise SystemExit(f"no unique module owns address 0x{address:08x}")
    return matches[0]


def _disassembly(path: Path, module: dict, mismatch_va: int) -> str | None:
    if not shutil.which("objdump"):
        return None
    module_start = int(module["start_va"])
    module_end = int(module["end_va"])
    result = subprocess.run(
        [
            "objdump",
            "-D",
            "-b",
            "binary",
            "-m",
            "i386",
            "-M",
            "intel",
            f"--adjust-vma=0x{module_start:x}",
            f"--start-address=0x{max(module_start, mismatch_va - 8):x}",
            f"--stop-address=0x{min(module_end, mismatch_va + 16):x}",
            str(path),
        ],
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode:
        return None
    lines = result.stdout.splitlines()
    instructions = [line for line in lines if line.lstrip().startswith(tuple("0123456789abcdef"))]
    return "\n".join(instructions) or None


def split_compare(args) -> int:
    module = _select_module(_load_manifest(), args.module)
    original = _original_path(module)
    built = _built_path(module)
    if not original.is_file():
        raise SystemExit(f"original module not found: {original}")
    if not built.is_file():
        raise SystemExit(f"built module not found: {built}; run: tony split build")
    expected = original.read_bytes()
    actual = built.read_bytes()
    prefix = 0
    while prefix < min(len(expected), len(actual)) and expected[prefix] == actual[prefix]:
        prefix += 1
    print(f"module: {module['id']}")
    print(f"expected size: {len(expected)}")
    print(f"actual size:   {len(actual)}")
    print(f"matching prefix: {prefix} bytes")
    if expected == actual:
        print("result: BYTE IDENTICAL")
        return 0
    mismatch_va = int(module["start_va"]) + prefix
    print(f"first mismatch VA: 0x{mismatch_va:08x}")
    print(f"expected: {expected[prefix:prefix + 8].hex(' ')}")
    print(f"actual:   {actual[prefix:prefix + 8].hex(' ')}")
    expected_disassembly = _disassembly(original, module, mismatch_va)
    actual_disassembly = _disassembly(built, module, mismatch_va)
    if expected_disassembly and actual_disassembly:
        print(f"expected disassembly:\n{expected_disassembly}")
        print(f"actual disassembly:\n{actual_disassembly}")
    return 1


def split_init(args) -> int:
    source = _source_executable()
    if MANIFEST.exists() and not args.force:
        raise SystemExit(f"split manifest already exists: {MANIFEST}; pass --force to replace it")
    pe = pefile.PE(str(source), fast_load=True)
    image_base = int(pe.OPTIONAL_HEADER.ImageBase)
    chunk_size = args.chunk_size
    if chunk_size <= 0:
        raise SystemExit("chunk size must be positive")

    sections = []
    modules = []
    for pe_section in pe.sections:
        raw_size = int(pe_section.SizeOfRawData)
        raw_offset = int(pe_section.PointerToRawData)
        rva = int(pe_section.VirtualAddress)
        virtual_size = int(pe_section.Misc_VirtualSize)
        name = _section_name(pe_section.Name)
        sections.append(
            {
                "name": name,
                "rva": rva,
                "start_va": image_base + rva,
                "file_offset": raw_offset,
                "raw_size": raw_size,
                "virtual_size": virtual_size,
                "zero_fill_size": max(virtual_size - raw_size, 0),
            }
        )
        for section_offset in range(0, raw_size, chunk_size):
            size = min(chunk_size, raw_size - section_offset)
            start_va = image_base + rva + section_offset
            module_id = _module_id(name, start_va)
            modules.append(
                {
                    "id": module_id,
                    "section": name,
                    "start_va": start_va,
                    "end_va": start_va + size,
                    "file_offset": raw_offset + section_offset,
                    "size": size,
                    "status": "raw",
                    "source": f"match/modules/{name.lstrip('.') or 'section'}/{module_id}.asm",
                }
            )

    manifest = {
        "version": 1,
        "image_base": image_base,
        "source": str(source.relative_to(ROOT)),
        "source_size": source.stat().st_size,
        "source_sha256": sha256(source),
        "chunk_size": chunk_size,
        "sections": sections,
        "modules": modules,
    }
    save_yaml(MANIFEST, manifest)
    for module in modules:
        _write_raw_source(module)
    print(f"Created {MANIFEST} with {len(modules)} modules across {len(sections)} sections")
    return split_extract(SimpleNamespace())


def split_extract(_args) -> int:
    source = _source_executable()
    manifest = _load_manifest()
    _require_valid_coverage(manifest)
    source_bytes = source.read_bytes()
    for module in manifest["modules"]:
        start = int(module["file_offset"])
        end = start + int(module["size"])
        data = source_bytes[start:end]
        if len(data) != int(module["size"]):
            raise SystemExit(f"module extends beyond source file: {module['id']}")
        output = _original_path(module)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(data)
        module["sha256"] = hashlib.sha256(data).hexdigest()
    save_yaml(MANIFEST, manifest)
    print(f"Extracted {len(manifest['modules'])} original modules")
    return 0


def _validate_coverage(manifest: dict) -> list[str]:
    errors = []
    modules_by_section: dict[str, list[dict]] = {}
    for module in manifest.get("modules", []):
        modules_by_section.setdefault(module["section"], []).append(module)
    for section in manifest.get("sections", []):
        expected = int(section["file_offset"])
        section_end = expected + int(section["raw_size"])
        modules = sorted(modules_by_section.pop(section["name"], []), key=lambda item: int(item["file_offset"]))
        for module in modules:
            start = int(module["file_offset"])
            end = start + int(module["size"])
            if start != expected:
                kind = "overlap" if start < expected else "gap"
                errors.append(f"{section['name']}: {kind} before {module['id']} at file offset 0x{start:x}")
            if int(module["end_va"]) - int(module["start_va"]) != int(module["size"]):
                errors.append(f"{module['id']}: VA range does not match size")
            expected_va = int(section["start_va"]) + start - int(section["file_offset"])
            if int(module["start_va"]) != expected_va:
                errors.append(f"{module['id']}: VA does not match its section file offset")
            expected = max(expected, end)
        if expected != section_end:
            errors.append(f"{section['name']}: coverage ends at 0x{expected:x}, expected 0x{section_end:x}")
    for section_name in modules_by_section:
        errors.append(f"modules reference unknown section: {section_name}")
    return errors


def _require_valid_coverage(manifest: dict) -> None:
    errors = _validate_coverage(manifest)
    if errors:
        raise SystemExit("invalid split coverage:\n" + "\n".join(f"  {error}" for error in errors))


def split_build(_args) -> int:
    manifest = _load_manifest()
    _require_valid_coverage(manifest)
    if not shutil.which("nasm"):
        raise SystemExit("NASM is required; install it or rerun the platform bootstrap")
    for module in manifest["modules"]:
        source = _module_source(module)
        output = _built_path(module)
        output.parent.mkdir(parents=True, exist_ok=True)
        result = subprocess.run(
            ["nasm", "-f", "bin", source.name, "-o", str(output)],
            cwd=source.parent,
            text=True,
            capture_output=True,
            check=False,
        )
        if result.returncode:
            raise SystemExit(f"NASM failed for {module['id']}:\n{result.stderr.strip()}")
    print(f"Built {len(manifest['modules'])} modules")
    return 0


def split_rebuild(args) -> int:
    if not args.no_build:
        split_build(args)
    source = _source_executable()
    manifest = _load_manifest()
    _require_valid_coverage(manifest)
    rebuilt = bytearray(source.read_bytes())
    for module in manifest["modules"]:
        built = _built_path(module)
        if not built.is_file():
            raise SystemExit(f"built module not found: {built}; run: tony split build")
        data = built.read_bytes()
        expected_size = int(module["size"])
        if len(data) != expected_size:
            raise SystemExit(f"built size mismatch for {module['id']}: expected {expected_size}, got {len(data)}")
        start = int(module["file_offset"])
        rebuilt[start : start + expected_size] = data
    output = resolve(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(rebuilt)
    print(f"Rebuilt executable: {output}")
    return 0


def split_verify(_args) -> int:
    manifest = _load_manifest()
    errors = _validate_coverage(manifest)
    matching = 0
    reconstructed = 0
    section_status: dict[str, dict[str, int]] = {}
    for module in manifest.get("modules", []):
        status = str(module.get("status", "raw"))
        if status not in {"raw", "asm", "cpp"}:
            errors.append(f"unknown module status {status!r}: {module['id']}")
        totals = section_status.setdefault(module["section"], {"raw": 0, "asm": 0, "cpp": 0})
        if status in totals:
            totals[status] += int(module["size"])
        built = _built_path(module)
        original = _original_path(module)
        if not original.is_file():
            errors.append(f"original module missing: {module['id']}")
            continue
        expected_hash = module.get("sha256")
        if not expected_hash:
            errors.append(f"original hash missing from manifest: {module['id']}")
        elif sha256(original) != expected_hash:
            errors.append(f"original module hash differs from manifest: {module['id']}")
        if not built.is_file():
            errors.append(f"built module missing: {module['id']}")
            continue
        if built.read_bytes() != original.read_bytes():
            errors.append(f"module bytes differ: {module['id']}")
        else:
            matching += 1
        if status != "raw":
            reconstructed += int(module["size"])

    source = _source_executable()
    rebuilt = ROOT / "match/generated/THawk2.rebuilt.exe"
    identical = rebuilt.is_file() and sha256(rebuilt) == sha256(source)
    if not identical:
        errors.append("rebuilt executable is missing or differs from the recorded executable")
    total = sum(int(module["size"]) for module in manifest.get("modules", []))
    print(f"coverage: {total} file bytes across {len(manifest.get('sections', []))} sections")
    for section in manifest.get("sections", []):
        statuses = section_status.get(section["name"], {"raw": 0, "asm": 0, "cpp": 0})
        raw_size = int(section["raw_size"])
        reconstructed_size = statuses["asm"] + statuses["cpp"]
        percent = reconstructed_size * 100 / raw_size if raw_size else 0
        print(
            f"  {section['name']}: raw={statuses['raw']} asm={statuses['asm']} "
            f"cpp={statuses['cpp']} reconstructed={percent:.2f}%"
        )
    print(f"modules: {matching}/{len(manifest.get('modules', []))} byte-identical")
    print(f"reconstructed: {reconstructed}/{total} bytes")
    print(f"rebuilt executable: {'BYTE IDENTICAL' if identical else 'DIFFERS'}")
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        return 1
    return 0
