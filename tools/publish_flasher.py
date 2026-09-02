"""Publish the web flasher to the gh-pages branch.

    python tools/publish_flasher.py bc01          # build the branch
    python tools/publish_flasher.py bc01 --push   # and force-push it

The flasher has to serve the firmware image from the same origin as the page:
esp-web-tools fetches it with XHR, and GitHub release assets do not send CORS
headers, so a release URL cannot be used. That means a 12 MB binary has to live
somewhere Pages can serve.

Keeping it on main put a new 12 MB blob in the project's permanent history at
every release. Here each publish writes gh-pages as a single **orphan** commit
with no parent, so the previous image is not referenced by anything afterwards
and the branch never accumulates. gh-pages is generated output, not source --
it is meant to be replaced wholesale, which is why the push is a force.

The branch is built with git plumbing rather than by checking anything out, so
this never touches the working tree, the index, or the current branch.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "docs", "flasher")
DIST = os.path.join(ROOT, "dist")
BRANCH = "gh-pages"

REDIRECT = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>SerpentX | Stay Open</title>
<meta http-equiv="refresh" content="0; url=flasher/">
<link rel="canonical" href="flasher/">
</head>
<body>
<p>Go to the <a href="flasher/">firmware flasher</a>.</p>
</body>
</html>
"""


def git(*args, data=None):
    out = subprocess.run(["git"] + list(args), cwd=ROOT, input=data,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if out.returncode != 0:
        sys.exit("git %s failed: %s" % (" ".join(args),
                                        out.stderr.decode(errors="replace")))
    return out.stdout.decode().strip()


def blob(path):
    """Write a file into the object store and return its sha."""
    return git("hash-object", "-w", "--", path)


def blob_bytes(data, name):
    """Write bytes into the object store via a temp file, return its sha."""
    tmp = os.path.join(DIST, ".publish-tmp-" + name)
    os.makedirs(DIST, exist_ok=True)
    with open(tmp, "wb") as fh:
        fh.write(data)
    try:
        return blob(tmp)
    finally:
        os.remove(tmp)


def mktree(entries):
    """entries: list of (mode, type, sha, name)."""
    spec = "".join("%s %s %s\t%s\n" % e for e in entries)
    return git("mktree", data=spec.encode())


def main():
    board = (sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("-")
             else "bc01").lower()
    push = "--push" in sys.argv

    if not os.path.isdir(SRC):
        sys.exit("no %s to publish" % SRC)

    # The image is taken from dist/, which make_release.py fills, so the branch
    # always carries an artifact that was actually built for a release rather
    # than whatever happens to be lying in docs/.
    images = [f for f in os.listdir(DIST) if f.startswith("stay-open-%s-" % board)
              and f.endswith("-full.bin")] if os.path.isdir(DIST) else []
    if len(images) != 1:
        sys.exit("expected exactly one stay-open-%s-*-full.bin in dist/, found %d"
                 " -- run tools/make_release.py %s first"
                 % (board, len(images), board))
    image = images[0]

    flasher = []
    for name in sorted(os.listdir(SRC)):
        full = os.path.join(SRC, name)
        if os.path.isfile(full) and not name.endswith(".bin"):
            flasher.append(("100644", "blob", blob(full), name))
    flasher.append(("100644", "blob", blob(os.path.join(DIST, image)), image))

    root = [
        ("100644", "blob", blob_bytes(b"", "nojekyll"), ".nojekyll"),
        ("100644", "blob", blob_bytes(REDIRECT.encode(), "index"), "index.html"),
        ("040000", "tree", mktree(flasher), "flasher"),
    ]

    tree = mktree(root)
    commit = git("commit-tree", tree, "-m",
                 "Publish flasher for %s (%s)" % (board, image))
    git("update-ref", "refs/heads/" + BRANCH, commit)

    print("built %s as a single orphan commit %s" % (BRANCH, commit[:12]))
    for mode, _, _, name in root:
        print("   ", name + ("/" if mode == "040000" else ""))
    for _, _, _, name in flasher:
        print("      flasher/" + name)

    if push:
        # Force: the branch is regenerated wholesale every time, and replacing
        # it is what stops old images being referenced.
        git("push", "--force", "origin", BRANCH)
        print("\npushed %s" % BRANCH)
    else:
        print("\nnot pushed. re-run with --push")


if __name__ == "__main__":
    main()
