"""Real puppy route, followed by fresh application resumes indoors/outdoors."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-dog-") as directory:
    save = str(pathlib.Path(directory) / "dog.hms")
    subprocess.run([sys.argv[1], sys.argv[3], save], check=True, timeout=180)
    for snapshot in (save + ".inside", save):
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
