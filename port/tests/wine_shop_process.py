"""Original winery purchases, drink/recipe use and three fresh-process resumes."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-winery-") as directory:
    save = str(pathlib.Path(directory) / "winery.hms")
    subprocess.run([sys.argv[1], sys.argv[3], save], check=True, timeout=240)
    for snapshot in (save + ".bought", save + ".held", save):
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
