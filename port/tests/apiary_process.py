"""Original flowers -> beehive -> honey -> Louis -> bonus, with fresh processes."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-apiary-") as directory:
    save = str(pathlib.Path(directory) / "honey.hms")
    subprocess.run([sys.argv[1], sys.argv[3], save], check=True, timeout=300)
    for snapshot in (save + ".hive", save + ".taken", save + ".gift", save):
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
