"""Caught/bred fish -> original Greg farm event -> actual improved-rod fishing."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-greg-pole-") as directory:
    pond = str(pathlib.Path(directory) / "pond.hms")
    save = str(pathlib.Path(directory) / "pole.hms")
    subprocess.run([sys.argv[1], sys.argv[4], pond], check=True, timeout=240)
    subprocess.run([sys.argv[2], sys.argv[4], pond + ".grown", save], check=True, timeout=300)
    for snapshot in (save + ".ready", save + ".quiet", save):
        subprocess.run([sys.argv[3], "--data", sys.argv[4], "--load-save", snapshot,
                        "--frames", "3"], check=True, timeout=60)
