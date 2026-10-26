"""Traverse grass and barn actions, then open the resulting save in a new app."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-grass-") as directory:
    save = str(pathlib.Path(directory) / "grass.hms")
    subprocess.run([sys.argv[1], sys.argv[3], save], check=True, timeout=180)
    subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", save,
                    "--frames", "3"], check=True, timeout=60)
