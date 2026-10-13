"""Lay out a C unit's ``.bss`` the way the original toolchain did.

Two kinds of uninitialised object reach a unit's ``.bss``, and the original
toolchain (CC1PSX, ASPSX 2.86, PSYLINK 2.82) treated them differently:

- **A ``static`` (``.lcomm``) is allocated by ASPSX** in the object's own
  ``.bss``, in first-declaration order, each aligned by its size rounded up to
  a power of two, at most 16. maspsx writes each as a bare ``sym: .space N``
  with no alignment at all, so a 2-byte object would push every later one off
  its word: :func:`align_bss_objects` inserts that ``.align``. Measured on
  28_GF_STAFF, whose twelve ``.bss`` objects (2 to 3356 bytes) all sit where
  this rule puts them and nowhere else -- a 12-byte ``DR_MODE`` after a 24-byte
  ``POLY_F4`` lands on the next 16, not the next 8. The alignment is relative
  to the section's start: 21_GF_MG3's ``.bss`` starts 8 past a 16 and its
  objects follow the rule from there (and so do 31_gf_mcard's and 08_DEBUG's).

- **A non-static tentative definition (``.comm``) is not allocated by ASPSX.**
  It emits a size-only record, and PSYLINK places the symbol at the tail of its
  *owning* object's ``.bss``, after that object's statics. The owner is the
  object with an initialised definition, else the largest tentative one, else
  the first in link order. PSYLINK aligns a common by the largest power of two
  not above its size, at most 8 -- not the static rule. Within one object the
  commons follow PSYLINK's hash of their original names, which are unknown, so
  the owning unit's source says their order instead: its definitions, in
  address order.

An image whose link follows PSYLINK (``OWN_COMMONS`` in its CMakeLists.txt:
11_GF_FARM and the EXE) has every C unit allocate its own ``.comm``
symbols: :func:`allocate_commons` turns each into a global definition after the
unit's statics, aligned like PSYLINK aligns it. Every other file reaching the
object declares it ``extern``. A small common (at most ``-G`` bytes, in
``.sbss``: the EXE's small commons) is allocated
the same way, at the tail of the unit's ``.sbss``, by the unit that owns it
(``small``: its tree block places a ``.sbss``); in any other unit it stays a
``.comm`` that ``ld`` resolves to the owner's definition.

In such a link PSYLINK also started every object's ``.bss`` on a doubleword (once the
link held a common of 8 bytes): :func:`end_bss_on_doubleword` ends
the unit's ``.bss`` on one, so the next object's, laid out on a word by the linker
script, lands on a doubleword too.

Standard library only: the build runs this file as its pass between maspsx and
``as``::

    maspsx ... | python3 psx_bss.py [--own-commons [--small-commons]] FILE.c | as ...
"""

from __future__ import annotations

import argparse
import re
import sys
from collections.abc import Sequence

_BSS_BLOCK = ".section .bss"
_SBSS_BLOCK = ".section .sbss"
_SPACE = re.compile(r"^\t\.space (\d+)$")
_COMM = re.compile(r"^\t\.comm ([^,\s]+),(\d+)$")
_MAX_ALIGN_POWER = 4  # 16 bytes: ASPSX's limit for a static
_MAX_COMMON_ALIGN_POWER = 3  # 8 bytes: PSYLINK's limit for a common


def alignment_power(size: int) -> int:
    """log2 of a static's alignment: its size rounded up to a power of two, <= 16."""
    power = 0
    while (1 << power) < size and power < _MAX_ALIGN_POWER:
        power += 1
    return power


def common_alignment_power(size: int) -> int:
    """log2 of a common's alignment: the largest power of two <= its size, <= 8."""
    power = 0
    while (2 << power) <= size and power < _MAX_COMMON_ALIGN_POWER:
        power += 1
    return power


def _last_block(lines: list[str], directive: str) -> int | None:
    try:
        return len(lines) - 1 - lines[::-1].index(directive)
    except ValueError:
        return None


def _outside_small_data(lines: list[str]) -> list[str]:
    """``lines`` without those under a ``.section .sbss`` directive."""
    out, small = [], False
    for line in lines:
        if line.startswith(".section"):
            small = line == _SBSS_BLOCK
        elif not small:
            out.append(line)
    return out


