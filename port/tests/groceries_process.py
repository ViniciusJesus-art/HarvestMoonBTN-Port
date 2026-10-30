"""Buy all five foods, cook bought ingredients, resume stable checkpoints."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-groceries-") as directory:
    save = str(pathlib.Path(directory) / "groceries.hms")
    subprocess.run([sys.argv[1], sys.argv[3], save], check=True, timeout=180)
    for snapshot in (save + ".held", save + ".stock", save):
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
