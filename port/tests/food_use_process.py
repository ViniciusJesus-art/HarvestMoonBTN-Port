"""Buy/cook ingredients, eat/drink through controls, resume stable states."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-food-use-") as directory:
    bought = str(pathlib.Path(directory) / "bought.hms")
    save = str(pathlib.Path(directory) / "used.hms")
    subprocess.run([sys.argv[1], sys.argv[4], bought], check=True, timeout=180)
    subprocess.run([sys.argv[2], sys.argv[4], bought, save], check=True, timeout=180)
    for snapshot in (save + ".eaten", save + ".drink", save):
        subprocess.run([sys.argv[3], "--data", sys.argv[4], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
