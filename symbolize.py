#!/usr/bin/env python3
"""Symbolize LBR snapshot JSON output using addr2line."""

import json
import subprocess
import sys
from typing import Any, Dict, List, Set


def parse_addr2line_output(output: str, addresses: List[str]) -> Dict[str, Dict[str, Any]]:
    """Parse addr2line -f output into a mapping from address to symbol info.

    addr2line -f outputs two lines per address: function name, then file:line.
    """
    lines = output.splitlines()
    result: Dict[str, Dict[str, Any]] = {}

    for i, addr in enumerate(addresses):
        func_line = lines[i * 2] if i * 2 < len(lines) else "??"
        loc_line = lines[i * 2 + 1] if i * 2 + 1 < len(lines) else "??:0"

        if func_line == "??" and loc_line in ("??:0", "??:?"):
            continue

        sym: Dict[str, Any] = {}
        if func_line != "??":
            sym["func"] = func_line
        if loc_line not in ("??:0", "??:?"):
            parts = loc_line.rsplit(":", 1)
            if len(parts) == 2:
                sym["file"] = parts[0]
                try:
                    sym["line"] = int(parts[1])
                except ValueError:
                    sym["line_raw"] = parts[1]
            else:
                sym["file"] = loc_line

        if sym:
            result[addr] = sym

    return result


def symbolize_events(events: List[Dict[str, Any]], binary: str) -> List[Dict[str, Any]]:
    """Resolve addresses in branch records using addr2line."""
    addresses: Set[str] = set()
    for event in events:
        for branch in event.get("branches", []):
            addresses.add(branch["from"])
            addresses.add(branch["to"])

    if not addresses:
        return events

    addr_list = sorted(addresses)

    proc = subprocess.run(
        ["addr2line", "-e", binary, "-f"] + addr_list,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
    )
    symbols = parse_addr2line_output(proc.stdout, addr_list)

    for event in events:
        for branch in event.get("branches", []):
            from_addr = branch["from"]
            to_addr = branch["to"]
            if from_addr in symbols:
                branch["from_sym"] = symbols[from_addr]
            if to_addr in symbols:
                branch["to_sym"] = symbols[to_addr]

    return events


def main() -> None:
    binary = sys.argv[1] if len(sys.argv) > 1 else "./exercise"

    events: List[Dict[str, Any]] = []
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        events.append(json.loads(line))

    events = symbolize_events(events, binary)
    json.dump(events, sys.stdout, indent=2)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main()
