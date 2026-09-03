#!/usr/bin/env python3
"""Build, release and publish every board in one step, at one version.

    python tools/ship.py bc01 bc04            # build everything, publish nothing
    python tools/ship.py bc01 bc04 --push     # ...and push the flasher
    python tools/ship.py bc01 bc04 --push --github   # ...and cut the release

Why this exists
---------------
Building, packaging, publishing the flasher and cutting a GitHub release were
four separate commands, and nothing checked that they agreed. They drifted:
the web flasher served 2.0.15 while the newest GitHub release was 2.0.14 and
carried BC01 files only, so a BC04 owner reading the releases page would
conclude the board was unsupported while the flasher was already handing out a
working image for it. Someone who flashed from the browser got a version that
did not exist anywhere else.

Running the steps from one place, over all the boards at once, is what keeps
them in step. The version is not passed in -- it comes from the build, so the
flasher and the release cannot describe different things.

This does not bump the version. Edit CONFIG_APP_PROJECT_VER in sdkconfig first;
publishing different content under a version that has already been released is
the thing that makes a version number worthless.
"""

import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, "dist")


def run(args, **kw):
    print("  $ " + " ".join(str(a) for a in args))
    subprocess.run(args, cwd=ROOT, check=True, **kw)


def built_version(board):
    """The version the build actually produced, not one passed in."""
    sdk = os.path.join(ROOT, "build", board, "sdkconfig")
    if not os.path.exists(sdk):
        sys.exit("no build for %s -- nothing to read a version from" % board)
    with open(sdk, encoding="utf-8", errors="replace") as fh:
        m = re.search(r'^CONFIG_APP_PROJECT_VER="([^"]+)"', fh.read(), re.M)
    if not m:
        sys.exit("no CONFIG_APP_PROJECT_VER in %s" % sdk)
    return m.group(1).split()[0]


def released_tags():
    """Tags that already have a release, or None if gh cannot answer.

    Asked as JSON rather than parsed out of the table: `gh release list` prints
    the title first and the tag third, so reading column zero silently returned
    a set of titles and would have let an already-published version be released
    a second time -- the exact thing this check exists to stop.
    """
    out = subprocess.run(["gh", "release", "list", "--limit", "200",
                          "--json", "tagName"],
                         cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if out.returncode != 0:
        return None
    import json
    return {r["tagName"] for r in json.loads(out.stdout.decode(errors="replace"))}


def main():
    boards = [a.lower() for a in sys.argv[1:] if not a.startswith("-")]
    if not boards:
        sys.exit("usage: ship.py <board> [board...] [--push] [--github]")
    push = "--push" in sys.argv
    github = "--github" in sys.argv

    for board in boards:
        print("\n=== building %s ===" % board)
        run([sys.executable, os.path.join("tools", "build_board.py"), board])

    # One version across every board, or the flasher would offer a choice
    # between two different releases wearing one name.
    versions = {b: built_version(b) for b in boards}
    if len(set(versions.values())) != 1:
        sys.exit("boards built at different versions: %s -- rebuild them all"
                 % ", ".join("%s %s" % kv for kv in sorted(versions.items())))
    version = next(iter(versions.values()))
    print("\nall boards at %s" % version)

    for board in boards:
        print("\n=== packaging %s ===" % board)
        run([sys.executable, os.path.join("tools", "make_release.py"), board])

    print("\n=== flasher ===")
    args = [sys.executable, os.path.join("tools", "publish_flasher.py")] + boards
    if push:
        args.append("--push")
    run(args)

    tag = "v" + version
    if github:
        existing = released_tags()
        if existing is None:
            sys.exit("gh is not available or not logged in; cannot cut a release")
        if tag in existing:
            sys.exit("%s is already released. Bump CONFIG_APP_PROJECT_VER rather "
                     "than replacing a published release's files." % tag)

        assets = sorted(f for f in os.listdir(DIST)
                        if f.endswith(".bin") or f == "SHA256SUMS")
        for board in boards:
            if not any(("-%s-" % board) in a for a in assets):
                sys.exit("no %s artifacts in dist/ -- refusing to cut a release "
                         "that omits a board it claims to cover" % board)

        print("\n=== github release %s ===" % tag)
        title = "Stay Open %s - %s" % (
            version, " and ".join(b.upper() for b in boards))
        run(["gh", "release", "create", tag, "--title", title, "--notes",
             "See the repository README and docs/ for what changed."]
            + [os.path.join("dist", a) for a in assets])
    elif push:
        print("\nFlasher pushed at %s. The GitHub release was NOT cut -- re-run "
              "with --github, or the releases page will lag the flasher." % tag)

    print("\ndone: %s, boards %s" % (version, ", ".join(boards)))


if __name__ == "__main__":
    main()
