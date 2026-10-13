"""Convert a PSY-Q `LNK` object into an ELF32 relocatable for GNU ld.

The converted object is what the EXE links for each Sony library member.

Sections with bytes become ELF sections (`.text`, `.rodata` (PSY-Q's
`.rdata`), `.data`, `.sdata`); uninitialised storage (`.sbss`/`.bss`) becomes
`SHT_NOBITS` sections as the link takes them: the object's own bytes, then the
commons it owns (`linked_storage`, PSYLINK's allocation). A
common another object owns stays an undefined global of its name, which `ld`
resolves to the owner's definition.
"""

from __future__ import annotations

import struct
from collections import defaultdict
from dataclasses import dataclass, field
from typing import Callable

from psyq_lnk import (
    GPREL16,
    GPREL16_LE,
    HI16,
    LO16,
    REL26,
    REL32,
    LnkObject,
    Relocation,
    Section,
    section_bound_symbol,
)


class PsyqElfError(ValueError):
    """The object uses something the conversion cannot express."""


ELF_SECTION_NAME = {".text": ".text", ".rdata": ".rodata", ".data": ".data", ".sdata": ".sdata",
                    ".sbss": ".sbss", ".bss": ".bss"}
UNINITIALISED = (".bss", ".sbss")

SHT_PROGBITS, SHT_SYMTAB, SHT_STRTAB, SHT_REL, SHT_NOBITS = 1, 2, 3, 9, 8
SHF_WRITE, SHF_ALLOC, SHF_EXECINSTR, SHF_MIPS_GPREL = 0x1, 0x2, 0x4, 0x10000000
STB_LOCAL, STB_GLOBAL = 0, 1
STT_NOTYPE, STT_SECTION = 0, 3
SHN_UNDEF = 0

R_MIPS_32, R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16, R_MIPS_GPREL16 = 2, 4, 5, 6, 7
_ELF_RELOC = {REL32: R_MIPS_32, REL26: R_MIPS_26, HI16: R_MIPS_HI16, LO16: R_MIPS_LO16,
              GPREL16: R_MIPS_GPREL16, GPREL16_LE: R_MIPS_GPREL16}

_SECTION_FLAGS = {
    ".text": SHF_ALLOC | SHF_EXECINSTR,
    ".rodata": SHF_ALLOC,
    ".data": SHF_ALLOC | SHF_WRITE,
    ".sdata": SHF_ALLOC | SHF_WRITE | SHF_MIPS_GPREL,
    ".sbss": SHF_ALLOC | SHF_WRITE | SHF_MIPS_GPREL,
    ".bss": SHF_ALLOC | SHF_WRITE,
}


@dataclass
class _Sym:
    name: str
    value: int
    section: int            # ELF section index, or SHN_UNDEF
    bind: int
    type: int = STT_NOTYPE


@dataclass
class _OutSection:
    name: str
    data: bytearray
    relocs: list[tuple[int, int, int]] = field(default_factory=list)  # (offset, sym, type)
    nobits: int | None = None   # size of an uninitialised section (it has no bytes)


def _sext16(v: int) -> int:
    return v - 0x10000 if v & 0x8000 else v


def _place_addend(word: int, rtype: int, addend: int) -> int:
    """Write a REL-style in-place addend into the relocated field of `word`."""
    if rtype == R_MIPS_32:
        return addend & 0xFFFFFFFF
    if rtype == R_MIPS_26:
        if addend & 3:
            raise PsyqElfError(f"unaligned REL26 addend {addend:#x}")
        return (word & ~0x03FFFFFF) | ((addend >> 2) & 0x03FFFFFF)
    if rtype == R_MIPS_LO16 or rtype == R_MIPS_GPREL16:
        return (word & ~0xFFFF) | (addend & 0xFFFF)
    if rtype == R_MIPS_HI16:
        # AHL = (hi << 16) + sext(lo); with lo = addend & 0xFFFF the high half
        # must absorb the sign of lo so the pair sums back to `addend`.
        hi = ((addend - _sext16(addend & 0xFFFF)) >> 16) & 0xFFFF
        return (word & ~0xFFFF) | hi
    raise PsyqElfError(f"unhandled relocation type {rtype}")


