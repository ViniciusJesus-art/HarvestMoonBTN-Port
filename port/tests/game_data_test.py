"""Synthetic-disc tests: no proprietary game data or SDK needed."""

from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import game_data


class GameDataTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="hmbtn-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.files = self.root / "source"
        for name in game_data.required_files():
            if not name.startswith("disc/"):
                continue
            path = self.files / name.removeprefix("disc/")
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"")
        (self.files / "license_data.dat").write_bytes(bytes(12 * 2336))
        (self.files / "SYSTEM.CNF").write_text('BOOT = cdrom:\\SLUS_011.15;1\n')
        (self.files / "SLUS_011.15").write_bytes(b"synthetic executable")
        (self.files / "A_FILE.BIN").write_bytes(bytes(32 * 2048))
        (self.files / "A_FILE.HDT").write_bytes(struct.pack("<33I", *range(0, 33 * 2048, 2048)))
        self.image = self.root / "input.bin"
        game_data.psx_disc.build(game_data.ROOT / "disc/disc.xml", self.files, {}, self.image, None)
        self.installed = self.root / "installed data"

    def test_install_survives_source_removal_and_preserves_members(self):
        game_data.install(self.image, self.installed)
        self.image.unlink()
        result = game_data.verify(self.installed)
        self.assertFalse(result["matches_reference"])
        self.assertEqual(len(list((self.installed / "A_FILE").iterdir())), 32)
        self.assertFalse((self.installed / "disc/A_FILE.BIN").exists())
        self.assertEqual((self.installed / "A_FILE/00_gf.bin").read_bytes(), bytes(2048))
        with self.assertRaisesRegex(ValueError, "already exists"):
            game_data.install(self.image, self.installed)

    def test_detects_changed_and_missing_files(self):
        game_data.install(self.image, self.installed)
        resource = self.installed / "A_FILE/00_gf.bin"
        resource.write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "integrity"):
            game_data.verify(self.installed)
        resource.unlink()
        with self.assertRaisesRegex(ValueError, "Missing"):
            game_data.verify(self.installed)

    def test_invalid_directory_leaves_no_partial_install(self):
        offsets = list(range(0, 33 * 2048, 2048))
        offsets[2] = 0
        (self.files / "A_FILE.HDT").write_bytes(struct.pack("<33I", *offsets))
        game_data.psx_disc.build(game_data.ROOT / "disc/disc.xml", self.files, {}, self.image, None)
        with self.assertRaisesRegex(ValueError, "ascending"):
            game_data.install(self.image, self.installed)
        self.assertFalse(self.installed.exists())


if __name__ == "__main__":
    unittest.main()
