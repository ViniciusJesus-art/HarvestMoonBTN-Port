"""Shared crop traversal, then load seasonal growth/regrowth in a fresh app."""
import pathlib
import subprocess
import sys
import tempfile

crop = sys.argv[4]
if crop not in ("tomato", "corn", "onion", "eggplant", "carrot", "sweet-potato"):
    raise SystemExit("Unsupported crop traversal")
with tempfile.TemporaryDirectory(prefix=f"hmbtn-{crop}-") as directory:
    save = str(pathlib.Path(directory) / "growing.hms")
    subprocess.run([sys.argv[1], sys.argv[3], f"--{crop}", save],
                   check=True, timeout=180)
    subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", save,
                    "--frames", "3"], check=True, timeout=60)

    winter_save = save + ".winter.hms"
    if crop in ("eggplant", "carrot", "sweet-potato"):
        if not pathlib.Path(winter_save).is_file():
            raise SystemExit("Missing winter traversal save")
        subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save", winter_save,
                        "--frames", "3"], check=True, timeout=60)
