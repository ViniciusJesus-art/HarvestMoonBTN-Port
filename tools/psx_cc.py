"""Compile one C file to an object, the way the original toolchain did.

    cpp | cc1 | maspsx | psx_bss.py | as ; objcopy --set-section-alignment .text=1

CC1PSX was GCC 2.95.2's cc1 and ASPSX an assembler GNU ``as`` replaces once maspsx
has rewritten cc1's output the way ASPSX read it; ``psx_bss.py`` lays out ``.bss``
as ASPSX and PSYLINK did. CMake gives every stage its whole
command, flags included, so the flags live in the CMake files and nowhere else:

    psx_cc.py SRC -o OBJ -MF DEPFILE \\
        --cpp CMD... --cc1 CMD... --maspsx CMD... --bss CMD... --as CMD... --objcopy CMD...

The stages run in that order, each reading the previous one's output. This script
adds only the file names: ``-MD DEPFILE SRC`` to cpp, ``-o OBJ`` to as and ``OBJ``
to objcopy. Run it from the project root, where the source paths and ``-I``
directories resolve. The dependency file cpp writes is rewritten with the object
as its target and every path absolute, as CMake's ``DEPFILE`` expects.

Standard library only.
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

STAGES = ("--cpp", "--cc1", "--maspsx", "--bss", "--as", "--objcopy")


def parse(argv: list[str]) -> tuple[str, str, str, dict[str, list[str]]]:
    head, stages, current = [], {}, None
    for arg in argv:
        if arg in STAGES:
            current = arg
            stages[current] = []
        elif current is None:
            head.append(arg)
        else:
            stages[current].append(arg)
    missing = [s for s in STAGES if not stages.get(s)]
    if len(head) != 5 or head[1] != "-o" or head[3] != "-MF" or missing:
        sys.exit("usage: psx_cc.py SRC -o OBJ -MF DEPFILE " + " ".join(f"{s} CMD..." for s in STAGES)
                 + (f"\nmissing: {' '.join(missing)}" if missing else ""))
    return head[0], head[2], head[4], stages


def run(cmd: list[str], data: bytes | None = None) -> bytes:
    proc = subprocess.run(cmd, input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    sys.stderr.write(proc.stderr.decode(errors="replace"))
    if proc.returncode != 0:
        sys.exit(f"psx_cc.py: {Path(cmd[0]).name} failed ({proc.returncode}): {' '.join(cmd)}")
    return proc.stdout


def rewrite_depfile(path: str, target: str) -> None:
    """cpp's ``base.o: deps...`` as ``/abs/OBJ: /abs/deps...``."""
    text = Path(path).read_text().replace("\\\n", " ")
    _, deps = text.split(":", 1)
    lines = [f"{os.path.abspath(target)}:"]
    lines += [f" {os.path.abspath(dep)}" for dep in deps.split()]
    Path(path).write_text(" \\\n".join(lines) + "\n")


def main(argv: list[str]) -> int:
    src, obj, depfile, stages = parse(argv)
    Path(obj).parent.mkdir(parents=True, exist_ok=True)
    preprocessed = run([*stages["--cpp"], "-MD", depfile, src])
    compiled = run(stages["--cc1"], preprocessed)
    massaged = run(stages["--maspsx"], compiled)
    laid_out = run(stages["--bss"], massaged)
    run([*stages["--as"], "-o", obj], laid_out)
    run([*stages["--objcopy"], obj])
    rewrite_depfile(depfile, obj)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