@dataclass(frozen=True)
class Storage:
    """An uninitialised section as linked: its size (own bytes and owned commons) and
    the commons it allocates, by offset (`linked_storage`)."""

    size: int
    commons: tuple[tuple[str, int], ...] = ()


#: PSYLINK's symbol hash: the sum of the name's bytes modulo this prime. Its common
#: allocation walks the hash buckets in order (`linked_storage`).
PSYLINK_HASH_BUCKETS = 509

#: The largest alignment PSYLINK gives a common (a doubleword).
COMMON_MAX_ALIGN = 8


def psylink_hash(name: str) -> int:
    return sum(name.encode()) % PSYLINK_HASH_BUCKETS


def common_alignment(size: int) -> int:
    """A common's alignment: its size's largest power of two, at most a doubleword."""
    align = 1
    while align * 2 <= size and align < COMMON_MAX_ALIGN:
        align *= 2
    return align


def linked_storage(sections: dict[str, int], commons: dict[str, int],
                   owned: list[str]) -> dict[str, Storage]:
    """Each uninitialised section an object links (PSY-Q name -> `Storage`).

    ``sections`` are the object's section sizes, ``commons`` every common it declares
    (name -> size, in its symbol order) and ``owned`` those it owns: the commons for
    which it is the first object in link order to declare them. PSYLINK put a common
    at the tail of its owner's `.bss`, after the object's own `.bss` bytes, each on
    `common_alignment`, in the order of its symbol hash buckets (`psylink_hash`); two
    in one bucket go the later-declared first (a bucket is a list it prepends to).
   
    """
    out: dict[str, Storage] = {}
    for section in UNINITIALISED:
        if sections.get(section):
            out[section] = Storage(sections[section], ())
    if owned:
        declared = list(commons)
        ordered = sorted(owned, key=lambda n: (psylink_hash(n), -declared.index(n)))
        cursor = sections.get(".bss", 0)
        placed = []
        for name in ordered:
            size, align = commons[name], common_alignment(commons[name])
            cursor = (cursor + align - 1) // align * align
            placed.append((name, cursor))
            cursor += size
        out[".bss"] = Storage(cursor, tuple(placed))
    return out


