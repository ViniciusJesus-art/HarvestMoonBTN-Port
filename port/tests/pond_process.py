"""Purchased feed and caught fish through pond growth; fresh app resumes."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-pond-") as directory:
    save = str(pathlib.Path(directory) / "pond.hms")
    subprocess.run([sys.argv[1], sys.argv[3], save], check=True, timeout=240)
    for snapshot in (save + ".fed", save + ".grown", save):
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
