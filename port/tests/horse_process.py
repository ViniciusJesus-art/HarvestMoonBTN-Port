"""Original acquisition routes and fresh application resumes of the same pet."""
import os
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-horse-") as directory:
    save = str(pathlib.Path(directory) / "pony.hms")
    shots = [os.environ["HMBTN_HORSE_SHOTS"]] if os.environ.get("HMBTN_HORSE_SHOTS") else []
    subprocess.run([sys.argv[1], sys.argv[3], save, *shots], check=True, timeout=180)
    for snapshot in (save + ".inside", save):
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
