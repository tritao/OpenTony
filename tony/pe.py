from __future__ import annotations

import json

import pefile

from .common import load_yaml, relative_to_root, resolve, save_yaml, sha256

_I386_MACHINE = 0x014C
_PE32_MAGIC = 0x010B


def _machine_name(machine: int) -> str:
    return {0x014C: "i386", 0x8664: "amd64", 0x01C0: "arm"}.get(machine, "unknown")


def _section_summary(pe) -> list[dict]:
    sections = []
    for section in pe.sections:
        sections.append({
            "name": section.Name.rstrip(b"\x00").decode("ascii", errors="replace"),
            "virtual_address": int(section.VirtualAddress),
            "virtual_size": int(section.Misc_VirtualSize),
            "raw_size": int(section.SizeOfRawData),
            "raw_offset": int(section.PointerToRawData),
            "characteristics": f"0x{int(section.Characteristics):08x}",
        })
    return sections


def _import_summary(pe) -> list[dict]:
    try:
        pe.parse_data_directories(
            directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]]
        )
    except (AttributeError, pefile.PEFormatError):
        return []

    summary = []
    for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        dll = entry.dll.decode("ascii", errors="replace") if entry.dll else ""
        summary.append({"dll": dll, "count": len(entry.imports)})
    return summary


def exe_identify(args) -> int:
    path = resolve(args.path)
    if not path.is_file():
        raise SystemExit(f"executable not found: {path}")
    try:
        pe = pefile.PE(str(path))
    except pefile.PEFormatError as exc:
        raise SystemExit(f"not a valid PE executable: {path}: {exc}") from exc

    machine_value = int(pe.FILE_HEADER.Machine)
    magic_value = int(pe.OPTIONAL_HEADER.Magic)
    machine = f"0x{machine_value:04x}"
    if machine_value != _I386_MACHINE or magic_value != _PE32_MAGIC:
        raise SystemExit(
            "unsupported executable: expected PE32/i386 "
            f"(machine={machine}, optional_header=0x{magic_value:04x})"
        )

    image_base = int(pe.OPTIONAL_HEADER.ImageBase)
    ep_rva = int(pe.OPTIONAL_HEADER.AddressOfEntryPoint)
    sections = _section_summary(pe)
    imports = _import_summary(pe)
    record = {
        "path": relative_to_root(path),
        "size": path.stat().st_size,
        "sha256": sha256(path),
        "machine": machine,
        "machine_name": _machine_name(machine_value),
        "pe_format": "PE32",
        "optional_header_magic": f"0x{magic_value:04x}",
        "pe_timestamp": int(pe.FILE_HEADER.TimeDateStamp),
        "image_base": image_base,
        "entry_point_rva": ep_rva,
        "entry_point_va": image_base + ep_rva,
        "section_count": len(sections),
        "sections": sections,
        "import_dlls": imports,
        "import_count": sum(item["count"] for item in imports),
    }
    print(json.dumps(record, indent=2))
    if args.record:
        config = load_yaml("re/config/binaries.yml")
        config["executables"]["thps2_pc"].update(record)
        save_yaml("re/config/binaries.yml", config)
        print("Recorded identity in re/config/binaries.yml")
    return 0
