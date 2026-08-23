#!/usr/bin/env python3
"""Prepare k2c generated C for native Plan 9 compilers.

The k2c output intentionally uses GNU __auto_type for inferred local
declarations. Hosted compilers accept that, but native Plan 9 8c does not.
This script copies build/kryon/generated to build/plan9/generated and rewrites
only those inferred declarations to the concrete type reported by GCC's
syntax-only tree dump.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from collections import defaultdict, deque
from pathlib import Path


INCLUDE_DIRS = [
    "src",
    "src/app",
    "src/core",
    "src/screens",
    "src/screens/settings",
    "src/practices",
    "src/practices/whm",
    "src/practices/meditation",
    "src/practices/sun_salutation",
    "src/storage",
    "src/platform",
    "src/platform/android",
    "src/third_party",
    "vendor/kryon/include",
    "vendor-builds/sqlite",
    "build/kryon/generated",
    "build/kryon/generated/src",
]

EMBEDDED_ASSET_PATTERNS = [
    "locales/*.txt",
    "assets/app/icon.png",
    "assets/easteregg/art.png",
    "assets/easteregg/waozi.png",
    "assets/practices/whm/1.png",
    "assets/practices/whm/2.png",
    "assets/practices/meditation/1.png",
    "assets/pet/egg1.png",
    "assets/practices/*/banner.png",
    "assets/practices/sunsalutation/poses_man_sheet.png",
    "assets/practices/sunsalutation/poses_woman_sheet.png",
    "assets/practices/sunsalutation/transition_01_01_to_02_man_sheet.png",
    "assets/practices/sunsalutation/transition_02_02_to_03_man_sheet.png",
    "assets/sounds/*.ogg",
    "assets/fonts/subset/NotoSans-Inbe-Regular.ttf",
    "assets/fonts/subset/NotoSansSC-Inbe-Regular.otf",
    "assets/fonts/subset/NotoSansJP-Inbe-Regular.otf",
    "assets/fonts/subset/NotoSansKR-Inbe-Regular.otf",
    "assets/fonts/subset/NotoSansTC-Inbe-Regular.otf",
]


FUNCTION_RE = re.compile(r"^;; Function ([A-Za-z_][A-Za-z0-9_]*) ")
AUTOTYPE_RE = re.compile(r"^(\s*)__auto_type\s+([A-Za-z_][A-Za-z0-9_]*)\s*=")
CAST_INIT_RE = re.compile(r"=\s*\(([^(){};]+)\)")
ARRAY_COMPOUND_INIT_RE = re.compile(r"=\s*\((?P<base>[^(){};\[\]]+)\[(?P<size>[^\]]+)\]\)\{(?P<body>.*)\};\s*$")
INT_INIT_RE = re.compile(r"=\s*[-+]?[0-9]+[UuLl]*\s*;")
FLOAT_INIT_RE = re.compile(r"=\s*[-+]?(?:[0-9]*\.[0-9]+|[0-9]+\.[0-9]*)(?:[fF])?\s*;")
FOR_DECL_RE = re.compile(
    r"^(\s*)for\(\s*((?:unsigned\s+)?(?:int|long)|size_t)\s+"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^;]+);\s*(.*\{\s*)$"
)
LOCAL_COMPOUND_RE = re.compile(
    r"^(\s*)((?:(?:struct|enum)\s+)?[A-Za-z_][A-Za-z0-9_]*)\s+"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*"
    r"\(((?:(?:struct|enum)\s+)?[A-Za-z_][A-Za-z0-9_]*)\)\{(.*)\};\s*$"
)
COMPOUND_START_RE = re.compile(r"\(((?:(?:struct|enum)\s+)?[A-Za-z_][A-Za-z0-9_]*)\)\{")
DECL_RE = re.compile(
    r"^\s*(?P<type>(?:(?:static|const|volatile|unsigned|signed|short|long)\s+)*"
    r"(?:(?:struct|enum)\s+)?[A-Za-z_][A-Za-z0-9_]*"
    r"(?:\s+(?:const|volatile))?(?:\s*\*)*)\s+"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*(?:=|;|\[)"
)
NESTED_FIELD_TYPES = {
    "bounds": "Rectangle",
    "color": "Color",
}


def normalize_type(type_text: str) -> str:
    text = " ".join(type_text.replace(" *", "*").replace("*", " *").split())
    text = text.replace(" *", " *")
    if text.startswith("static "):
        text = text[len("static "):]
    def replace_tag(match: re.Match[str]) -> str:
        kind = match.group(1)
        name = match.group(2)
        if kind == "struct" and name in {"tm", "stat", "dirent"}:
            return match.group(0)
        return name

    text = re.sub(r"\b(struct|enum)\s+([A-Za-z_][A-Za-z0-9_]*)", replace_tag, text)
    if text == "_Bool":
        text = "int"
    return text


def split_top_level(text: str) -> list[str]:
    parts: list[str] = []
    start = 0
    paren = brace = bracket = 0
    in_string = False
    escape = False
    for i, ch in enumerate(text):
        if in_string:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_string = False
            continue
        if ch == '"':
            in_string = True
        elif ch == "(":
            paren += 1
        elif ch == ")":
            paren -= 1
        elif ch == "{":
            brace += 1
        elif ch == "}":
            brace -= 1
        elif ch == "[":
            bracket += 1
        elif ch == "]":
            bracket -= 1
        elif ch == "," and paren == 0 and brace == 0 and bracket == 0:
            parts.append(text[start:i].strip())
            start = i + 1
    tail = text[start:].strip()
    if tail:
        parts.append(tail)
    return parts


def designated_fields(body: str) -> list[tuple[str, str]] | None:
    fields: list[tuple[str, str]] = []
    for part in split_top_level(body):
        match = re.match(r"^\.\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)$", part)
        if not match:
            return None
        fields.append((match.group(1), match.group(2).strip()))
    return fields


def emit_compound_temp(indent: str, type_name: str, body: str, temp: str) -> list[str]:
    body = body.strip()
    if body == "0":
        return [
            f"{indent}{type_name} {temp};",
            f"{indent}memset(&{temp}, 0, sizeof({temp}));",
        ]
    fields = designated_fields(body)
    if fields is not None:
        lines = [
            f"{indent}{type_name} {temp};",
            f"{indent}memset(&{temp}, 0, sizeof({temp}));",
        ]
        for index, (field, value) in enumerate(fields):
            nested_type = NESTED_FIELD_TYPES.get(field)
            if nested_type is not None and value.startswith("{") and value.endswith("}"):
                nested = f"{temp}_{field}_{index}"
                lines.append(f"{indent}{nested_type} {nested} = {value};")
                lines.append(f"{indent}{temp}.{field} = {nested};")
            else:
                lines.append(f"{indent}{temp}.{field} = {value};")
        return lines
    return [f"{indent}{type_name} {temp} = {{{body}}};"]


def find_matching_brace(text: str, open_index: int) -> int:
    depth = 0
    in_string = False
    escape = False
    for i in range(open_index, len(text)):
        ch = text[i]
        if in_string:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_string = False
            continue
        if ch == '"':
            in_string = True
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return i
    return -1


def brace_delta(line: str) -> int:
    depth = 0
    in_string = False
    escape = False
    for ch in line:
        if in_string:
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_string = False
            continue
        if ch == '"':
            in_string = True
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
    return depth


def inject_plan9_generated_header(text: str) -> str:
    include = '#include "inbe_plan9_generated.h"'
    if include in text:
        return text
    lines = text.splitlines()
    insert_at = 0
    while insert_at < len(lines) and lines[insert_at].startswith("/*"):
        insert_at += 1
    lines.insert(insert_at, include)
    return "\n".join(lines) + "\n"


def expose_weak_declarations_for_plan9(text: str) -> str:
    return text.replace(
        "#if defined(__GNUC__) || defined(__clang__)\n"
        "void *CreateApp(const char *project_path) __attribute__((weak));\n"
        "void DestroyApp(void *app) __attribute__((weak));\n"
        "void ApplyRoute(void *app, const AppRouteInfo *route) __attribute__((weak));\n"
        "void BeginScreenDraw(void *app, Rectangle viewport) __attribute__((weak));\n"
        "#endif\n",
        "void *CreateApp(const char *project_path) __attribute__((weak));\n"
        "void DestroyApp(void *app) __attribute__((weak));\n"
        "void ApplyRoute(void *app, const AppRouteInfo *route) __attribute__((weak));\n"
        "void BeginScreenDraw(void *app, Rectangle viewport) __attribute__((weak));\n",
    )


def rewrite_for_declarations(text: str) -> tuple[str, int]:
    out: list[str] = []
    rewritten = 0
    depth = 0
    pending_closes: list[tuple[int, str]] = []

    for line in text.splitlines():
        match = FOR_DECL_RE.match(line)
        if match:
            indent, type_name, name, init, rest = match.groups()
            base_depth = depth
            out.append(f"{indent}{{")
            out.append(f"{indent}    {type_name} {name};")
            out.append(f"{indent}    for({name} = {init}; {rest}")
            depth += brace_delta(line)
            pending_closes.append((base_depth, indent))
            rewritten += 1
        else:
            out.append(line)
            depth += brace_delta(line)

        while pending_closes and depth == pending_closes[-1][0]:
            _, close_indent = pending_closes.pop()
            out.append(f"{close_indent}}}")

    return "\n".join(out) + "\n", rewritten


def rewrite_compound_literals(text: str) -> tuple[str, int]:
    out: list[str] = []
    rewritten = 0
    for lineno, line in enumerate(text.splitlines(), 1):
        local = LOCAL_COMPOUND_RE.match(line)
        if local and local.group(2) == local.group(4):
            indent, type_name, name, _, body = local.groups()
            out.extend(emit_compound_temp(indent, type_name, body, name))
            rewritten += 1
            continue

        indent_match = re.match(r"^(\s*)", line)
        indent = indent_match.group(1) if indent_match else ""
        cursor = 0
        rebuilt = ""
        temps: list[str] = []
        temp_index = 0
        while True:
            match = COMPOUND_START_RE.search(line, cursor)
            if not match:
                rebuilt += line[cursor:]
                break
            open_brace = match.end() - 1
            close_brace = find_matching_brace(line, open_brace)
            if close_brace < 0:
                rebuilt += line[cursor:]
                break
            type_name = match.group(1)
            body = line[open_brace + 1:close_brace]
            temp = f"__inbe_plan9_l{lineno}_{temp_index}"
            temp_index += 1
            temps.extend(emit_compound_temp(indent, type_name, body, temp))
            rebuilt += line[cursor:match.start()] + temp
            cursor = close_brace + 1
            rewritten += 1
        if temps:
            out.extend(temps)
            out.append(rebuilt)
        else:
            out.append(line)
    return "\n".join(out) + "\n", rewritten


def source_function_names(text: str) -> list[str]:
    names: list[str] = []
    lines = text.splitlines()
    for i, line in enumerate(lines):
        if not line.startswith("{") or i == 0:
            continue
        j = i - 1
        while j >= 0 and lines[j].strip() == "":
            j -= 1
        if j < 0:
            continue
        match = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*\(", lines[j].strip())
        if match:
            names.append(match.group(1))
    return names


def run_gcc_dump(root: Path, source: Path, dump_path: Path) -> None:
    cmd = [
        "gcc",
        "-std=gnu99",
        "-fsyntax-only",
        f"-fdump-tree-original={dump_path}",
        "-DANDROID_BUILD=0",
        "-DUI_EMBEDDED_ONLY=1",
        "-DMINIZ_NO_ZLIB_COMPATIBLE_NAMES=1",
    ]
    for inc in INCLUDE_DIRS:
        cmd.append(f"-I{root / inc}")
    cmd.append(str(source))
    subprocess.run(cmd, cwd=root, check=True)


def parse_dump(dump_path: Path, wanted_functions: set[str]) -> dict[str, dict[str, deque[str]]]:
    result: dict[str, dict[str, deque[str]]] = defaultdict(lambda: defaultdict(deque))
    current: str | None = None
    with dump_path.open("r", encoding="utf-8", errors="replace") as f:
        for line in f:
            match = FUNCTION_RE.match(line)
            if match:
                current = match.group(1)
                continue
            if current not in wanted_functions:
                continue
            decl = DECL_RE.match(line)
            if not decl:
                continue
            name = decl.group("name")
            if name.startswith("D."):
                continue
            type_text = normalize_type(decl.group("type"))
            if type_text in {"if", "for", "while", "return", "goto"}:
                continue
            result[current][name].append(type_text)
    return result


def rewrite_source(text: str, types: dict[str, dict[str, deque[str]]], source: Path) -> tuple[str, int]:
    out: list[str] = []
    current: str | None = None
    pending: str | None = None
    depth = 0
    unresolved = 0

    for lineno, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if depth == 0:
            match = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*\(", stripped)
            if match:
                pending = match.group(1)

        if stripped == "{" and depth == 0 and pending is not None:
            current = pending
            pending = None

        auto = AUTOTYPE_RE.match(line)
        if auto and current is not None:
            indent, name = auto.groups()
            array_init = ARRAY_COMPOUND_INIT_RE.search(line)
            if array_init:
                base = normalize_type(array_init.group("base").strip())
                size = array_init.group("size").strip()
                body = array_init.group("body").strip()
                line = f"{indent}{base} {name}[{size}] = {{{body}}};"
                out.append(line)
                depth += line.count("{") - line.count("}")
                if depth <= 0:
                    depth = 0
                    current = None
                continue
            candidates = types.get(current, {}).get(name)
            if candidates:
                type_text = candidates.popleft()
            else:
                cast = CAST_INIT_RE.search(line)
                if cast:
                    type_text = normalize_type(cast.group(1))
                elif INT_INIT_RE.search(line):
                    type_text = "int"
                elif FLOAT_INIT_RE.search(line):
                    type_text = "float"
                else:
                    unresolved += 1
                    out.append(line)
                    depth += line.count("{") - line.count("}")
                    if depth <= 0:
                        depth = 0
                        current = None
                    continue
            line = AUTOTYPE_RE.sub(f"{indent}{type_text} {name} =", line, count=1)

        out.append(line)
        depth += line.count("{") - line.count("}")
        if depth <= 0:
            depth = 0
            current = None

    return "\n".join(out) + "\n", unresolved


def write_file_list(root: Path, out_dir: Path, file_list: Path) -> int:
    files = []
    for dst in sorted(out_dir.rglob("*.c")):
        rel = dst.relative_to(out_dir)
        rel_text = rel.as_posix()
        if "/platform/android/" in f"/{rel_text}":
            continue
        files.append((out_dir / rel).relative_to(root).as_posix())

    file_list.parent.mkdir(parents=True, exist_ok=True)
    file_list.write_text("\n".join(files) + "\n", encoding="utf-8")
    return len(files)


def write_plan9_include_wrappers(include_dir: Path) -> None:
    include_dir.mkdir(parents=True, exist_ok=True)
    (include_dir / "inbe_plan9_generated.h").write_text(
        """#ifndef INBE_PLAN9_GENERATED_H