def align_bss_objects(asm: str) -> str:
    """Return maspsx output with each trailing ``.bss`` object aligned by its size.

    maspsx writes the block as ``.section .bss`` followed by, per object,
    ``\\t.globl sym`` / ``sym:`` / ``\\t.space N``. Only the last such block is
    touched; output without one comes back unchanged.
    """
    lines = asm.split("\n")
    start = _last_block(lines, _BSS_BLOCK)
    if start is None:
        return asm

    out = lines[: start + 1]
    block = lines[start + 1 :]
    i = 0
    while i < len(block):
        is_object = (
            i + 2 < len(block)
            and block[i].startswith("\t.globl ")
            and block[i + 1] == block[i][len("\t.globl ") :] + ":"
            and _SPACE.match(block[i + 2])
        )
        if is_object:
            power = alignment_power(int(_SPACE.match(block[i + 2]).group(1)))
            if power:
                out.append(f"\t.align {power}")
            out.extend(block[i : i + 3])
            i += 3
        else:
            out.append(block[i])
            i += 1
    return "\n".join(out)


def definition_order(c_source: str) -> list[str]:
    """The names a C source defines at file scope, in the order it defines them.

    A file-scope declaration defines storage when it is not a ``typedef``, not
    ``extern`` and not a function prototype; function definitions, directives and
    file-scope ``__asm__`` define none. The scan is of the source as written (no
    preprocessing).
    """
    names: list[str] = []
    for code in _top_level_declarations(c_source):
        names += _defined_names(code)
    return names


# -- the file-scope scan behind `definition_order` --------------------------------

_IDENT = re.compile(r"[A-Za-z_]\w*")
_KEYWORDS = frozenset(
    "auto break case char const continue default do double else enum extern float for goto "
    "if inline int long register return short signed sizeof static struct switch typedef "
    "union unsigned void volatile while __attribute__ __asm__ asm __inline__".split())
_MACRO_CALL = re.compile(r"[A-Z][A-Z0-9_]*\s*\(")
_KNR_HEAD = re.compile(r"\(\s*(?!void\b)[A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*\s*\)"
                       r"\s*(?!__attribute__)(?:const|signed|unsigned|struct|union|enum|[A-Za-z_]\w*)"
                       r"[\s*]+[A-Za-z_]\w*\s*[;,\[]")


def _skip_blank(text: str, i: int) -> int:
    """Index of the first character at or after ``i`` that is not blank or a comment."""
    while i < len(text):
        if text[i].isspace():
            i += 1
        elif text.startswith("/*", i) or text.startswith("//", i):
            i = _end_comment(text, i)
        else:
            break
    return i


def _end_comment(text: str, i: int) -> int:
    if text.startswith("/*", i):
        end = text.find("*/", i + 2)
        return len(text) if end < 0 else end + 2
    end = text.find("\n", i)
    return len(text) if end < 0 else end


def _end_quoted(text: str, i: int) -> int:
    quote = text[i]
    i += 1
    while i < len(text) and text[i] != quote:
        i += 2 if text[i] == "\\" else 1
    return i + 1


def _end_directive(text: str, i: int) -> int:
    """Index of the newline that ends the directive at ``i`` (continuations followed)."""
    while i < len(text):
        if text.startswith("/*", i):
            i = _end_comment(text, i)
        elif text[i] == "\\" and text.startswith("\n", i + 1):
            i += 2
        elif text[i] == "\n":
            return i
        else:
            i += 1
    return len(text)


def _starts_construct(text: str, i: int) -> bool:
    i = _skip_blank(text, i)
    return i >= len(text) or text[i] == "#" or text[i].isalpha() or text[i] == "_"


def _end_statement(text: str, i: int) -> int:
    """Index just past the declaration or function definition starting at ``i``: a
    ``;`` outside brackets, or the ``}`` of a body that follows a ``)`` (or a K&R
    parameter list) with no ``=`` before it, or -- a file-scope macro invocation
    without a ``;`` (``FARM_CUE_RUNNER(...)``) -- its closing ``)``."""
    macro_call = _MACRO_CALL.match(text, i) is not None
    knr = body = saw_eq = False
    depth = 0
    prev = ""
    while i < len(text):
        c = text[i]
        if text.startswith("/*", i) or text.startswith("//", i):
            i = _end_comment(text, i)
            continue
        if c in "\"'":
            i = _end_quoted(text, i)
            prev = c
            continue
        if c in "([{":
            if c == "(" and depth == 0 and not macro_call and not saw_eq:
                knr = knr or _KNR_HEAD.match(text, i) is not None
            if c == "{" and depth == 0:
                body = (prev == ")" or knr) and not saw_eq
            depth += 1
        elif c in ")]}":
            depth -= 1
            if c == "}" and depth == 0 and body:
                return i + 1
            if c == ")" and depth == 0 and macro_call and _starts_construct(text, i + 1):
                return i + 1
        elif c == ";" and depth == 0 and not knr:
            return i + 1
        elif c == "=" and depth == 0:
            saw_eq = True
        if not c.isspace():
            prev = c
        i += 1
    return len(text)


