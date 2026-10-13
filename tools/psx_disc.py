"""Read and write the game's CD image.

The disc is one MODE2/2352 data track: raw 2352-byte sectors holding an ISO 9660
file system with the CD-XA extensions. Two commands:

    psx_disc.py extract DISC.bin --out DIR
        Every file of the disc's file system into DIR, at its path on the disc, and
        the licence sectors as DIR/license_data.dat.

    psx_disc.py build LAYOUT.xml --files DIR [--replace NAME=PATH]... -o OUT.bin [--cue OUT.cue]
        The image LAYOUT.xml describes (an mkpsxiso <iso_project>), its files read
        from DIR, or from PATH for each --replace'd disc path.

Standard library only.
"""

from __future__ import annotations

import argparse
import struct
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path

SECTOR = 2352               # a raw sector
USER = 2048                 # a Form 1 sector's user data
RAW = SECTOR - 16           # a sector without its sync and header: what XA files store
SYNC = b"\x00" + b"\xff" * 10 + b"\x00"
PREGAP = 150                # the 2 s before LBA 0 (MSF 00:02:00)
LICENSE_SECTORS = 12        # sectors 0-11: the licence data; 12-15 are empty Form 2
PVD_LBA = 16

# Sub-header sub-modes (CD-XA).
SM_EOR, SM_DATA, SM_FORM2, SM_EOF = 0x01, 0x08, 0x20, 0x80
DATA, DATA_LAST = SM_DATA, SM_EOF | SM_DATA | SM_EOR

# XA attributes of a directory record (big-endian u16).
XA_FORM1, XA_FORM2, XA_INTERLEAVED, XA_DIRECTORY = 0x0800, 0x1000, 0x2000, 0x8000


# -- sectors: header, EDC, ECC ------------------------------------------------------
#
# EDC and ECC are computed for every Form 1 sector of the image at once: column k is
# byte k of every sector, and each step of the per-sector algorithms becomes one
# operation on whole columns (an XOR of two columns, a table lookup as
# bytes.translate). Byte for byte it is the textbook algorithm.


def _edc_tables() -> list[bytes]:
    """The EDC's CRC table (polynomial 0xD8018001, reflected), split into its four
    little-endian bytes: tables[j][i] is byte j of entry i."""
    entries = []
    for i in range(256):
        crc = i
        for _ in range(8):
            crc = (crc >> 1) ^ (0xD8018001 if crc & 1 else 0)
        entries.append(crc)
    return [bytes((entry >> 8 * j) & 0xFF for entry in entries) for j in range(4)]


_EDC = _edc_tables()


def _gf_tables() -> tuple[bytes, bytes]:
    """GF(2^8) (x^8+x^4+x^3+x^2+1): multiplication by 2, and division by 3."""
    times2, divide3 = bytearray(256), bytearray(256)
    for i in range(256):
        j = (i << 1) ^ (0x11D if i & 0x80 else 0)
        times2[i] = j
        divide3[i ^ j] = i
    return bytes(times2), bytes(divide3)


_TIMES2, _DIVIDE3 = _gf_tables()


def _power_tables(count: int) -> list[bytes]:
    """tables[k] multiplies by 2^k."""
    tables = [bytes(range(256))]
    for _ in range(count):
        tables.append(tables[-1].translate(_TIMES2))
    return tables


_POWERS = _power_tables(43)


def _xor(a: bytes, b: bytes) -> bytes:
    return (int.from_bytes(a, "little") ^ int.from_bytes(b, "little")).to_bytes(len(a), "little")


def _parity(word: list[bytes]) -> tuple[bytes, bytes]:
    """The two parity bytes of one Reed-Solomon code word of the product code (ECMA-130
    Annex A), given its bytes in order."""
    count = len(word)
    a = b = bytes(len(word[0]))
    for k, byte in enumerate(word):
        a = _xor(a, byte.translate(_POWERS[count - k]))
        b = _xor(b, byte)
    first = _xor(a.translate(_TIMES2), b).translate(_DIVIDE3)
    return first, _xor(first, b)


