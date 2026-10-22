"""Exercise the farm preview with a sparse, wholly synthetic archive."""
from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile
import unittest

EXECUTABLE = sys.argv.pop(1)


class ScenePreviewTest(unittest.TestCase):
    def test_preview_and_invalid_model(self):
        with tempfile.TemporaryDirectory(prefix="hmbtn scene assets ") as temp:
            root = Path(temp)
            (root / "A_FILE").mkdir()
            archive = root / "A_FILE/00_gf.bin"
            # One flat red triangle around the development camera target.
            model = bytearray(80)
            struct.pack_into("<3I", model, 0, 0x42, 0, 1)
            struct.pack_into("<7I", model, 12, 28, 3, 0, 0, 52, 1, 0)
            for i, vertex in enumerate([(-6800, 0, 1200), (-4800, 0, 5200), (-2800, 0, 1200)]):
                struct.pack_into("<3h", model, 40 + i * 8, *vertex)
            struct.pack_into("<I3Bx3Hxx", model, 64, 0x21018000, 255, 0, 0, 0, 1, 2)
            bank = struct.pack("<HHI4H", 0x0D, 1, 0x04000002, 0, 0, 1, 1) + bytes(4)
            with archive.open("wb") as output:
                output.truncate(0x386A800)
                for i in range(15):
                    output.seek(0x83800 + 0x948 + i * 44)
                    output.write(struct.pack("<I", 0x8000))
                # Empty, valid field sprites: no proprietary resources in fixtures.
                ani = bytearray(82)
                struct.pack_into("<I5H2x5I", ani, 0, 14, 2, 1, 1, 1, 1, 36, 52, 56, 68, 72)
                struct.pack_into("<4H", ani, 36, 0, 0, 1, 1)
                struct.pack_into("<4H", ani, 44, 0, 1, 16, 1)
                struct.pack_into("<2H", ani, 52, 1, 0)
                struct.pack_into("<2H", ani, 56, 0, 1)
                struct.pack_into("<2H", ani, 68, 1, 0)
                struct.pack_into("<HhhBBH", ani, 72, 0xFFFF, 0, 0, 1, 255, 0)
                for at in [0, 200, 400, 632, 832, 1032, 1196]:
                    output.seek(0x385F800 + at)
                    output.write(ani)
                output.seek(0x385F800 + 5588)
                output.write(bank)
                output.seek(0x85800)
                output.write(model)
                output.seek(0xBA800)
                output.write(bank)
            env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software")
            screenshot = root / "scene.bmp"
            command = [EXECUTABLE, "--scene-preview", "--data", str(root), "--frames", "3",
                       "--window-size", "1280x720", "--screenshot", str(screenshot)]
            completed = subprocess.run(command, env=env, capture_output=True, text=True, timeout=15)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            self.assertIn("1 objects, 1 primitives", completed.stdout)
            self.assertIn("Frames presented: 3", completed.stdout)
            bmp = screenshot.read_bytes()
            width, height = struct.unpack_from("<ii", bmp, 18)
            self.assertEqual((width, abs(height)), (1280, 720))
            offset = struct.unpack_from("<I", bmp, 10)[0]
            bits = struct.unpack_from("<H", bmp, 28)[0]
            stride = ((width * bits + 31) // 32) * 4
            row = offset + (abs(height) - 361 if height > 0 else 360) * stride
            pixel = bmp[row + 640 * (bits // 8):row + 640 * (bits // 8) + 3]
            self.assertEqual(pixel[:2], bytes(2))
            self.assertGreater(pixel[2], 240)  # Center is actual rendered red geometry.
            self.assertEqual(bmp[row + 80 * (bits // 8):row + 80 * (bits // 8) + 3], bytes(3))
            with archive.open("r+b") as output:
                output.seek(0x85800 + 12)
                output.write(struct.pack("<I", 0xFFFFFFF0))
            completed = subprocess.run(command, env=env, capture_output=True, text=True, timeout=15)
            self.assertEqual(completed.returncode, 1)
            self.assertIn("outside bounds", completed.stderr)
            archive.unlink()
            completed = subprocess.run(command, env=env, capture_output=True, text=True, timeout=15)
            self.assertEqual(completed.returncode, 1)
            self.assertIn("00_gf.bin", completed.stderr)

    def test_exclusive_modes(self):
        for args in [["--scene-preview", "--window-test"], ["--diagnostics", "--scene-preview"],
                     ["--intro-preview", "--scene-preview"], ["--intro-preview", "--window-test"],
                     ["--intro-preview", "--diagnostics"], ["--childhood-preview", "--diagnostics"],
                     ["--childhood-preview", "--intro-preview"], ["--childhood-preview", "--window-test"],
                     ["--scene-preview", "--childhood-preview"], ["--house-preview", "--diagnostics"],
                     ["--house-preview", "--scene-preview"], ["--house-preview", "--intro-preview"],
                     ["--house-preview", "--childhood-preview"], ["--house-preview", "--window-test"]]:
            completed = subprocess.run([EXECUTABLE, *args], capture_output=True, text=True, timeout=5)
            self.assertEqual(completed.returncode, 2)


if __name__ == "__main__":
    unittest.main()
