"""PSY-Q object (`LNK` v2) and library (`LIB` v1) reader.

Sony's runtime libraries ship as `LIB` archives of `LNK` objects; the build
reads them to convert the library objects the EXE links.

This reader keeps what identification needs -- section bytes, relocation sites
and their expressions, and symbol definitions -- and skips the SLD / debug
records by their documented sizes. It does not write either format.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path


class PsyqFormatError(ValueError):
    """The bytes are not a well-formed PSY-Q LNK object or LIB archive."""


# ---------------------------------------------------------------- relocations

REL32 = 16
REL26 = 74
HI16 = 82
LO16 = 84
GPREL16 = 100
GPREL16_LE = 30

# Which bits of the 32-bit little-endian word a relocation type rewrites. Every
# other bit of that word is fixed by the object and must match the linked image.
RELOC_FIELD_MASK = {
    REL32: 0xFFFFFFFF,
    REL26: 0x03FFFFFF,
    HI16: 0x0000FFFF,
    LO16: 0x0000FFFF,
    GPREL16: 0x0000FFFF,
    GPREL16_LE: 0x0000FFFF,
}

# Expression opcodes.
EXPR_VALUE = 0
EXPR_SYMBOL = 2
EXPR_SECTION_BASE = 4
EXPR_SECTION_START = 12
EXPR_SECTION_END = 22
EXPR_ADD = 44
EXPR_SUB = 46
EXPR_DIV = 50
# 44..62 (even) are the binary operators; only ADD is interpreted. The others
# (SUB, DIV and the bitwise/shift forms that appear in libsn) are carried as
# opaque trees -- identification only needs to know which word they patch.
EXPR_BINARY = frozenset(range(44, 64, 2))


@dataclass(frozen=True)
class Expr:
    """A relocation expression tree node."""

    op: int
    value: int = 0          # EXPR_VALUE: the constant; SYMBOL/SECTION_*: the index
    left: "Expr | None" = None
    right: "Expr | None" = None

    def symbol_and_addend(self) -> tuple[str, int, int] | None:
        """Reduce the expression to (kind, index, addend).

        kind is "sym" (a symbol id), "sect" (a section's base in this object),
        "start" or "end" (the bound of the *linked* section of that name, which
        the linker provides -- the startup clears `.sbss` to `.bss` end with
        these). Handled shapes: `target`, `target + const`, `const + target`,
        `target - const`. Anything else returns None.
        """
        if self.op == EXPR_SYMBOL:
            return ("sym", self.value, 0)
        if self.op == EXPR_SECTION_BASE:
            return ("sect", self.value, 0)
        if self.op == EXPR_SECTION_START:
            return ("start", self.value, 0)
        if self.op == EXPR_SECTION_END:
            return ("end", self.value, 0)
        if self.left is None or self.right is None:
            return None
        if self.op == EXPR_ADD:
            for base, const in ((self.left, self.right), (self.right, self.left)):
                if const.op == EXPR_VALUE:
                    inner = base.symbol_and_addend()
                    if inner is not None:
                        return (inner[0], inner[1], inner[2] + const.value)
        if self.op == EXPR_SUB and self.right.op == EXPR_VALUE:  # left - right
            inner = self.left.symbol_and_addend()
            if inner is not None:
                return (inner[0], inner[1], inner[2] - self.right.value)
        return None


def section_bound_symbol(kind: str, section: str) -> str:
    """The symbol standing for a linked section's "start" or "end" (`__psyq_bss_end`)."""
    return f"__psyq_{section.lstrip('.')}_{kind}"


@dataclass(frozen=True)
class Relocation:
    type: int
    offset: int             # byte offset within the section's data
    expr: Expr

    @property
    def field_mask(self) -> int:
        return RELOC_FIELD_MASK[self.type]


@dataclass
class Section:
    index: int
    name: str
    group: int
    alignment: int
    data: bytearray = field(default_factory=bytearray)
    pending_zeroes: int = 0
    relocations: list[Relocation] = field(default_factory=list)
    uninitialized: int = 0  # bytes reserved by UNINITIALIZED symbols (.bss/.sbss)
    chunk_start: int = 0    # where the most recent BYTES run began

    @property
    def size(self) -> int:
        return len(self.data) + self.pending_zeroes + self.uninitialized


@dataclass(frozen=True)
class Symbol:
    """A symbol an object defines (exported or local) or needs (imported / common)."""

    index: int              # file-local symbol number; locals get negative numbers
    name: str
    kind: str               # "export" | "local" | "import" | "common"
    section: int = 0
    offset: int = 0
    size: int = 0


@dataclass
class LnkObject:
    name: str
    sections: dict[int, Section]
    symbols: list[Symbol]
    function_sizes: dict[str, int]

    def section_named(self, name: str) -> Section | None:
        for sec in self.sections.values():
            if sec.name == name:
                return sec
        return None

    def symbol_by_index(self, index: int) -> Symbol | None:
        for sym in self.symbols:
            if sym.index == index:
                return sym
        return None

    def defined_in(self, section_index: int) -> list[Symbol]:
        """Exported + local symbols defined in one section, by offset."""
        return sorted(
            (s for s in self.symbols
             if s.kind in ("export", "local") and s.section == section_index),
            key=lambda s: s.offset,
        )


# --------------------------------------------------------------------- reader


class _Cursor:
    def __init__(self, data: bytes, pos: int = 0, end: int | None = None):
        self.data = data
        self.pos = pos
        self.end = len(data) if end is None else end

    def take(self, fmt: str):
        size = struct.calcsize(fmt)
        if self.pos + size > self.end:
            raise PsyqFormatError(f"truncated at {self.pos:#x}")
        vals = struct.unpack_from(fmt, self.data, self.pos)
        self.pos += size
        return vals if len(vals) > 1 else vals[0]

    def u8(self) -> int:
        return self.take("<B")

    def u16(self) -> int:
        return self.take("<H")

    def u32(self) -> int:
        return self.take("<I")

    def raw(self, size: int) -> bytes:
        if self.pos + size > self.end:
            raise PsyqFormatError(f"truncated at {self.pos:#x}")
        out = self.data[self.pos:self.pos + size]
        self.pos += size
        return out

    def pstring(self) -> str:
        return self.raw(self.u8()).decode("latin-1")


def _read_expr(cur: _Cursor) -> Expr:
    op = cur.u8()
    if op == EXPR_VALUE:
        return Expr(op, cur.u32())
    if op in (EXPR_SYMBOL, EXPR_SECTION_BASE, EXPR_SECTION_START, EXPR_SECTION_END):
        return Expr(op, cur.u16())
    if op in EXPR_BINARY:
        right = _read_expr(cur)
        left = _read_expr(cur)
        return Expr(op, 0, left, right)
    raise PsyqFormatError(f"unknown expression opcode {op} at {cur.pos - 1:#x}")


# Debug-only records: opcode -> fixed payload layout (struct format) and the
# number of trailing Pascal strings. Identification ignores them.
_SKIP_FIXED = {
    50: ("<H", 0),          # INC_SLD_LINENUM
    52: ("<HB", 0),         # INC_SLD_LINENUM_BY_BYTE
    54: ("<HH", 0),         # INC_SLD_LINENUM_BY_WORD
    56: ("<HI", 0),         # SET_SLD_LINENUM
    58: ("<HIH", 0),        # SET_SLD_LINENUM_FILE
    60: ("<H", 0),          # END_SLD
    76: ("<HII", 0),        # FUNCTION_END
    78: ("<HII", 0),        # BLOCK_START
    80: ("<HII", 0),        # BLOCK_END
    82: ("<HIHHI", 1),      # SECTION_DEF
}


def parse_lnk(data: bytes, name: str = "", start: int = 0, end: int | None = None) -> LnkObject:
    """Parse one `LNK` v2 object from `data[start:end]`."""
    cur = _Cursor(data, start, end)
    if cur.raw(3) != b"LNK":
        raise PsyqFormatError(f"{name}: missing LNK signature")
    version = cur.u8()
    if version != 2:
        raise PsyqFormatError(f"{name}: unsupported LNK version {version}")

    sections: dict[int, Section] = {}
    symbols: list[Symbol] = []
    function_sizes: dict[str, int] = {}
    current: Section | None = None
    local_index = 0
    fn_name, fn_start = "", 0

    while True:
        op = cur.u8()
        if op == 0:  # END
            break
        if op == 2:  # BYTES
            size = cur.u16()
            if current is None:
                raise PsyqFormatError(f"{name}: BYTES before any SWITCH")
            current.chunk_start = len(current.data) + current.pending_zeroes
            if current.pending_zeroes:
                current.data.extend(bytes(current.pending_zeroes))
                current.pending_zeroes = 0
            current.data.extend(cur.raw(size))
        elif op == 6:  # SWITCH
            idx = cur.u16()
            current = sections.get(idx)
            if current is None:
                raise PsyqFormatError(f"{name}: SWITCH to undeclared section {idx:#x}")
        elif op == 8:  # ZEROES
            size = cur.u32()
            if current is None:
                raise PsyqFormatError(f"{name}: ZEROES before any SWITCH")
            current.pending_zeroes += size
        elif op == 10:  # RELOCATION
            rtype = cur.u8()
            if rtype not in RELOC_FIELD_MASK:
                raise PsyqFormatError(f"{name}: unsupported relocation type {rtype}")
            offset = cur.u16()
            expr = _read_expr(cur)
            if current is None:
                raise PsyqFormatError(f"{name}: RELOCATION before any SWITCH")
            # The offset is relative to the start of the most recent BYTES run.
            current.relocations.append(Relocation(rtype, current.chunk_start + offset, expr))
        elif op == 12:  # EXPORTED_SYMBOL
            idx, sect, off = cur.take("<HHI")
            symbols.append(Symbol(idx, cur.pstring(), "export", sect, off))
        elif op == 14:  # IMPORTED_SYMBOL
            idx = cur.u16()
            symbols.append(Symbol(idx, cur.pstring(), "import"))
        elif op == 16:  # SECTION
            idx, group, align = cur.take("<HHB")
            sections[idx] = Section(idx, cur.pstring(), group, align)
        elif op == 18:  # LOCAL_SYMBOL
            sect, off = cur.take("<HI")
            local_index -= 1
            symbols.append(Symbol(local_index, cur.pstring(), "local", sect, off))
        elif op == 28:  # FILENAME
            cur.u16()
            cur.pstring()
        elif op == 46:  # PROGRAMTYPE
            cur.u8()
        elif op == 48:  # UNINITIALIZED
            idx, sect, size = cur.take("<HHI")
            sym_name = cur.pstring()
            symbols.append(Symbol(idx, sym_name, "common", sect, 0, size))
        elif op in (74, 86):  # FUNCTION / FUNCTION_START2
            sect, off = cur.take("<HI")
            cur.take("<HIHIHII")
            if op == 86:
                cur.take("<II")
            fn_name, fn_start = cur.pstring(), off
        elif op == 76:  # FUNCTION_END (sized explicitly: it closes the function)
            _sect, off, _line = cur.take("<HII")
            function_sizes[fn_name] = off - fn_start
        elif op == 84:  # SECTION_DEF2
            cur.take("<HIHHI")
            dims = cur.u16()
            cur.take("<" + "H" * dims) if dims else None
            cur.pstring()
            cur.pstring()
        elif op in _SKIP_FIXED:
            fmt, strings = _SKIP_FIXED[op]
            cur.take(fmt)
            for _ in range(strings):
                cur.pstring()
        else:
            raise PsyqFormatError(f"{name}: unknown opcode {op} at {cur.pos - 1:#x}")

    for sec in sections.values():
        sec.data.extend(bytes(sec.pending_zeroes))
        sec.pending_zeroes = 0
    return LnkObject(name, sections, symbols, function_sizes)


@dataclass(frozen=True)
class LibMember:
    """One object of a library. Member names are not unique within a LIB
    (`LIBGTE.LIB` has three `SMP_00_1`), so `index` -- the member's position
    in the archive -- is part of its identity."""

    index: int
    name: str               # module name (v1: 8 chars, spaces stripped; v2: file stem, upper-cased)
    date: int               # raw DOS-style timestamp word
    exports: tuple[str, ...]
    obj: LnkObject


def parse_lib(data: bytes, name: str = "") -> list[LibMember]:
    """Parse a `LIB` archive (version 1 or 2) into its member objects."""
    if data[:3] != b"LIB":
        raise PsyqFormatError(f"{name}: missing LIB signature")
    if data[3] == 1:
        return _parse_lib_v1(data, name)
    if data[3] == 2:
        return _parse_lib_v2(data, name)
    raise PsyqFormatError(f"{name}: unsupported LIB version {data[3]}")


def _parse_lib_v1(data: bytes, name: str) -> list[LibMember]:
    """Members back to back, each with its own header and export list."""
    members: list[LibMember] = []
    pos = 4
    while pos < len(data):
        cur = _Cursor(data, pos)
        mod = cur.raw(8).decode("latin-1").rstrip(" \0")
        date, header_size, total_size = cur.take("<III")
        exports: list[str] = []
        while True:
            n = cur.u8()
            if n == 0:
                break
            exports.append(cur.raw(n).decode("latin-1"))
        obj_start = pos + header_size
        obj_end = pos + total_size
        obj = parse_lnk(data, f"{name}:{mod}", obj_start, obj_end)
        members.append(LibMember(len(members), mod, date, tuple(exports), obj))
        pos = obj_end
    return members


def _parse_lib_v2(data: bytes, name: str) -> list[LibMember]:
    """Objects back to back from offset 12, indexed by a trailing directory."""
    dir_offset, dir_size = struct.unpack_from("<II", data, 4)
    cur = _Cursor(data, dir_offset, dir_offset + dir_size)
    members: list[LibMember] = []
    while cur.pos < cur.end:
        obj_offset, obj_size, date = cur.take("<III")
        filename = cur.pstring()
        exports: list[str] = []
        while cur.u16() != 0:
            cur.u16()  # per-export word of unknown meaning
            exports.append(cur.pstring())
        mod = filename.rsplit(".", 1)[0].upper()
        obj = parse_lnk(data, f"{name}:{mod}", obj_offset, obj_offset + obj_size)
        members.append(LibMember(len(members), mod, date, tuple(exports), obj))
    return members


def read_library_file(path: Path) -> list[LibMember]:
    """Read a `.LIB` archive, or a lone `.OBJ` as a one-member library."""
    data = path.read_bytes()
    if data[:3] == b"LIB":
        return parse_lib(data, path.name)
    if data[:3] == b"LNK":
        return [LibMember(0, path.stem.upper(), 0, (), parse_lnk(data, path.name))]
    raise PsyqFormatError(f"{path}: neither a LIB archive nor an LNK object")
