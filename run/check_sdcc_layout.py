#!/usr/bin/env python3
import re
import sys


def read_symbols(path):
    symbols = {}
    pattern = re.compile(r"^\s*([0-9A-Fa-f]+)\s+(s__CODE|l__CODE|s__DATA|l__DATA)\b")
    with open(path, "r", encoding="ascii", errors="ignore") as handle:
        for line in handle:
            match = pattern.match(line)
            if match:
                symbols[match.group(2)] = int(match.group(1), 16)
    return symbols


def main(argv):
    if len(argv) != 3:
        print("usage: check_sdcc_layout.py MAP STACK", file=sys.stderr)
        return 2

    map_path = argv[1]
    stack = int(argv[2], 0)
    symbols = read_symbols(map_path)
    missing = [name for name in ("s__CODE", "l__CODE", "s__DATA", "l__DATA") if name not in symbols]
    if missing:
        print(f"layout: missing symbols in {map_path}: {', '.join(missing)}", file=sys.stderr)
        return 2

    code_start = symbols["s__CODE"]
    code_end = code_start + symbols["l__CODE"]
    data_start = symbols["s__DATA"]
    data_end = data_start + symbols["l__DATA"]

    print(
        "layout: CODE %04X-%04X, DATA %04X-%04X, STACK %04X"
        % (code_start, code_end, data_start, data_end, stack)
    )

    if code_end > data_start:
        print("layout: CODE overlaps DATA", file=sys.stderr)
        return 1
    if data_end >= stack:
        print("layout: DATA reaches stack", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
