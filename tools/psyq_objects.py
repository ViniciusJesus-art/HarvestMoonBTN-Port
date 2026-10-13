"""Convert the PSY-Q library objects the EXE links into ELF objects for GNU ld.

The game's EXE links 371 objects of Sony's PSY-Q Runtime Library 4.7: the startup
``2MBYTE.OBJ`` and members of its ``.LIB`` archives. Nothing of the SDK is in this
repository; this tool reads the release (``/opt/psyq/sdk-4.7`` in the build image)
and writes one ELF relocatable per object, exactly as PSYLINK linked it:

- ``psyq_lnk.py`` reads Sony's LIB/LNK formats, ``psyq_elf.py`` writes the ELF;
- a word the game's copy of an object differs in is written over it first
  (``sdk/patches.txt``, each with its reason);
- a common symbol is allocated by PSYLINK's rule, in the ``.bss`` of the first
  object in link order that declares it (``psyq_elf.linked_storage``). That is why
  every object is converted in one run, in link order.

    psyq_objects.py --sdk /opt/psyq/sdk-4.7 --objects sdk/objects.txt \\
                    --patches sdk/patches.txt --out BUILD/src/main/sdk

``sdk/objects.txt``: one object per line, in link order,
``<object path> <file> <member> <index>`` (the member's position in its archive;
names repeat inside a LIB). ``sdk/patches.txt``: ``<file> <member> <section>
<offset> <archived word> <linked word>``. ``#`` starts a comment in both.

Standard library only.
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

from psyq_elf import linked_storage, to_elf
from psyq_lnk import read_library_file


@dataclass(frozen=True)
class LinkedObject:
    output: str     # where the converted object goes, relative to --out
    file: str       # 2MBYTE.OBJ, LIBGPU.LIB, ...
    member: str
    index: int


@dataclass(frozen=True)
class Patch:
    file: str
    member: str
    section: str
    offset: int
    archived: int
    linked: int


def _rows(path: Path) -> list[list[str]]:
    rows = []
    for line in path.read_text().splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            rows.append(line.split())
    return rows


def read_objects(path: Path) -> list[LinkedObject]:
    return [LinkedObject(out, file, member, int(index)) for out, file, member, index in _rows(path)]


def read_patches(path: Path) -> list[Patch]:
    return [Patch(file, member, section, int(offset, 0), int(archived, 0), int(linked, 0))
            for file, member, section, offset, archived, linked in _rows(path)]


def convert(sdk: Path, objects: list[LinkedObject], patches: list[Patch], out: Path) -> None:
    files = {p.name.upper(): p for p in sdk.rglob("*") if p.suffix.lower() in (".lib", ".obj")}
    archives: dict[str, list] = {}
    members = []
    for obj in objects:
        if obj.file not in files:
            sys.exit(f"psyq_objects.py: {obj.file} is not in {sdk}")
        if obj.file not in archives:
            archives[obj.file] = read_library_file(files[obj.file])
        member = archives[obj.file][obj.index]
        if member.name != obj.member:
            sys.exit(f"psyq_objects.py: {obj.file} member #{obj.index} is {member.name}, "
                     f"{obj.output} expects {obj.member}")
        members.append(member.obj)

    owner: dict[str, int] = {}
    for i, lnk in enumerate(members):
        for sym in lnk.symbols:
            if sym.kind == "common":
                owner.setdefault(sym.name, i)

    for i, (obj, lnk) in enumerate(zip(objects, members)):
        for patch in patches:
            if (patch.file, patch.member) == (obj.file, obj.member):
                data = lnk.section_named(patch.section).data
                word = struct.unpack_from("<I", data, patch.offset)[0]
                if word != patch.archived:
                    sys.exit(f"psyq_objects.py: {obj.file}:{obj.member} {patch.section}"
                             f"+{patch.offset:#x} is {word:#x}, the patch expects "
                             f"{patch.archived:#x}")
                struct.pack_into("<I", data, patch.offset, patch.linked)
        sections = {sec.name: sec.size for sec in lnk.sections.values() if sec.size}
        commons = {s.name: s.size for s in lnk.symbols if s.kind == "common"}
        owned = [name for name in commons if owner[name] == i]
        path = out / obj.output
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(to_elf(lnk, linked_storage(sections, commons, owned)))


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--sdk", type=Path, required=True, help="the PSY-Q 4.7 release directory")
    ap.add_argument("--objects", type=Path, required=True)
    ap.add_argument("--patches", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args(argv)
    convert(args.sdk, read_objects(args.objects), read_patches(args.patches), args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