#define INBE_PLAN9_GENERATED_H
extern int view_width;
extern int view_height;
#endif
""",
        encoding="utf-8",
    )
    (include_dir / "miniz.h").write_text(
        """#ifndef INBE_PLAN9_MINIZ_WRAPPER_H
#define INBE_PLAN9_MINIZ_WRAPPER_H
#include \"../../../src/third_party/miniz.h\"
#endif
""",
        encoding="utf-8",
    )


def generate_embedded_assets(root: Path, output: Path) -> None:
    files: list[Path] = []
    for pattern in EMBEDDED_ASSET_PATTERNS:
        files.extend(sorted(root.glob(pattern)))
    rel_files = [path.relative_to(root).as_posix() for path in sorted(set(files))]
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["sh", "vendor/kryon/scripts/embed-assets.sh", output.as_posix(), *rel_files],
        cwd=root,
        check=True,
    )


SQLITE_UINT64_RE = re.compile(r"UINT64_C\(0x([0-9a-fA-F]+)\)")


def rewrite_sqlite_for_plan9(root: Path, sqlite_out: Path) -> None:
    sqlite_src = root / "vendor-builds/sqlite/sqlite3.c"
    text = sqlite_src.read_text(encoding="utf-8", errors="replace")

    def replace_uint64(match: re.Match[str]) -> str:
        value = int(match.group(1), 16)
        hi = (value >> 32) & 0xFFFFFFFF
        lo = value & 0xFFFFFFFF
        return f"(((u64)0x{hi:08x}UL << 32) | (u64)0x{lo:08x}UL)"

    sqlite_out.parent.mkdir(parents=True, exist_ok=True)
    sqlite_out.write_text(SQLITE_UINT64_RE.sub(replace_uint64, text),
                          encoding="utf-8")


def prepare(root: Path, generated: Path, out_dir: Path, dump_dir: Path, file_list: Path, include_dir: Path, embedded_assets: Path, sqlite_out: Path) -> int:
    if not generated.is_dir():
        print(f"missing generated directory: {generated}", file=sys.stderr)
        return 1
    if out_dir.exists():
        shutil.rmtree(out_dir)
    shutil.copytree(generated, out_dir)
    dump_dir.mkdir(parents=True, exist_ok=True)

    rewritten = 0
    unresolved = 0
    for dst in sorted(out_dir.rglob("*.c")):
        rel = dst.relative_to(out_dir)
        src = generated / rel
        text = src.read_text(encoding="utf-8", errors="replace")
        rewritten_text = inject_plan9_generated_header(text)
        rewritten_text = expose_weak_declarations_for_plan9(rewritten_text)
        file_unresolved = 0
        if "__auto_type" in text:
            dump = dump_dir / (str(rel).replace(os.sep, "__") + ".original")
            functions = set(source_function_names(text))
            run_gcc_dump(root, src, dump)
            types = parse_dump(dump, functions)
            rewritten_text, file_unresolved = rewrite_source(rewritten_text, types, src)
        compounds = 0
        while True:
            rewritten_text, pass_compounds = rewrite_compound_literals(rewritten_text)
            compounds += pass_compounds
            if pass_compounds == 0:
                break
        rewritten_text, for_decls = rewrite_for_declarations(rewritten_text)
        dst.write_text(rewritten_text, encoding="utf-8")
        unresolved += file_unresolved
        if "__auto_type" in text or compounds > 0 or for_decls > 0:
            rewritten += 1

    listed = write_file_list(root, out_dir, file_list)
    write_plan9_include_wrappers(include_dir)
    rewrite_sqlite_for_plan9(root, sqlite_out)
    generate_embedded_assets(root, embedded_assets)
    print(
        f"prepared {out_dir} ({rewritten} rewritten C files, "
        f"{unresolved} unresolved inactive declarations, {listed} files listed)"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--generated", default="build/kryon/generated")
    parser.add_argument("--out", default="build/plan9/generated")
    parser.add_argument("--dump-dir", default="build/plan9/gcc-dumps")
    parser.add_argument("--file-list", default="build/plan9/generated-c-files.txt")
    parser.add_argument("--include-dir", default="build/plan9/include")
    parser.add_argument("--embedded-assets", default="build/plan9/inbe_embedded_assets.c")
    parser.add_argument("--sqlite-out", default="build/plan9/sqlite3_plan9.c")
    args = parser.parse_args()
    root = Path(args.root).resolve()
    return prepare(
        root,
        root / args.generated,
        root / args.out,
        root / args.dump_dir,
        root / args.file_list,
        root / args.include_dir,
        root / args.embedded_assets,
        root / args.sqlite_out,
    )


if __name__ == "__main__":
    raise SystemExit(main())
