"""Original Kappa route, then independent app resumes before and after the reward."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-kappa-") as directory:
    save = str(pathlib.Path(directory) / "kappa.hms")
    subprocess.run([sys.argv[1], sys.argv[3], save], check=True, timeout=180)
    for snapshot in (save + ".before", save):
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
