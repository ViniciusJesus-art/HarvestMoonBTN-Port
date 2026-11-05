"""Adult horse bytecode path and fresh app resumes while mounted and on foot."""
import os
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-horse-riding-") as directory:
    save = str(pathlib.Path(directory) / "horse.hms")
    shots = [os.environ["HMBTN_HORSE_RIDING_SHOTS"]] if os.environ.get("HMBTN_HORSE_RIDING_SHOTS") else []
    subprocess.run([sys.argv[1], sys.argv[3], save, *shots], check=True, timeout=180)
    for snapshot in (save + ".stable", save, save + ".after", save + ".dawn"):
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
