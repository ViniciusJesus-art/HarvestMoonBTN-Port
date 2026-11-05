"""Seasonal growth, paid ball and fetch, then three independent app resumes."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-dog-ball-") as directory:
    save = str(pathlib.Path(directory) / "ball.hms")
    subprocess.run([sys.argv[1], sys.argv[3], save], check=True, timeout=180)
    for snapshot in (save + ".adult", save + ".return", save):
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
