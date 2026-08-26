from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import yaml

from .common import ROOT, resolve

TYPE_ROOT = ROOT / "re/types"
CONFIDENCE = {"provisional", "inferred", "observed", "confirmed"}
GHIDRA_FIELD_CONFIDENCE = {"observed", "confirmed"}
PRIMITIVE_SIZES = {
    "i8": 1,
    "u8": 1,
    "char": 1,
    "i16": 2,
    "u16": 2,
    "i32": 4,
    "u32": 4,
    "f32": 4,
    "q12_i32": 4,
    "q16_i32": 4,
    "i64": 8,
    "u64": 8,
    "f64": 8,
}
NAME_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_.]*$")


class UniqueKeyLoader(yaml.SafeLoader):
    pass


def _construct_unique_mapping(loader: UniqueKeyLoader, node: yaml.MappingNode, deep: bool = False) -> dict:
    mapping = {}
    for key_node, value_node in node.value:
        key = loader.construct_object(key_node, deep=deep)
        if key in mapping:
            raise yaml.constructor.ConstructorError(
                "while constructing a mapping",
                node.start_mark,
                f"found duplicate key {key!r}",
                key_node.start_mark,
            )
        mapping[key] = loader.construct_object(value_node, deep=deep)
    return mapping


UniqueKeyLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, _construct_unique_mapping)


@dataclass(frozen=True)
class TypeExpression:
    kind: str
    name: str | None = None
    element: TypeExpression | None = None
    count: int | None = None


def _split_generic(value: str) -> tuple[str, str] | None:
    opening = value.find("<")
    if opening <= 0 or not value.endswith(">"):
        return None
    return value[:opening], value[opening + 1 : -1]


def _split_array_arguments(value: str) -> tuple[str, str] | None:
    depth = 0
    for index, character in enumerate(value):
        if character == "<":
            depth += 1
        elif character == ">":
            depth -= 1
        elif character == "," and depth == 0:
            return value[:index], value[index + 1 :]
    return None


def parse_type_expression(value: object) -> TypeExpression:
    if not isinstance(value, str) or not value or any(character.isspace() for character in value):
        raise ValueError("type must be a nonempty canonical string without whitespace")
    if value in PRIMITIVE_SIZES or value in {"cstring", "bytes"}:
        return TypeExpression("primitive", name=value)
    generic = _split_generic(value)
    if generic:
        constructor, arguments = generic
        if constructor == "pointer":
            return TypeExpression("pointer", element=parse_type_expression(arguments))
        if constructor == "sequence":
            return TypeExpression("sequence", element=parse_type_expression(arguments))
        if constructor == "bytes":
            if not arguments.isdecimal() or int(arguments) <= 0:
                raise ValueError("bytes count must be a positive decimal integer")
            return TypeExpression("bytes", count=int(arguments))
        if constructor == "array":
            split = _split_array_arguments(arguments)
            if split is None:
                raise ValueError("array requires an element type and positive count")
            element, count = split
            if not count.isdecimal() or int(count) <= 0:
                raise ValueError("array count must be a positive decimal integer")
            return TypeExpression("array", element=parse_type_expression(element), count=int(count))
        raise ValueError(f"unknown type constructor {constructor!r}")
    if NAME_RE.fullmatch(value):
        return TypeExpression("named", name=value)
    raise ValueError(f"invalid type expression {value!r}")


def _expression_size(expression: TypeExpression, known_sizes: dict[str, int | None], pointer_size: int) -> int | None:
    if expression.kind == "primitive":
        return PRIMITIVE_SIZES.get(expression.name or "")
    if expression.kind == "pointer":
        return pointer_size
    if expression.kind == "bytes":
        return expression.count
    if expression.kind == "array":
        element_size = _expression_size(expression.element, known_sizes, pointer_size)  # type: ignore[arg-type]
        return element_size * expression.count if element_size is not None and expression.count is not None else None
    if expression.kind == "named":
        return known_sizes.get(expression.name or "")
    return None


def _evidence_errors(evidence: object, context: str) -> list[str]:
    errors: list[str] = []
    if not isinstance(evidence, list) or not evidence:
        return [f"{context}: evidence must be a nonempty list"]
    for item in evidence:
        path = item if isinstance(item, str) else item.get("file") if isinstance(item, dict) else None
        if not isinstance(path, str):
            errors.append(f"{context}: evidence entry must be a path or mapping with file")
        elif not resolve(path).is_file():
            errors.append(f"{context}: evidence file does not exist: {path}")
    return errors


