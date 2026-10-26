"""Exercise game resources, then launch a fresh native application with the save."""
import pathlib
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="hmbtn-save-gameplay-") as directory:
    subprocess.run([sys.argv[1], sys.argv[3], directory], check=True, timeout=180)
    subprocess.run([sys.argv[2], "--data", sys.argv[3], "--load-save",
                    str(pathlib.Path(directory) / "farm.hms"), "--frames", "3"],
                   check=True, timeout=60)
    # The real app selects Continue with SDL keyboard events; no private scene.
    for filename, expected in (("farm.hms", "Native save loaded;"),
                               ("missing.hms", "Cannot continue:")):
        result = subprocess.run([sys.argv[4], sys.argv[3], str(pathlib.Path(directory) / filename)],
                                text=True, capture_output=True, timeout=60)
        assert result.returncode == 0, result.stdout + result.stderr
        assert expected in result.stdout + result.stderr, result.stdout + result.stderr
