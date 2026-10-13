"""Unpack and pack the game's archive, A_FILE.BIN and its directory A_FILE.HDT.

A_FILE.HDT is little-endian u32 byte offsets into A_FILE.BIN, one per member and a
last one, the archive's size; member i is BIN[offsets[i]:offsets[i + 1]]. Every member
is zero-padded to whole 2048-byte sectors, so every offset is a multiple of 2048.

    afile.py unpack --members disc/a_file.txt --bin A_FILE.BIN --hdt A_FILE.HDT --out DIR
        Every member into DIR, under its file name in the member list.

    afile.py pack --members disc/a_file.txt --original DIR --built DIR --bin OUT --hdt OUT
        The archive: each member from the directory its row names (`original`: an
        unpacked one; `built`: the project's build directory), in index order.

Standard library only.
"""

from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

SECTOR = 0x800


@dataclass
class Member:
    index: int
    file: str
    source: str          # "original" or "built"
    name: str            # the EXE's name for it


def load_members(path: Path) -> list[Member]:
    members = []
    for line in path.read_text().splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        index, file, source, name = line.split()
        if source not in ("original", "built"):
            raise SystemExit(f"{path}: member {index} comes from {source!r}")
        members.append(Member(int(index), file, source, name))
    if [m.index for m in members] != list(range(len(members))):
        raise SystemExit(f"{path}: the members are not numbered 0..{len(members) - 1} in order")
    return members


def unpack(members: list[Member], bin_path: Path, hdt_path: Path, out: Path) -> None:
    data, directory = bin_path.read_bytes(), hdt_path.read_bytes()
    offsets = struct.unpack(f"<{len(directory) // 4}I", directory)
    if len(offsets) != len(members) + 1:
        raise SystemExit(f"{hdt_path}: {len(offsets) - 1} members, the list has {len(members)}")
    if offsets[0] != 0 or offsets[-1] != len(data):
        raise SystemExit(f"{hdt_path}: the offsets do not span {bin_path}")
    if any(offset % SECTOR for offset in offsets):
        raise SystemExit(f"{hdt_path}: an offset is not sector-aligned")
    out.mkdir(parents=True, exist_ok=True)
    for member, start, end in zip(members, offsets, offsets[1:]):
        (out / member.file).write_bytes(data[start:end])


def pack(members: list[Member], original: Path, built: Path, bin_path: Path,
         hdt_path: Path) -> None:
    offsets, archive = [0], bytearray()
    for member in members:
        content = ((built if member.source == "built" else original) / member.file).read_bytes()
        archive += content + bytes(-len(content) % SECTOR)
        offsets.append(len(archive))
    bin_path.write_bytes(archive)
    hdt_path.write_bytes(struct.pack(f"<{len(offsets)}I", *offsets))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    commands = parser.add_subparsers(dest="command", required=True)
    for name in ("unpack", "pack"):
        p = commands.add_parser(name)
        p.add_argument("--members", type=Path, required=True)
        p.add_argument("--bin", type=Path, required=True)
        p.add_argument("--hdt", type=Path, required=True)
        if name == "unpack":
            p.add_argument("--out", type=Path, required=True)
        else:
            p.add_argument("--original", type=Path, required=True)
            p.add_argument("--built", type=Path, required=True)
    args = parser.parse_args(argv)
    members = load_members(args.members)
    if args.command == "unpack":
        unpack(members, args.bin, args.hdt, args.out)
    else:
        pack(members, args.original, args.built, args.bin, args.hdt)
    return 0


if __name__ == "__main__":
    sys.exit(main())
