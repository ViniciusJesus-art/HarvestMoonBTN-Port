"""Installed Kai script and meals, then fresh app resumes of stable checkpoints."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-kai-") as directory:
    save = str(pathlib.Path(directory) / "kai.hms")
    subprocess.run([sys.argv[1], sys.argv[3], save], check=True, timeout=240)
    for snapshot in (save + ".water", save + ".food", save):
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
