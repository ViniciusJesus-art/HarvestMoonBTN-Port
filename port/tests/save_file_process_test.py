"""Real process restart and isolated filesystem failures; no installed game data."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-native-saves-") as directory:
    root = pathlib.Path(directory)
    binary = sys.argv[1]
    subprocess.run([binary, "exercise", str(root)], check=True, timeout=30)
    save = root / "restart.hms"
    subprocess.run([binary, "write", str(save)], check=True, timeout=30)
    subprocess.run([binary, "read", str(save)], check=True, timeout=30)