def to_elf(obj: LnkObject, storage: dict[str, Storage] | None = None,
           e_flags: int = 0) -> bytes:
    """ELF32 little-endian MIPS relocatable equivalent of `obj`.

    `storage` gives each uninitialised section the object links (PSY-Q name ->
    `Storage`): its `SHT_NOBITS` size and the commons defined in it. A section with
    no entry links nothing (an empty `.bss`); a common not allocated here stays an
    undefined global.
    """
    storage = storage or {}
    out_sections: list[_OutSection] = []
    elf_index: dict[int, int] = {}          # LNK section id -> ELF section index
    for sec in sorted(obj.sections.values(), key=lambda s: s.index):
        if sec.name not in ELF_SECTION_NAME:
            raise PsyqElfError(f"{obj.name}: unsupported section {sec.name}")
        if sec.name in UNINITIALISED:
            linked = storage.get(sec.name)
            if linked is None:
                if sec.size:
                    raise PsyqElfError(f"{obj.name}: {sec.name} has {sec.size} bytes "
                                       "and no storage")
                continue
            if linked.size < sec.size:
                raise PsyqElfError(f"{obj.name}: {sec.name} is {sec.size} bytes, "
                                   f"its storage {linked.size}")
            out_sections.append(_OutSection(ELF_SECTION_NAME[sec.name], bytearray(),
                                            nobits=linked.size))
        elif not sec.data and not sec.relocations:
            continue
        else:
            out_sections.append(_OutSection(ELF_SECTION_NAME[sec.name], bytearray(sec.data)))
        elf_index[sec.index] = len(out_sections)  # 1-based: index 0 is SHN_UNDEF
    for name in storage:
        if obj.section_named(name) is None or obj.section_named(name).index not in elf_index:
            raise PsyqElfError(f"{obj.name}: storage for {name}, which the object does not "
                               "have")

    # symbols: null, one STT_SECTION per section, locals, then globals
    symbols: list[_Sym] = [_Sym("", 0, SHN_UNDEF, STB_LOCAL)]
    section_sym: dict[int, int] = {}
    for lnk_idx, e_idx in elf_index.items():
        section_sym[lnk_idx] = len(symbols)
        symbols.append(_Sym("", 0, e_idx, STB_LOCAL, STT_SECTION))
    by_lnk_id: dict[int, int] = {}
    for s in obj.symbols:
        if s.kind == "local" and s.section in elf_index:
            by_lnk_id[s.index] = len(symbols)
            symbols.append(_Sym(s.name, s.offset, elf_index[s.section], STB_LOCAL))
    first_global = len(symbols)
    globals_by_name: dict[str, int] = {}

    def global_symbol(name: str, value: int = 0, section: int = SHN_UNDEF) -> int:
        if name in globals_by_name:
            return globals_by_name[name]
        globals_by_name[name] = len(symbols)
        symbols.append(_Sym(name, value, section, STB_GLOBAL))
        return globals_by_name[name]

    allocated = {name: (section, offset) for section, linked in storage.items()
                 for name, offset in linked.commons}
    for s in obj.symbols:
        if s.kind == "export":
            if s.section not in elf_index:
                raise PsyqElfError(f"{obj.name}: export {s.name} in an empty section")
            by_lnk_id[s.index] = global_symbol(s.name, s.offset, elf_index[s.section])
        elif s.kind == "common" and s.name in allocated:
            section, offset = allocated.pop(s.name)
            by_lnk_id[s.index] = global_symbol(
                s.name, offset, elf_index[obj.section_named(section).index])
        elif s.kind in ("import", "common"):
            by_lnk_id[s.index] = global_symbol(s.name)
    if allocated:
        raise PsyqElfError(f"{obj.name}: storage allocates {sorted(allocated)}, "
                           "which the object does not declare")

    # relocations
    for lnk_idx, e_idx in elf_index.items():
        sec: Section = obj.sections[lnk_idx]
        out = out_sections[e_idx - 1]
        for rel in _ordered_for_ld(sec.relocations):
            rtype = _ELF_RELOC[rel.type]
            target = rel.expr.symbol_and_addend()
            if target is None:
                raise PsyqElfError(f"{obj.name}: {sec.name}+{rel.offset:#x}: expression too complex")
            kind, index, addend = target
            if kind in ("start", "end"):
                bound = obj.sections.get(index)
                if bound is None:
                    raise PsyqElfError(f"{obj.name}: bound of unknown section {index:#x}")
                sym = global_symbol(section_bound_symbol(kind, bound.name))
            elif kind == "sect":
                if index in section_sym:
                    sym = section_sym[index]
                else:
                    raise PsyqElfError(f"{obj.name}: reference to empty section {index:#x}")
            else:
                if index not in by_lnk_id:
                    raise PsyqElfError(f"{obj.name}: reference to unknown symbol {index}")
                sym = by_lnk_id[index]
            word = struct.unpack_from("<I", out.data, rel.offset)[0]
            struct.pack_into("<I", out.data, rel.offset, _place_addend(word, rtype, addend))
            out.relocs.append((rel.offset, sym, rtype))

    return _write_elf(out_sections, symbols, first_global, e_flags)