def _strip_comments(text: str) -> str:
    out: list[str] = []
    i = 0
    while i < len(text):
        if text.startswith("/*", i) or text.startswith("//", i):
            i = _end_comment(text, i)
            out.append(" ")
        elif text[i] in "\"'":
            j = _end_quoted(text, i)
            out.append(text[i:j])
            i = j
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def _top_level_declarations(text: str) -> list[str]:
    """The code (comments removed) of every file-scope statement that is not a directive."""
    out: list[str] = []
    i = _skip_blank(text, 0)
    while i < len(text):
        if text[i] == "#":
            i = _end_directive(text, i)
        else:
            end = _end_statement(text, i)
            out.append(_strip_comments(text[i:end]).strip())
            i = end
        i = _skip_blank(text, i)
    return out


def _collapse_bodies(code: str) -> str:
    """``code`` with every ``{...}`` (a struct body, an initializer) as ``{}``."""
    out: list[str] = []
    depth = 0
    for c in code:
        if c == "{":
            if depth == 0:
                out.append("{}")
            depth += 1
        elif c == "}":
            depth -= 1
        elif depth == 0:
            out.append(c)
    return "".join(out)


def _split_depth0(code: str, sep: str) -> list[str]:
    parts: list[str] = []
    depth = 0
    cur: list[str] = []
    for c in code:
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        if c == sep and depth == 0:
            parts.append("".join(cur))
            cur = []
        else:
            cur.append(c)
    parts.append("".join(cur))
    return parts


def _drop_attributes(code: str) -> str:
    while True:
        m = re.search(r"__attribute__\s*\(", code)
        if not m:
            return code
        depth = 0
        for j in range(m.end() - 1, len(code)):
            depth += {"(": 1, ")": -1}.get(code[j], 0)
            if depth == 0:
                code = code[:m.start()] + code[j + 1:]
                break
        else:
            return code


def _declarator_name(part: str) -> str | None:
    depth = 0
    for j, c in enumerate(part):
        if c == "(" and depth == 0:
            m = re.match(r"\(\s*\*+\s*(?:const\s+)?([A-Za-z_]\w*)", part[j:])
            if m:
                return m.group(1)                   # a pointer to one: (*f)(...), (*t[4])()
            before = _IDENT.findall(part[:j])
            head = part[:j].rstrip()
            if before and head.endswith(before[-1]) and before[-1] not in _KEYWORDS:
                return before[-1]                   # a function: f(...)
            return None
        depth += {"(": 1, "[": 1, ")": -1, "]": -1}.get(c, 0)
    names = [n for n in _IDENT.findall(re.sub(r"\[[^\]]*\]", "", part)) if n not in _KEYWORDS]
    return names[-1] if names else None


def _is_prototype(declarator: str) -> bool:
    """``int f(void)`` (a function declaration), not ``int (*fp)(void)`` or ``int x``."""
    depth = 0
    for j, c in enumerate(declarator):
        if c == "(" and depth == 0:
            if re.match(r"\(\s*\*", declarator[j:]):
                return False
            head = declarator[:j].rstrip()
            idents = _IDENT.findall(head)
            return bool(idents) and head.endswith(idents[-1]) and idents[-1] not in _KEYWORDS
        depth += {"(": 1, "[": 1, ")": -1, "]": -1}.get(c, 0)
    return False


def _defined_names(code: str) -> list[str]:
    """The objects one file-scope statement defines (none for anything but a definition)."""
    if (re.match(r"(?:INCLUDE_ASM|INCLUDE_RODATA|INCLUDE_DATA|__asm__|asm)\b", code)
            or code.endswith("}")):                 # hosted assembly, asm, a function
        return []
    collapsed = _drop_attributes(_collapse_bodies(code.rstrip(";")))
    if re.match(r"typedef\b", collapsed) or re.search(r"\bextern\b", collapsed):
        return []
    if re.fullmatch(r"\s*(?:struct|union|enum)\s+\w+\s*\{\}\s*", collapsed):
        return []                                   # `struct X {...};` declares only its tag
    declarators = [_split_depth0(part, "=")[0] for part in _split_depth0(collapsed, ",")]
    if _is_prototype(declarators[0]):
        return []
    return [name for name in map(_declarator_name, declarators) if name]


