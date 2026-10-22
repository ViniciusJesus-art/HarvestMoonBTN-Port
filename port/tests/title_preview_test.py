"""Exercise the executable's full preview path with synthetic data only."""

from pathlib import Path
import os
import struct
import subprocess
import sys
import tempfile
import shutil
import unittest

EXECUTABLE = sys.argv.pop(1)


class TitlePreviewTest(unittest.TestCase):
    def test_wide_window(self):
        with tempfile.TemporaryDirectory(prefix="hmbtn presentation ") as temp:
            path = Path(temp) / "wide.bmp"
            env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software")
            result = subprocess.run([EXECUTABLE, "--window-test", "--window-size", "1280x720",
                                     "--frames", "3", "--screenshot", str(path)],
                                    env=env, capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            bmp = path.read_bytes()
            width, height = struct.unpack_from("<ii", bmp, 18)
            self.assertEqual((width, abs(height)), (1280, 720))
            offset = struct.unpack_from("<I", bmp, 10)[0]
            bits = struct.unpack_from("<H", bmp, 28)[0]
            stride = ((width * bits + 31) // 32) * 4
            # The 4:3 diagnostic is 960x720, centred with 160px clear-colour margins.
            row = offset + (abs(height) - 31 if height > 0 else 30) * stride
            self.assertEqual(bmp[row + 80*(bits//8):row + 80*(bits//8)+3], bytes([29, 23, 16]))
            self.assertEqual(bmp[row + 1200*(bits//8):row + 1200*(bits//8)+3], bytes([29, 23, 16]))
            self.assertEqual(bmp[row + 640*(bits//8):row + 640*(bits//8)+3], bytes([56, 55, 32]))
            for size in ["0x0", "1920x0", "9999999999999999999999x720", "1280x720abc"]:
                result = subprocess.run([EXECUTABLE, "--window-size", size], env=env,
                                        capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 2)

    def test_preview_and_bad_resources(self):
        with tempfile.TemporaryDirectory(prefix="hmbtn title test ") as temp:
            root = Path(temp)
            resources = root / "A_FILE"
            resources.mkdir()
            block = bytearray(0x23000)
            # Two images, one list/part, 38 ranges sharing two animation frames.
            struct.pack_into("<I5H2x5I", block, 0, 0x0E, 2, 1, 1, 38, 2,
                             36, 52, 56, 68, 220)
            struct.pack_into("<4H", block, 36, 0, 0, 1, 1)
            struct.pack_into("<4H", block, 44, 0, 4, 16, 1)
            struct.pack_into("<2H", block, 52, 1, 0)
            struct.pack_into("<2H", block, 56, 0, 1)
            for i in range(38):
                struct.pack_into("<2H", block, 68 + i * 4, 2, 0)
            block[226] = 2
            block[236] = 2
            block[237] = 0x80
            # Palette followed by one raw image word, both padded to four bytes.
            bank = 0xD1C
            struct.pack_into("<BBH", block, bank, 0x0D, 0, 2)
            struct.pack_into("<I4H", block, bank + 4, 0x06000020, 0, 4, 16, 1)
            struct.pack_into("<H", block, bank + 18, 0x001F)
            struct.pack_into("<I4H", block, bank + 48, 0x04000002, 0, 0, 1, 1)
            struct.pack_into("<H", block, bank + 60, 0x1111)
            path = resources / "00_gf.bin"
            path.write_bytes(bytes(0x2000) + block)
            # Minimal VAB/SEQ, written sparsely at the original sound archive offsets.
            vh = bytearray(4096)
            vh[:4] = b"pBAV"
            struct.pack_into("<I", vh, 4, 7)
            struct.pack_into("<3H", vh, 18, 1, 1, 1)
            vh[24:26] = bytes([127, 64])
            vh[32:37] = bytes([1, 127, 0, 0, 64])
            vh[0x820:0x828] = bytes([0, 0, 127, 64, 72, 0, 0, 120])
            struct.pack_into("<4H", vh, 0x830, 0x80ff, 0x5fcb, 0, 1)
            struct.pack_into("<H", vh, 0xa22, 4)
            vb = bytes([8, 4]) + bytes([0x17]) * 14 + bytes([8, 3]) + bytes([0x71]) * 14
            seq = b"pQES" + bytes([0, 0, 0, 1, 0, 96, 7, 161, 32, 4, 2,
                                      0, 0x90, 60, 100, 32, 0x90, 60, 0, 32, 255, 47, 0])
            sound = resources / "09_sound.stm"
            with sound.open("wb") as out:
                out.write(vh)
                out.write(vb)
                out.seek(4411392)
                out.write(vh + bytes(2048))
                out.write(vb)
                out.seek(4517888)
                out.write(seq.ljust(10240, b"\0"))
            env = dict(os.environ, SDL_VIDEO_DRIVER="dummy", SDL_RENDER_DRIVER="software",
                       SDL_AUDIO_DRIVER="dummy")
            screenshot = root / "preview.bmp"

            def run(*args):
                return subprocess.run([EXECUTABLE, "--data", str(root), "--frames", "30", *args],
                                      env=env, capture_output=True, text=True, timeout=15)

            result = run("--screenshot", str(screenshot))
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Title assets loaded", result.stdout)
            self.assertIn("Frames presented: 30", result.stdout)
            self.assertIn("Native audio ready: dummy, song 26", result.stdout)
            self.assertNotIn("Title state: 7", result.stdout)  # Publisher precedes options.
            bmp = screenshot.read_bytes()
            self.assertEqual(bmp[:2], b"BM")
            offset = struct.unpack_from("<I", bmp, 10)[0]
            width, height = struct.unpack_from("<ii", bmp, 18)
            bits = struct.unpack_from("<H", bmp, 28)[0]
            stride = ((width * bits + 31) // 32) * 4
            # Publisher background reaches the bottom; no diagnostic bar or text.
            pixel = offset + (2 if height > 0 else abs(height) - 3) * stride + width // 2 * (bits // 8)
            self.assertEqual(len(set(bmp[pixel:pixel + 3])), 1)
            # Portable Windows/Linux layout must not depend on the working directory.
            portable = root / "portable espaço"
            (portable / "data" / "A_FILE").mkdir(parents=True)
            executable = portable / Path(EXECUTABLE).name
            shutil.copy2(EXECUTABLE, executable)
            for dll in Path(EXECUTABLE).parent.glob("*.dll"):
                shutil.copy2(dll, portable / dll.name)
            shutil.copy2(path, portable / "data" / "A_FILE" / "00_gf.bin")
            shutil.copy2(sound, portable / "data" / "A_FILE" / "09_sound.stm")
            result = subprocess.run([str(executable), "--frames", "3"], cwd=root, env=env,
                                    capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Title assets loaded", result.stdout)
            path.write_bytes(b"truncated")
            result = run()
            self.assertEqual(result.returncode, 1)
            self.assertIn("Resource interval outside", result.stderr)
            block[0] = 0
            path.write_bytes(bytes(0x2000) + block)
            result = run()
            self.assertEqual(result.returncode, 1)
            self.assertIn("Invalid ANI header", result.stderr)
            path.unlink()
            self.assertEqual(run().returncode, 1)


if __name__ == "__main__":
    unittest.main()
