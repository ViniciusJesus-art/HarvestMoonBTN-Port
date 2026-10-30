"""Installed Doug counter: meals/day-night transition and stable fresh resumes."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-inn-") as directory:
    save = str(pathlib.Path(directory) / "inn.hms")
    subprocess.run([sys.argv[1], sys.argv[3], save], check=True, timeout=240)
    for snapshot in (save + ".day", save + ".night", save):
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
