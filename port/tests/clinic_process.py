"""Original-resource clinic path and three stable fresh-process resumes."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-clinic-") as directory:
    save = str(pathlib.Path(directory) / "clinic.hms")
    subprocess.run([sys.argv[1], sys.argv[3], save], check=True, timeout=240)
    for snapshot in (save + ".bought", save + ".held", save):
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
