#!/usr/bin/env python3
"""Measure how much of this tree is still upstream ESP-Miner.

Matches source files by basename against an ESP-Miner checkout and reports
line-sequence similarity for each pair. Regenerates docs/upstream-similarity.txt.

    git clone --depth 50 https://github.com/bitaxeorg/ESP-Miner.git
    python tools/compare_upstream.py --upstream ESP-Miner --tree .
"""

import argparse
import difflib
import os
import sys

SKIP_DIRS = {
    ".git", "node_modules", "build", "managed_components",
    "lvgl__lvgl", "images", "dist", "test-results", "playwright-report",
}
EXTS = (".c", ".h")
# Below this ratio a pair is treated as an unrelated same-named file.
MIN_RATIO = 0.15


def collect(root):
    """Map basename -> [paths], skipping vendored and generated trees."""
    found = {}
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        for name in filenames:
            if name.endswith(EXTS):
                found.setdefault(name, []).append(os.path.join(dirpath, name))
    return found


def read_lines(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as handle:
            return handle.read().splitlines()
    except OSError:
        return []


def relative(path, root):
    return os.path.relpath(path, root).replace(os.sep, "/")


def best_match(local_path, candidates):
    """Pick the upstream candidate this file most resembles."""
    best = None
    local = read_lines(local_path)
    if not local:
        return None
    for candidate in candidates:
        upstream = read_lines(candidate)
        if not upstream:
            continue
        matcher = difflib.SequenceMatcher(None, local, upstream)
        if matcher.quick_ratio() < 0.3:
            continue
        ratio = matcher.ratio()
        identical = sum(block.size for block in matcher.get_matching_blocks())
        if best is None or ratio > best[0]:
            best = (ratio, candidate, len(local), len(upstream), identical)
    return best


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", required=True, help="path to an ESP-Miner checkout")
    parser.add_argument("--tree", default=".", help="path to this firmware tree")
    args = parser.parse_args()

    if not os.path.isdir(args.upstream):
        sys.exit(f"upstream checkout not found: {args.upstream}")

    local_files = collect(args.tree)
    upstream_files = collect(args.upstream)

    rows = []
    for name, paths in sorted(local_files.items()):
        if name not in upstream_files:
            continue
        for path in paths:
            match = best_match(path, upstream_files[name])
            if match and match[0] > MIN_RATIO:
                rows.append((
                    match[0],
                    relative(path, args.tree),
                    relative(match[1], args.upstream),
                    match[2], match[3], match[4],
                ))

    rows.sort(reverse=True)
    header = f"{'sim':>6}  {'local file':<46} {'ESP-Miner file':<42} {'loc':>5} {'up':>5} {'same':>5}"
    print(header)
    print("-" * len(header))

    identical_total = local_total = 0
    for ratio, local, upstream, n_local, n_upstream, identical in rows:
        print(f"{ratio * 100:5.1f}%  {local:<46} {upstream:<42} "
              f"{n_local:>5} {n_upstream:>5} {identical:>5}")
        identical_total += identical
        local_total += n_local

    print("-" * len(header))
    print(f"{len(rows)} files matched upstream; "
          f"{identical_total} identical lines of {local_total}")


if __name__ == "__main__":
    main()