def allocate_commons(asm: str, order: Sequence[str] = (), *, small: bool = False) -> str:
    """Return maspsx output with the unit's ``.comm`` symbols allocated in its ``.bss``.

    maspsx (``--use-comm-section``) writes each common of the trailing ``.bss``
    block as ``\\t.comm sym,N`` among the statics, in cc1's (first-declaration)
    order. Each becomes ``.align`` / ``.globl sym`` / ``sym:`` / ``.space N``,
    after every static of the block, with :func:`common_alignment_power`. They
    go in ``order`` -- the unit's :func:`definition_order`: a header's
    ``extern`` declares a shared one long before the owner defines it, so cc1's
    order is the headers' -- and any not in it after those, in cc1's order.
    Run it after :func:`align_bss_objects`.
    Output without a ``.bss`` block comes back unchanged. A ``.comm`` in the
    ``.sbss`` block (a common of at most ``-G`` bytes) is allocated there the same
    way when ``small`` (the unit owns its small commons) and otherwise stays
    ``ld``'s; one anywhere else is refused.
    """
    lines = asm.split("\n")
    start = _last_block(lines, _BSS_BLOCK)
    stray = [ln for ln in _outside_small_data(lines if start is None else lines[:start])
             if _COMM.match(ln)]
    if stray:
        raise ValueError(f"a common outside the trailing .bss block: {stray[0].strip()}")
    if start is not None:
        lines = _allocate_block(lines, start, order)
    if small:
        sbss = _last_block(lines, _SBSS_BLOCK)
        if sbss is not None:
            lines = _allocate_block(lines, sbss, order)
    return "\n".join(lines)


def _allocate_block(lines: list[str], start: int, order: Sequence[str]) -> list[str]:
    """``lines`` with the commons of the block opened at ``start`` (up to the next
    ``.section`` or the end) allocated after its statics, in ``order``."""
    end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith(".section")),
               len(lines))
    block = lines[start + 1 : end]
    trailing = 0
    while trailing < len(block) and block[len(block) - 1 - trailing] == "":
        trailing += 1
    body = block[: len(block) - trailing]
    statics = [ln for ln in body if not _COMM.match(ln)]
    commons = [_COMM.match(ln).groups() for ln in body if _COMM.match(ln)]
    if not commons:
        return lines

    rank = {name: i for i, name in reversed(list(enumerate(order)))}
    commons.sort(key=lambda c: rank.get(c[0], len(rank)))
    out = lines[: start + 1] + statics
    for symbol, size in commons:
        power = common_alignment_power(int(size))
        if power:
            out.append(f"\t.align {power}")
        out += [f"\t.globl {symbol}", f"{symbol}:", f"\t.space {size}"]
    return out + [""] * trailing + lines[end:]


def end_bss_on_doubleword(asm: str) -> str:
    """Return maspsx output with its trailing ``.bss`` block padded to a doubleword.

    Run it after :func:`allocate_commons`, whose commons end the block. Output without
    a ``.bss`` block, or with an empty one, comes back unchanged.
    """
    lines = asm.split("\n")
    start = _last_block(lines, _BSS_BLOCK)
    if start is None:
        return asm
    end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith(".section")),
               len(lines))
    last = end
    while last > start + 1 and lines[last - 1] == "":
        last -= 1
    if not any(_SPACE.match(line) for line in lines[start + 1 : last]):
        return asm
    return "\n".join(lines[:last] + ["\t.align 3"] + lines[last:])


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Lay out maspsx output's .bss the way ASPSX and "
                                             "PSYLINK did (stdin -> stdout).")
    ap.add_argument("source", help="the unit's C source (its definition order ranks its commons)")
    ap.add_argument("--own-commons", action="store_true",
                    help="allocate the unit's .bss commons itself and end its .bss on a "
                         "doubleword (a PSYLINK-linked module)")
    ap.add_argument("--small-commons", action="store_true",
                    help="also allocate its .sbss commons (the unit owns them)")
    args = ap.parse_args(argv)
    asm = align_bss_objects(sys.stdin.read())
    if args.own_commons:
        with open(args.source, errors="replace") as f:
            order = definition_order(f.read())
        asm = allocate_commons(asm, order, small=args.small_commons)
        asm = end_bss_on_doubleword(asm)
    sys.stdout.write(asm)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