def validate_type_documents(root: Path = TYPE_ROOT) -> tuple[list[str], dict[str, int]]:
    errors: list[str] = []
    documents: list[tuple[Path, dict[str, Any]]] = []
    type_defs: dict[str, tuple[Path, dict[str, Any]]] = {}
    counts = {"files": 0, "types": 0, "fields": 0}
    for path in sorted(root.glob("*.yml")):
        try:
            with path.open("r", encoding="utf-8") as stream:
                document = yaml.load(stream, Loader=UniqueKeyLoader) or {}
        except yaml.YAMLError as exc:
            errors.append(f"{path}: invalid YAML: {exc}")
            continue
        documents.append((path, document))
        counts["files"] += 1
        if document.get("version") != 1:
            errors.append(f"{path}: version must be 1")
        for definition in document.get("types", []):
            name = definition.get("name") if isinstance(definition, dict) else None
            if not isinstance(name, str) or not NAME_RE.fullmatch(name):
                errors.append(f"{path}: type has invalid name {name!r}")
                continue
            if name in type_defs:
                errors.append(f"{path}: duplicate type name {name!r}; first defined in {type_defs[name][0]}")
            else:
                type_defs[name] = (path, definition)
            counts["types"] += 1

    known_sizes = {
        name: definition.get("size") if isinstance(definition.get("size"), int) else None
        for name, (_path, definition) in type_defs.items()
    }
    for path, document in documents:
        pointer_size = 4
        for definition in document.get("types", []):
            if not isinstance(definition, dict) or not isinstance(definition.get("name"), str):
                continue
            name = definition["name"]
            context = f"{path}:{name}"
            kind = definition.get("kind")
            if kind not in {"fixed_layout", "variable_record", "alias", "opaque"}:
                errors.append(f"{context}: invalid or missing kind")
                continue
            confidence = definition.get("confidence")
            if confidence not in CONFIDENCE:
                errors.append(f"{context}: invalid confidence {confidence!r}")
            if kind == "alias":
                target = definition.get("target")
                if target not in type_defs:
                    errors.append(f"{context}: alias target does not exist: {target!r}")
                if definition.get("fields"):
                    errors.append(f"{context}: alias must not define fields")
                continue
            size = definition.get("size")
            if size is not None and (not isinstance(size, int) or size <= 0):
                errors.append(f"{context}: size must be null or a positive integer")
            fields = definition.get("fields", [])
            if not isinstance(fields, list):
                errors.append(f"{context}: fields must be a list")
                continue
            field_names: set[str] = set()
            occupied: list[tuple[int, int, str, str | None]] = []
            for field in fields:
                counts["fields"] += 1
                if not isinstance(field, dict):
                    errors.append(f"{context}: field must be a mapping")
                    continue
                field_name = field.get("name")
                field_context = f"{context}.{field_name}"
                if not isinstance(field_name, str) or not NAME_RE.fullmatch(field_name):
                    errors.append(f"{context}: invalid field name {field_name!r}")
                elif field_name in field_names:
                    errors.append(f"{context}: duplicate field name {field_name!r}")
                else:
                    field_names.add(field_name)
                if field.get("confidence") not in CONFIDENCE:
                    errors.append(f"{field_context}: invalid confidence {field.get('confidence')!r}")
                errors.extend(_evidence_errors(field.get("evidence", definition.get("evidence")), field_context))
                try:
                    expression = parse_type_expression(field.get("type"))
                except ValueError as exc:
                    errors.append(f"{field_context}: {exc}")
                    continue
                for referenced in _referenced_names(expression):
                    if referenced not in type_defs and referenced != "void":
                        errors.append(f"{field_context}: unknown referenced type {referenced!r}")
                offset = field.get("offset")
                if not isinstance(offset, int):
                    if kind != "variable_record" or not isinstance(offset, str):
                        errors.append(f"{field_context}: offset must be an integer for {kind}")
                    continue
                field_size = _expression_size(expression, known_sizes, pointer_size)
                if field_size is not None:
                    end = offset + field_size
                    if isinstance(size, int) and end > size:
                        errors.append(f"{field_context}: field ends at 0x{end:x}, beyond size 0x{size:x}")
                    overlay = field.get("overlay_group")
                    for other_start, other_end, other_name, other_overlay in occupied:
                        if offset < other_end and end > other_start and (not overlay or overlay != other_overlay):
                            errors.append(f"{field_context}: overlaps {other_name} without a shared overlay_group")
                    occupied.append((offset, end, str(field_name), overlay if isinstance(overlay, str) else None))
    return errors, counts


