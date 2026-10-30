"""Buy, wrap and deliver bread; resume hand, bag and delivered checkpoints."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-wrapping-") as directory:
    save = str(pathlib.Path(directory) / "wrapping.hms")
    subprocess.run([sys.argv[1], sys.argv[3], save], check=True, timeout=180)
    for snapshot in (save + ".held", save + ".bag", save):
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
