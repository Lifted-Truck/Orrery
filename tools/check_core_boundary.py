#!/usr/bin/env python3
"""Framework-isolation gate: shell/core (and the engines) must contain no JUCE.

The doctrine boundary (pure, framework-free cores; UI/IO/time in thin adapters):
JUCE may appear ONLY under shell/plugin. A hit anywhere in shell/core/ is a
violation — the fix is to move the dependency into the plugin shell or replace
it, never to exempt the file. Runs as a ctest in the core (fast) build, so it
needs no compiler and no network.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCAN_DIRS = ["shell/core", "engines"]  # engines are framework-free too

# The one sanctioned exemption (DECISIONS #16): engines/<name>/gui/ is each
# engine's VIEW adapter — the organ owns its visualization, and views are JUCE
# by nature (they compile only in the plugin build). The engine CORE (model/
# src/include/tests) stays framework-free and fully scanned. shell/core has NO
# exemption.
def exempt(path: Path) -> bool:
    rel = path.relative_to(ROOT)
    return rel.parts[0] == "engines" and "gui" in rel.parts
PATTERNS = [
    re.compile(r"#\s*include\s*[<\"]juce", re.IGNORECASE),
    re.compile(r"\bjuce::"),
    re.compile(r"\bJUCE_[A-Z_]+\b"),
]
EXTS = {".h", ".hpp", ".c", ".cpp", ".cc", ".mm", ".cmake", ".txt"}


def main() -> int:
    violations = []
    for d in SCAN_DIRS:
        base = ROOT / d
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file() or path.suffix not in EXTS:
                continue
            if exempt(path):
                continue
            for lineno, line in enumerate(
                    path.read_text(errors="replace").splitlines(), 1):
                for pat in PATTERNS:
                    if pat.search(line):
                        violations.append(
                            f"{path.relative_to(ROOT)}:{lineno}: {line.strip()}")

    if violations:
        print("FAIL: JUCE reference(s) inside a framework-free core:")
        for v in violations:
            print("  " + v)
        return 1
    print("ok: shell/core is framework-free")
    return 0


if __name__ == "__main__":
    sys.exit(main())
