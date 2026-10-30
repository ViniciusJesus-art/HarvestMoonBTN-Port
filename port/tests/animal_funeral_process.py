"""Exercise original funerals; open pending and completed gameplay states in fresh apps."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-funeral-") as directory:
    snapshot = str(pathlib.Path(directory) / "funeral.hms")
    subprocess.run([sys.argv[1], sys.argv[3], snapshot], check=True, timeout=180)
    for path in (snapshot + ".pending", snapshot):
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", path,
                        "--frames", "4"], check=True, timeout=60)
