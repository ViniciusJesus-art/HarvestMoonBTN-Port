"""PS-X EXE header: parse it, and write it from a linked image.

The 2048-byte PlayStation 1 executable header, written after the EXE link the
way PSY-Q's CPE2X did (`write_exe`).

Standard library only; the build runs it as a post-link step:
``python3 ps_x_exe.py ELF PAYLOAD -o EXE --region "North America" --stack 0x801FFFF0``.
"""

from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path

MAGIC = b"PS-X EXE"
HEADER_SIZE = 0x800
SECTOR = 0x800

#: Where the region marker starts and how long its field is (to the header end).
MARKER_OFFSET = 0x4C
_MARKER_FIELD = HEADER_SIZE - MARKER_OFFSET


class PsxExeError(ValueError):
    """The bytes are not a valid PS-X EXE, or cannot be made into one."""


def region_marker(region: str) -> str:
    """The licence text CPE2X writes for a region (`/ca` North America, `/ce`
    Europe, `/cj` Japan)."""
    return f"Sony Computer Entertainment Inc. for {region} area"


@dataclass(frozen=True)
class PsxExeHeader:
    """The header's fields. The four `data_*`/`bss_*` words and
    `sp_size` are zero in every EXE CPE2X wrote for this game."""

    pc0: int
    gp0: int
    text_addr: int
    text_size: int
    sp_addr: int
    data_addr: int = 0
    data_size: int = 0
    bss_addr: int = 0
    bss_size: int = 0
    sp_size: int = 0
    marker: str = ""

    @classmethod
    def parse(cls, data: bytes) -> "PsxExeHeader":
        if len(data) < HEADER_SIZE:
            raise PsxExeError(f"too short for a PS-X EXE header ({len(data)} bytes)")
        if data[:8] != MAGIC:
            raise PsxExeError(f"bad magic: {data[:8]!r}")
        (pc0, gp0, text_addr, text_size, data_addr, data_size, bss_addr, bss_size,
         sp_addr, sp_size) = struct.unpack_from("<10I", data, 0x10)
        marker = data[MARKER_OFFSET:HEADER_SIZE].split(b"\0", 1)[0].decode("ascii")
        return cls(pc0=pc0, gp0=gp0, text_addr=text_addr, text_size=text_size,
                   sp_addr=sp_addr, data_addr=data_addr, data_size=data_size,
                   bss_addr=bss_addr, bss_size=bss_size, sp_size=sp_size,
                   marker=marker)

    def to_bytes(self) -> bytes:
        """The 2048 header bytes: every byte not named is zero."""
        marker = self.marker.encode("ascii")
        if len(marker) >= _MARKER_FIELD:
            raise PsxExeError(f"region marker of {len(marker)} bytes does not fit")
        out = bytearray(HEADER_SIZE)
        out[:8] = MAGIC
        struct.pack_into("<10I", out, 0x10, self.pc0, self.gp0, self.text_addr,
                         self.text_size, self.data_addr, self.data_size,
                         self.bss_addr, self.bss_size, self.sp_addr, self.sp_size)
        out[MARKER_OFFSET:MARKER_OFFSET + len(marker)] = marker
        return bytes(out)

    def vram_to_offset(self, vram: int) -> int:
        """File offset of a virtual address within the loaded text image."""
        if not (self.text_addr <= vram < self.text_addr + self.text_size):
            raise PsxExeError(f"vram {vram:#x} outside text {self.text_addr:#x}+{self.text_size:#x}")
        return HEADER_SIZE + (vram - self.text_addr)


@dataclass(frozen=True)
class LinkedImage:
    """What the header needs from the linked ELF: the entry point
    and the address the payload loads to."""

    entry: int
    load_address: int

    @classmethod
    def from_elf(cls, elf: bytes) -> "LinkedImage":
        """Read a 32-bit little-endian ELF's entry (`e_entry`) and the lowest
        virtual address of a `PT_LOAD` segment with bytes in the file."""
        if elf[:4] != b"\x7fELF" or elf[4] != 1 or elf[5] != 1:
            raise PsxExeError("not a 32-bit little-endian ELF")
        entry, phoff = struct.unpack_from("<II", elf, 0x18)
        phentsize, phnum = struct.unpack_from("<HH", elf, 0x2A)
        loads = []
        for i in range(phnum):
            p_type, _offset, vaddr, _paddr, filesz = struct.unpack_from(
                "<5I", elf, phoff + i * phentsize)
            if p_type == 1 and filesz:  # PT_LOAD
                loads.append(vaddr)
        if not loads:
            raise PsxExeError("the ELF loads nothing")
        return cls(entry=entry, load_address=min(loads))


def header_for(image: LinkedImage, payload_size: int, *, region: str,
               stack: int) -> PsxExeHeader:
    """The header CPE2X writes for a linked image whose payload is
    ``payload_size`` bytes."""
    if payload_size % SECTOR:
        raise PsxExeError(f"payload of {payload_size:#x} bytes is not whole sectors")
    return PsxExeHeader(pc0=image.entry, gp0=0, text_addr=image.load_address,
                        text_size=payload_size, sp_addr=stack,
                        marker=region_marker(region))


def write_exe(elf: Path, payload: Path, out: Path, *, region: str,
              stack: int) -> PsxExeHeader:
    """Write ``out``: the header for ``elf``, then ``payload`` (`objcopy -O
    binary` of the same ELF)."""
    body = payload.read_bytes()
    header = header_for(LinkedImage.from_elf(elf.read_bytes()), len(body),
                        region=region, stack=stack)
    out.write_bytes(header.to_bytes() + body)
    return header


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Write a PS-X EXE from a linked ELF and its "
                                             "objcopy -O binary payload.")
    ap.add_argument("elf", type=Path)
    ap.add_argument("payload", type=Path)
    ap.add_argument("-o", "--output", type=Path, required=True)
    ap.add_argument("--region", required=True, help='e.g. "North America"')
    ap.add_argument("--stack", type=lambda s: int(s, 0), required=True,
                    help="initial $sp, e.g. 0x801FFFF0")
    args = ap.parse_args(argv)
    write_exe(args.elf, args.payload, args.output, region=args.region, stack=args.stack)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
