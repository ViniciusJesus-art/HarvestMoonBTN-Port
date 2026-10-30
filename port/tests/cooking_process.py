"""Buy utensils through the original flow, then cook and resume in new processes."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-cooking-") as directory:
    acquired = str(pathlib.Path(directory) / "acquired.hms")
    cooked = str(pathlib.Path(directory) / "cooked.hms")
    subprocess.run([sys.argv[1], sys.argv[4], acquired], check=True, timeout=180)
    subprocess.run([sys.argv[2], sys.argv[4], acquired, cooked], check=True, timeout=180)
    subprocess.run([sys.argv[3], "--data", sys.argv[4], "--load-save", cooked,
                    "--frames", "3"], check=True, timeout=60)
