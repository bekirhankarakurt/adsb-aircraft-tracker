#!/usr/bin/env python3
"""Turn a gcovr --json-summary file into a shields.io endpoint JSON document."""
import json
import sys


def colour(percent: float) -> str:
    for threshold, name in ((90, "brightgreen"), (80, "green"), (70, "yellowgreen"),
                            (60, "yellow"), (50, "orange")):
        if percent >= threshold:
            return name
    return "red"


def main() -> int:
    with open(sys.argv[1], encoding="utf-8") as f:
        percent = float(json.load(f)["line_percent"])
    print(json.dumps({"schemaVersion": 1, "label": "coverage",
                      "message": f"{percent:.0f}%", "color": colour(percent)}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
