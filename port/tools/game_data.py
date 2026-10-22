"""Install/verify extracted game data; Python 3.10+, no external packages."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import afile
import psx_disc


def digest(path: Path, algorithm: str = "sha256") -> str:
    result = hashlib.new(algorithm)
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def required_files() -> set[str]:
    files = {"disc/license_data.dat"}

    def walk(node, prefix=""):
        for child in node:
            if child.tag == "dir":
                walk(child, prefix + child.attrib["name"] + "/")
            elif child.tag == "file" and child.attrib["name"] != "A_FILE.BIN":
                files.add("disc/" + prefix + child.attrib["name"])

    walk(ET.parse(ROOT / "disc/disc.xml").find(".//directory_tree"))
    files.update("A_FILE/" + m.file for m in afile.load_members(ROOT / "disc/a_file.txt"))
    return files


def verify(folder: Path) -> dict:
    manifest = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("format") != 1 or manifest.get("game") != "SLUS_011.15":
        raise ValueError("Unsupported game data manifest")
    files = manifest["files"]
    if set(files) != required_files():
        raise ValueError("Manifest does not contain the required game files")
    for name, record in files.items():
        path = folder / name
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"Missing file or symbolic link: {name}")
        if path.stat().st_size != record["size"] or digest(path) != record["sha256"]:
            raise ValueError(f"File integrity check failed: {name}")
    return manifest


def install(image: Path, destination: Path) -> dict:
    if destination.exists():
        raise ValueError(f"Destination already exists (will not overwrite): {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    members = afile.load_members(ROOT / "disc/a_file.txt")
    # Stage on the destination filesystem; failed imports leave no partial installation.
    with tempfile.TemporaryDirectory(prefix=".hmbtn-import-", dir=destination.parent) as temp:
        staged = Path(temp) / "data"
        disc = staged / "disc"
        psx_disc.extract(image, disc)
        boot = (disc / "SYSTEM.CNF").read_text(encoding="ascii")
        if "SLUS_011.15" not in boot or not (disc / "SLUS_011.15").is_file():
            raise ValueError("Expected the American SLUS_011.15 disc layout")
        directory = (disc / "A_FILE.HDT").read_bytes()
        if len(directory) != (len(members) + 1) * 4:
            raise ValueError("Unexpected A_FILE.HDT length")
        offsets = struct.unpack(f"<{len(members) + 1}I", directory)
        if any(a > b for a, b in zip(offsets, offsets[1:])):
            raise ValueError("A_FILE.HDT offsets are not ascending")
        archive_sha1 = digest(disc / "A_FILE.BIN", "sha1")
        afile.unpack(members, disc / "A_FILE.BIN", disc / "A_FILE.HDT", staged / "A_FILE")
        # The individual members replace the large aggregate; no disc image is retained.
        (disc / "A_FILE.BIN").unlink()
        differences = []
        for line in (ROOT / "disc/binaries.sha1").read_text().splitlines():
            expected, name = line.split()
            path = disc / name if name == "SLUS_011.15" else staged / "A_FILE" / name
            if digest(path, "sha1") != expected:
                differences.append(name)
        expected_archive = (ROOT / "disc/disc.sha1").read_text().split()[0]
        files = {}
        for name in sorted(required_files()):
            path = staged / name
            files[name] = {"size": path.stat().st_size, "sha256": digest(path)}
        manifest = {
            "format": 1,
            "game": "SLUS_011.15",
            "source_sha1": digest(image, "sha1"),
            "archive_sha1": archive_sha1,
            "matches_reference": not differences and archive_sha1 == expected_archive,
            "modified_code": differences,
            "files": files,
        }
        (staged / "manifest.json").write_text(
            json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        verify(staged)
        staged.rename(destination)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    importing = commands.add_parser("import", help="Extract a MODE2/2352 image once")
    importing.add_argument("image", type=Path)
    importing.add_argument("--out", type=Path, required=True)
    checking = commands.add_parser("verify", help="Verify installed files without the image")
    checking.add_argument("folder", type=Path)
    args = parser.parse_args()
    try:
        if args.command == "import":
            manifest = install(args.image, args.out)
        else:
            manifest = verify(args.folder)
    except (OSError, ValueError, KeyError, struct.error) as error:
        parser.exit(1, f"Game data error: {error}\n")
    print(f"Verified {len(manifest['files'])} extracted files; source image is not required.")
    if not manifest["matches_reference"]:
        print("Modified source: compatibility with compiled American code is unverified.")
        print("Modified code: " + ", ".join(manifest["modified_code"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