def load_type_definitions(root: Path = TYPE_ROOT) -> dict[str, dict[str, Any]]:
    """Load the validated corpus in deterministic name order."""

    errors, _counts = validate_type_documents(root)
    if errors:
        raise ValueError("invalid recovered type corpus:\n" + "\n".join(errors))
    definitions: dict[str, dict[str, Any]] = {}
    for path in sorted(root.glob("*.yml")):
        with path.open("r", encoding="utf-8") as stream:
            document = yaml.load(stream, Loader=UniqueKeyLoader) or {}
        for definition in document.get("types", []):
            definitions[str(definition["name"])] = definition
    return dict(sorted(definitions.items()))


def ghidra_type_plan(root: Path = TYPE_ROOT) -> dict[str, Any]:
    """Return the conservative, Ghidra-independent type import plan."""

    definitions = load_type_definitions(root)
    known_sizes = {
        name: definition.get("size") if isinstance(definition.get("size"), int) else None
        for name, definition in definitions.items()
    }
    imported: list[dict[str, Any]] = []
    skipped: list[dict[str, str]] = []
    aliases: list[dict[str, str]] = []

    for name, definition in definitions.items():
        kind = str(definition["kind"])
        if kind == "alias":
            aliases.append({"name": name, "target": str(definition["target"])})
            continue
        if kind == "variable_record":
            skipped.append({"name": name, "reason": "variable_record has no fixed Ghidra layout"})
            continue

        fields: list[dict[str, Any]] = []
        for field in definition.get("fields", []):
            field_name = str(field["name"])
            confidence = str(field["confidence"])
            if confidence not in GHIDRA_FIELD_CONFIDENCE:
                skipped.append({"name": f"{name}.{field_name}", "reason": f"{confidence} confidence"})
                continue
            if field.get("overlay_group"):
                skipped.append({"name": f"{name}.{field_name}", "reason": "overlapping field requires a union"})
                continue
            expression = parse_type_expression(field["type"])
            field_size = _expression_size(expression, known_sizes, 4)
            if not isinstance(field.get("offset"), int) or field_size is None:
                skipped.append({"name": f"{name}.{field_name}", "reason": "non-fixed offset or size"})
                continue
            fields.append(
                {
                    "name": field_name,
                    "offset": int(field["offset"]),
                    "type": str(field["type"]),
                    "size": field_size,
                    "comment": str(field.get("description", "")),
                }
            )

        declared_size = definition.get("size")
        extent = max((field["offset"] + field["size"] for field in fields), default=0)
        size = declared_size if isinstance(declared_size, int) else extent
        if size <= 0:
            skipped.append({"name": name, "reason": "no fixed extent"})
            continue
        imported.append(
            {
                "name": name,
                "kind": kind,
                "size": size,
                "description": str(definition.get("description", "")),
                "fields": sorted(fields, key=lambda item: (item["offset"], item["name"])),
            }
        )
        for alias in sorted(definition.get("aliases", [])):
            aliases.append({"name": str(alias), "target": name})

    return {
        "version": 1,
        "category": "/OpenTony/Recovered",
        "types": imported,
        "aliases": sorted(aliases, key=lambda item: item["name"]),
        "skipped": sorted(skipped, key=lambda item: item["name"]),
    }


def _referenced_names(expression: TypeExpression) -> set[str]:
    if expression.kind == "named":
        return {expression.name or ""}
    if expression.element is not None:
        return _referenced_names(expression.element)
    return set()


def types_verify(_args) -> int:
    errors, counts = validate_type_documents()
    print(f"recovered types: {counts['types']} types, {counts['fields']} fields across {counts['files']} files")
    if errors:
        for error in errors:
            print(f"FAIL {error}")
        return 1
    print("recovered type layouts: VALID")
    return 0