def seal(sectors: bytearray) -> None:
    """Fill the EDC (bytes 2072..2075) and the P and Q parity (2076..2351) of
    concatenated Form 1 sectors."""
    zero = bytes(len(sectors) // SECTOR)
    # The ECC's input, by position: the sector from byte 12, its header counted as
    # zero (Mode 2), then (below) the EDC and P.
    body = [zero] * 4 + [bytes(sectors[offset::SECTOR]) for offset in range(16, 2072)]

    # EDC: the CRC of the sub-header and the data (bytes 16..2071), initial value 0.
    crc = [zero] * 4
    for column in body[4:]:
        index = _xor(crc[0], column)
        crc = [_xor(crc[1], index.translate(_EDC[0])), _xor(crc[2], index.translate(_EDC[1])),
               _xor(crc[3], index.translate(_EDC[2])), index.translate(_EDC[3])]
    body += crc

    # P: 86 words of 24 bytes, the columns of the 24 x 86 matrix the 2064 bytes make.
    p = [_parity([body[word + 86 * k] for k in range(24)]) for word in range(86)]
    body += [first for first, _ in p] + [second for _, second in p]

    # Q: 52 words of 43 bytes along the matrix's diagonals, P included.
    q = [_parity([body[((word >> 1) * 86 + (word & 1) + 88 * k) % 2236] for k in range(43)])
         for word in range(52)]
    for offset, column in enumerate(body[2060:] + [first for first, _ in q]
                                    + [second for _, second in q], start=2072):
        sectors[offset::SECTOR] = column


def _bcd(value: int) -> int:
    return (value // 10) << 4 | value % 10


def header(lba: int) -> bytes:
    """Sync and header: the sector's MSF address in BCD, mode 2."""
    address = lba + PREGAP
    minute, second, frame = address // 4500, address // 75 % 60, address % 75
    return SYNC + bytes((_bcd(minute), _bcd(second), _bcd(frame), 2))


class Image:
    """The image as it is written: its sectors in order, and which are Form 1 (their EDC
    and ECC are filled at the end, all at once: `seal`)."""

    def __init__(self) -> None:
        self.data = bytearray()
        self.form1: list[int] = []

    @property
    def lba(self) -> int:
        return len(self.data) // SECTOR

    def form1_sector(self, data: bytes, submode: int) -> None:
        """2048 bytes of data (zero-padded) in a Form 1 sector."""
        if len(data) > USER:
            raise ValueError("a Form 1 sector holds 2048 bytes")
        self.form1.append(self.lba)
        self.data += header(self.lba) + bytes((0, 0, submode, 0)) * 2 + data.ljust(SECTOR - 24, b"\0")

    def empty_form2_sector(self, submode: int) -> None:
        """A Form 2 sector of zeros; its EDC is left zero, as everywhere on the disc."""
        self.data += header(self.lba) + bytes((0, 0, submode, 0)) * 2 + bytes(RAW - 8)

    def raw_sector(self, data: bytes) -> None:
        """A sector stored without its sync and header (an XA file's, the licence's):
        written as it is."""
        if len(data) != RAW:
            raise ValueError(f"a raw sector is {RAW} bytes")
        self.data += header(self.lba) + data

    def seal(self) -> None:
        """The EDC and ECC of every Form 1 sector."""
        sectors = bytearray(b"".join(self.data[n * SECTOR:(n + 1) * SECTOR] for n in self.form1))
        seal(sectors)
        for i, n in enumerate(self.form1):
            self.data[n * SECTOR:(n + 1) * SECTOR] = sectors[i * SECTOR:(i + 1) * SECTOR]


# -- the layout ----------------------------------------------------------------------


@dataclass
class Date:
    """An ISO 9660 recording date: year since 1900, month .. second, GMT offset in
    15-minute steps."""
    year: int
    month: int
    day: int
    hour: int
    minute: int
    second: int
    gmt: int

    @classmethod
    def parse(cls, text: str, gmt: int) -> "Date":
        """``YYYYMMDDhhmmss``, as mkpsxiso's date= attributes write it."""
        y, mo, d, h, mi, s = (int(text[i:j]) for i, j in
                              ((0, 4), (4, 6), (6, 8), (8, 10), (10, 12), (12, 14)))
        return cls(y - 1900, mo, d, h, mi, s, gmt)

    def record(self) -> bytes:
        return struct.pack("<6Bb", self.year, self.month, self.day, self.hour, self.minute,
                           self.second, self.gmt)


@dataclass
class Entry:
    """A file, a directory or a run of dummy sectors, in disc order."""
    kind: str                       # "data", "mixed" (XA), "dir", "dummy"
    name: str = ""
    path: str = ""                  # on the disc, e.g. STR/01.XA
    date: Date | None = None
    sectors: int = 0
    size: int = 0                   # the directory record's data length
    lba: int = 0
    children: list["Entry"] = field(default_factory=list)
    parent: "Entry | None" = None


@dataclass
class Layout:
    identifiers: dict[str, str]
    license: str
    xa_attrib: int
    xa_perm: int
    root: Entry


def load_layout(path: Path) -> Layout:
    """Read the subset of mkpsxiso's <iso_project> the disc uses."""
    track = ET.parse(path).getroot().find("track")
    defaults = track.find("default_attributes").attrib
    gmt = int(defaults["gmt_offs"])
    tree = track.find("directory_tree")
    root = Entry("dir", date=Date.parse(tree.get("date"), int(tree.get("gmt_offs", gmt))))

    def walk(element: ET.Element, parent: Entry) -> None:
        for child in element:
            if child.tag == "dummy":
                parent.children.append(Entry("dummy", sectors=int(child.get("sectors")),
                                             parent=parent))
                continue
            name = child.get("name")
            path = f"{parent.path}/{name}" if parent.path else name
            date = Date.parse(child.get("date"), int(child.get("gmt_offs", gmt)))
            kind = "dir" if child.tag == "dir" else child.get("type", "data")
            if kind not in ("data", "mixed", "dir"):
                raise SystemExit(f"{path}: type {kind!r} is not supported")
            entry = Entry(kind, name=name, path=path, date=date, parent=parent)
            parent.children.append(entry)
            if kind == "dir":
                walk(child, entry)

    walk(tree, root)
    return Layout(identifiers=dict(track.find("identifiers").attrib),
                  license=track.find("license").get("file"),
                  xa_attrib=int(defaults["xa_attrib"]), xa_perm=int(defaults["xa_perm"]),
                  root=root)


def _entries(directory: Entry):
    for child in directory.children:
        yield child
        if child.kind == "dir":
            yield from _entries(child)


def _directories(root: Entry) -> list[Entry]:
    """Every directory in path-table order: by depth, then parent, then name."""
    ordered, level = [root], [root]
    while level:
        level = [c for d in level for c in sorted((c for c in d.children if c.kind == "dir"),
                                                  key=_sort_key)]
        ordered += level
    return ordered


def _identifier(entry: Entry) -> bytes:
    return (entry.name if entry.kind == "dir" else entry.name + ";1").encode("ascii")


def _sort_key(entry: Entry) -> tuple[bytes, bytes]:
    name, _, extension = _identifier(entry).partition(b".")
    return name, extension


# -- ISO 9660 structures --------------------------------------------------------------


def _both16(value: int) -> bytes:
    return struct.pack("<H", value) + struct.pack(">H", value)


def _both32(value: int) -> bytes:
    return struct.pack("<I", value) + struct.pack(">I", value)


def _xa_attribute(layout: Layout, entry: Entry) -> int:
    if entry.kind == "dir":
        return XA_DIRECTORY | XA_FORM1 | layout.xa_perm
    if entry.kind == "mixed":
        return layout.xa_attrib << 8 | layout.xa_perm
    return XA_FORM1 | layout.xa_perm


def directory_record(layout: Layout, entry: Entry, identifier: bytes, date: Date,
                     xa: bool = True) -> bytes:
    """A directory record; ``xa``: with its CD-XA system-use field (14 bytes), as every
    record in a directory has (the volume descriptor's root record has none)."""
    body = (bytes(1) + _both32(entry.lba) + _both32(entry.size) + date.record()
            + bytes((2 if entry.kind == "dir" else 0, 0, 0)) + _both16(1)
            + bytes((len(identifier),)) + identifier)
    if len(identifier) % 2 == 0:
        body += b"\x00"
    if xa:
        body += (struct.pack(">HHH", 0, 0, _xa_attribute(layout, entry)) + b"XA"
                 + bytes((1 if entry.kind == "mixed" else 0,)) + bytes(5))
    return bytes((len(body) + 1,)) + body


def directory_sectors(layout: Layout, directory: Entry) -> list[bytes]:
    """A directory's records ('.', '..', then its entries by name), packed so that no
    record crosses a sector, as 2048-byte sectors."""
    parent = directory.parent or directory
    records = [directory_record(layout, directory, b"\x00", directory.date),
               directory_record(layout, parent, b"\x01", parent.date)]
    records += [directory_record(layout, child, _identifier(child), child.date)
                for child in sorted((c for c in directory.children if c.kind != "dummy"),
                                    key=_sort_key)]
    sectors, current = [], b""
    for record in records:
        if len(current) + len(record) > USER:
            sectors.append(current)
            current = b""
        current += record
    return sectors + [current]


def path_table(directories: list[Entry], endian: str) -> bytes:
    table = b""
    for directory in directories:
        name = _identifier(directory) if directory.parent else b"\x00"
        parent = directories.index(directory.parent) + 1 if directory.parent else 1
        table += bytes((len(name), 0)) + struct.pack(f"{endian}IH", directory.lba, parent) + name
        if len(name) % 2:
            table += b"\x00"
    return table


def _text(value: str, size: int) -> bytes:
    return value.encode("ascii").ljust(size, b" ")


def _pvd_date(value: str) -> bytes:
    """``YYYYMMDDhhmmsscc+q`` (q: the GMT offset in 15-minute steps) as 16 digits and a
    signed byte."""
    digits, sign, offset = value[:16], value[16:17], value[17:]
    return digits.encode("ascii") + struct.pack("<b", int(offset) * (-1 if sign == "-" else 1))


def primary_volume_descriptor(layout: Layout, placement: Placement) -> bytes:
    ids = layout.identifiers
    tables = placement.table_lbas
    unset = b"0" * 16 + b"\x00"
    root = layout.root
    pvd = (b"\x01CD001\x01\x00" + _text(ids.get("system", ""), 32) + _text(ids.get("volume", ""), 32)
           + bytes(8) + _both32(placement.volume_sectors) + bytes(32) + _both16(1) + _both16(1)
           + _both16(USER) + _both32(placement.table_size)
           + struct.pack("<II", tables[0], tables[1]) + struct.pack(">II", tables[2], tables[3])
           + directory_record(layout, root, b"\x00", root.date, xa=False)
           + _text(ids.get("volume_set", ""), 128) + _text(ids.get("publisher", ""), 128)
           + _text(ids.get("data_preparer", ""), 128) + _text(ids.get("application", ""), 128)
           + _text(ids.get("copyright", ""), 37) + _text(ids.get("abstract_file", ""), 37)
           + _text(ids.get("bibliographic_file", ""), 37)
           + _pvd_date(ids["creation_date"]) + _pvd_date(ids["modification_date"])
           + unset + unset + b"\x01\x00")
    application_use = bytearray(512)
    application_use[141:149] = b"CD-XA001"
    return pvd + application_use


# -- extract -------------------------------------------------------------------------


def _user_data(image: bytes, lba: int) -> bytes:
    return image[lba * SECTOR + 24:lba * SECTOR + 24 + USER]


def _records(image: bytes, lba: int, size: int):
    for n in range((size + USER - 1) // USER):
        data, offset = _user_data(image, lba + n), 0
        while offset < USER and data[offset]:
            length = data[offset]
            yield data[offset:offset + length]
            offset += length


def extract(disc: Path, out: Path) -> None:
    image = disc.read_bytes()
    if len(image) % SECTOR:
        raise SystemExit(f"{disc}: not a MODE2/2352 image ({len(image)} bytes)")
    pvd = _user_data(image, PVD_LBA)
    if pvd[:6] != b"\x01CD001":
        raise SystemExit(f"{disc}: no ISO 9660 volume descriptor at sector {PVD_LBA}")
    out.mkdir(parents=True, exist_ok=True)
    (out / "license_data.dat").write_bytes(
        b"".join(image[n * SECTOR + 16:(n + 1) * SECTOR] for n in range(LICENSE_SECTORS)))

    def walk(lba: int, size: int, directory: Path) -> None:
        directory.mkdir(parents=True, exist_ok=True)
        for record in _records(image, lba, size):
            name = record[33:33 + record[32]]
            if name in (b"\x00", b"\x01"):
                continue
            (extent,), (length,) = struct.unpack_from("<I", record, 2), struct.unpack_from("<I", record, 10)
            name = name.decode("ascii").split(";")[0]
            if record[25] & 2:
                walk(extent, length, directory / name)
                continue
            system_use = record[33 + record[32] + (1 - record[32] % 2):]
            attribute = struct.unpack_from(">H", system_use, 4)[0] if system_use[6:8] == b"XA" else 0
            count = (length + USER - 1) // USER
            if attribute & (XA_FORM2 | XA_INTERLEAVED):
                data = b"".join(image[n * SECTOR + 16:(n + 1) * SECTOR]
                                for n in range(extent, extent + count))
            else:
                data = b"".join(_user_data(image, n) for n in range(extent, extent + count))[:length]
            (directory / name).write_bytes(data)

    root = pvd[156:190]
    walk(struct.unpack_from("<I", root, 2)[0], struct.unpack_from("<I", root, 10)[0], out)


# -- build ---------------------------------------------------------------------------


@dataclass
class Placement:
    directories: list[Entry]                # in path-table order
    table_size: int                         # bytes
    table_sectors: int                      # each of the four copies
    table_lbas: tuple[int, int, int, int]   # L, its copy, M, its copy
    volume_sectors: int


def size_files(layout: Layout, files: Path, replace: dict[str, Path]) -> dict[str, Path]:
    """Every file's sectors and directory-record length; returns where each is read from."""
    sources = {}
    for entry in _entries(layout.root):
        if entry.kind not in ("data", "mixed"):
            continue
        source = sources[entry.path] = replace.get(entry.path, files / entry.path)
        length = source.stat().st_size
        if entry.kind == "mixed":
            if length % RAW:
                raise SystemExit(f"{source}: an XA file is whole {RAW}-byte sectors")
            entry.sectors, entry.size = length // RAW, length // RAW * USER
        else:
            entry.sectors, entry.size = max(1, (length + USER - 1) // USER), length
    unknown = set(replace) - set(sources)
    if unknown:
        raise SystemExit(f"--replace names files the layout does not have: {sorted(unknown)}")
    return sources


def place(layout: Layout) -> Placement:
    """The path tables from LBA 18, then the root directory, then every entry in layout
    order."""
    directories = _directories(layout.root)
    table_size = len(path_table(directories, "<"))
    table_sectors = (table_size + USER - 1) // USER
    table_lbas = tuple(PVD_LBA + 2 + n * table_sectors for n in range(4))
    lba = PVD_LBA + 2 + 4 * table_sectors
    for entry in [layout.root, *_entries(layout.root)]:
        entry.lba = lba
        if entry.kind == "dir":
            entry.sectors = len(directory_sectors(layout, entry))
            entry.size = entry.sectors * USER
        lba += entry.sectors
    return Placement(directories, table_size, table_sectors, table_lbas, volume_sectors=lba)


def build(layout_path: Path, files: Path, replace: dict[str, Path], out: Path,
          cue: Path | None) -> None:
    layout = load_layout(layout_path)
    sources = size_files(layout, files, replace)
    placement = place(layout)
    license = (files / layout.license).read_bytes()
    if len(license) != LICENSE_SECTORS * RAW:
        raise SystemExit(f"{layout.license}: the licence is {LICENSE_SECTORS} sectors of {RAW} bytes")

    image = Image()
    for n in range(LICENSE_SECTORS):
        image.raw_sector(license[n * RAW:(n + 1) * RAW])
    while image.lba < PVD_LBA:
        image.empty_form2_sector(SM_FORM2)
    image.form1_sector(primary_volume_descriptor(layout, placement), SM_DATA | SM_EOR)
    image.form1_sector(b"\xffCD001\x01", DATA_LAST)
    for endian in "<<>>":
        write_data(image, path_table(placement.directories, endian), placement.table_sectors)
    write_directory(image, layout, layout.root)
    for entry in _entries(layout.root):
        if image.lba != entry.lba:
            raise AssertionError(f"{entry.path or 'dummy'}: placed at {entry.lba}, written at {image.lba}")
        if entry.kind == "dir":
            write_directory(image, layout, entry)
        elif entry.kind == "dummy":
            for _ in range(entry.sectors):
                image.raw_sector(bytes(RAW))
        elif entry.kind == "mixed":
            data = sources[entry.path].read_bytes()
            for n in range(entry.sectors):
                image.raw_sector(data[n * RAW:(n + 1) * RAW])
        else:
            write_data(image, sources[entry.path].read_bytes(), entry.sectors)
    image.seal()
    out.write_bytes(image.data)
    if cue:
        cue.write_text(f'FILE "{out.name}" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n')


def write_directory(image: Image, layout: Layout, directory: Entry) -> None:
    """A directory's sectors: the last one ends the record and the file."""
    sectors = directory_sectors(layout, directory)
    for n, data in enumerate(sectors):
        image.form1_sector(data, DATA_LAST if n == len(sectors) - 1 else DATA)


def write_data(image: Image, data: bytes, sectors: int) -> None:
    """A file's Form 1 sectors: the last one ends the record and the file."""
    for n in range(sectors):
        image.form1_sector(data[n * USER:(n + 1) * USER], DATA_LAST if n == sectors - 1 else DATA)


# -- the command line ----------------------------------------------------------------


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    commands = parser.add_subparsers(dest="command", required=True)
    p = commands.add_parser("extract", help="the disc's files and licence into a directory")
    p.add_argument("disc", type=Path)
    p.add_argument("--out", type=Path, required=True)
    p = commands.add_parser("build", help="an image from a layout and its files")
    p.add_argument("layout", type=Path)
    p.add_argument("--files", type=Path, required=True)
    p.add_argument("--replace", action="append", default=[], metavar="NAME=PATH")
    p.add_argument("-o", "--out", type=Path, required=True)
    p.add_argument("--cue", type=Path)
    args = parser.parse_args(argv)
    if args.command == "extract":
        extract(args.disc, args.out)
    else:
        replace = {}
        for item in args.replace:
            name, _, path = item.partition("=")
            replace[name] = Path(path)
        build(args.layout, args.files, replace, args.out, args.cue)
    return 0


if __name__ == "__main__":
    sys.exit(main())
