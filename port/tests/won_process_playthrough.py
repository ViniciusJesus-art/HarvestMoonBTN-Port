"""Original-resource Won/crop traversal, then resume its save in a fresh app."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix=f"hmbtn-won-{sys.argv[4]}-") as directory:
    save = str(pathlib.Path(directory) / "crop.hms")
    subprocess.run([sys.argv[1], sys.argv[3], sys.argv[4], save],
                   check=True, timeout=180)
    subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", save,
                    "--frames", "3"], check=True, timeout=60)