def _ordered_for_ld(relocs: list[Relocation]) -> list[Relocation]:
    """Order relocations so every HI16 is followed by a LO16 of the same target.

    GNU ld pairs a `R_MIPS_HI16` with the next `R_MIPS_LO16` against the same
    symbol in relocation order, and takes the carry from that LO16's addend.
    PSY-Q states each relocation with its full target, so grouping by target --
    all HI16s of a target, then its LO16s -- gives every HI16 a partner with
    the same addend.
    """
    his: dict[tuple, list[Relocation]] = defaultdict(list)
    los: dict[tuple, list[Relocation]] = defaultdict(list)
    rest: list[Relocation] = []
    for r in relocs:
        key = r.expr.symbol_and_addend()
        if r.type == HI16:
            his[key].append(r)
        elif r.type == LO16:
            los[key].append(r)
        else:
            rest.append(r)
    ordered: list[Relocation] = list(rest)
    for key, group in his.items():
        if key not in los:
            raise PsyqElfError(f"HI16 at {group[0].offset:#x} has no LO16 with the same target")
        ordered.extend(group)
        ordered.extend(los.pop(key))
    for group in los.values():
        ordered.extend(group)
    return ordered


def _write_elf(sections: list[_OutSection], symbols: list[_Sym], first_global: int,
               e_flags: int) -> bytes:
    shstr = bytearray(b"\0")
    strtab = bytearray(b"\0")

    def add(tab: bytearray, s: str) -> int:
        off = len(tab)
        tab.extend(s.encode() + b"\0")
        return off

    body = bytearray()
    headers: list[tuple] = []  # (name_off, type, flags, offset, size, link, info, align, entsize)

    def emit(data: bytes, align: int = 4) -> int:
        while (0x34 + len(body)) % align:
            body.append(0)
        off = 0x34 + len(body)
        body.extend(data)
        return off

    rel_count = sum(1 for s in sections if s.relocs)
    symtab_index = 1 + len(sections) + rel_count
    for sec in sections:
        if sec.nobits is not None:
            headers.append((add(shstr, sec.name), SHT_NOBITS, _SECTION_FLAGS[sec.name],
                            0x34 + len(body), sec.nobits, 0, 0, 8, 0))
            continue
        off = emit(bytes(sec.data))
        headers.append((add(shstr, sec.name), SHT_PROGBITS, _SECTION_FLAGS[sec.name],
                        off, len(sec.data), 0, 0, 4, 0))
    for i, sec in enumerate(sections, start=1):
        if not sec.relocs:
            continue
        raw = b"".join(struct.pack("<II", o, (s << 8) | t) for o, s, t in sec.relocs)
        off = emit(raw)
        headers.append((add(shstr, ".rel" + sec.name), SHT_REL, 0, off, len(raw),
                        symtab_index, i, 4, 8))
    sym_raw = b"".join(
        struct.pack("<IIIBBH", add(strtab, s.name) if s.name else 0, s.value, 0,
                    (s.bind << 4) | s.type, 0, s.section)
        for s in symbols)
    off = emit(sym_raw)
    headers.append((add(shstr, ".symtab"), SHT_SYMTAB, 0, off, len(sym_raw),
                    symtab_index + 1, first_global, 4, 16))
    name_strtab = add(shstr, ".strtab")
    name_shstrtab = add(shstr, ".shstrtab")
    off_str = emit(bytes(strtab), 1)
    headers.append((name_strtab, SHT_STRTAB, 0, off_str, len(strtab), 0, 0, 1, 0))
    off_shstr = emit(bytes(shstr), 1)
    headers.append((name_shstrtab, SHT_STRTAB, 0, off_shstr, len(shstr), 0, 0, 1, 0))

    shoff = emit(b"", 4)
    shdrs = struct.pack("<10I", *([0] * 10))
    for (name, typ, flags, o, size, link, info, align, entsize) in headers:
        shdrs += struct.pack("<10I", name, typ, flags, 0, o, size, link, info, align, entsize)
    ident = b"\x7fELF" + bytes([1, 1, 1, 0]) + bytes(8)
    ehdr = ident + struct.pack("<HHIIIIIHHHHHH", 1, 8, 1, 0, 0, shoff, e_flags,
                               0x34, 0, 0, 40, len(headers) + 1, len(headers))
    return ehdr + bytes(body) + shdrs
