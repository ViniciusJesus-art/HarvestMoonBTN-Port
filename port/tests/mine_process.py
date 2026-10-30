"""Both source mine chains and their underground/completed snapshots in new apps."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-mine-") as directory:
    for variant in (0, 1):
        save = str(pathlib.Path(directory) / f"mine-{variant}.hms")
        subprocess.run([sys.argv[1], sys.argv[3], str(variant), save], check=True, timeout=180)
        for snapshot in (save + ".underground", save):
            subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", snapshot,
                            "--frames", "3"], check=True, timeout=60)
