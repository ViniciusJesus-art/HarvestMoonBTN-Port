"""Run the shared crop traversal, then resume its mid-growth save in a new app."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-potato-") as directory:
    save = str(pathlib.Path(directory) / "growing.hms")
    subprocess.run([sys.argv[1], sys.argv[3], "--potato", save],
                   check=True, timeout=180)
    subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", save,
                    "--frames", "3"], check=True, timeout=60)
