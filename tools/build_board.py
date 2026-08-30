"""Build the firmware for one board, into that board's own directory.

    python tools/build_board.py bc01
    python tools/build_board.py bc04 flash monitor

Boards differ in their ASIC UART pins and nothing warns you when they are
wrong: the miner boots, serves its interface, detects the chip and never
returns a share. Keeping one build directory per board means a BC01 image and
a BC04 image cannot overwrite each other, and the artifact that comes out
carries the board in its name.

Everything not listed in boards/<board>.defaults comes from the base sdkconfig,
so the two builds differ only where the hardware does.
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOARD_DIR = os.path.join(ROOT, "boards")


def boards():
    if not os.path.isdir(BOARD_DIR):
        return []
    return sorted(f[:-len(".defaults")] for f in os.listdir(BOARD_DIR)
                  if f.endswith(".defaults"))


def main(argv):
    known = boards()
    if len(argv) < 2 or argv[1] not in known:
        sys.stderr.write("usage: build_board.py <%s> [idf.py args...]\n"
                         % "|".join(known or ["<none found>"]))
        return 2

    board = argv[1]
    targets = argv[2:] or ["build"]

    build_dir = os.path.join(ROOT, "build", board)
    fragment = os.path.join(BOARD_DIR, board + ".defaults")
    base = os.path.join(ROOT, "sdkconfig")

    # The base config carries the tuning that is not board specific -- PSRAM,
    # partition layout, log levels. The fragment is applied over it, so a board
    # file only ever states what that board changes.
    defaults = base + ";" + fragment if os.path.exists(base) else fragment

    # ESP-IDF applies SDKCONFIG_DEFAULTS only when it has to create the config,
    # so once build/<board>/sdkconfig exists it silently wins and later edits to
    # the base file or the board fragment do not reach the build. Regenerate it
    # whenever either input is newer -- the two source files are the authority,
    # and anything set by hand in the generated copy is meant to be disposable.
    generated = os.path.join(build_dir, "sdkconfig")
    if os.path.exists(generated):
        newest_input = max(os.path.getmtime(f)
                           for f in (base, fragment) if os.path.exists(f))
        if newest_input > os.path.getmtime(generated):
            print("config inputs changed; regenerating %s" % generated)
            os.remove(generated)

    # Run idf.py through the interpreter rather than as a bare command: the
    # defaults list is semicolon separated and the paths here contain spaces,
    # which a shell gets wrong on both counts.
    idf_path = os.environ.get("IDF_PATH")
    if idf_path:
        launcher = [sys.executable, os.path.join(idf_path, "tools", "idf.py")]
    else:
        launcher = ["idf.py"]

    cmd = launcher + [
        "-B", build_dir,
        "-DSDKCONFIG_DEFAULTS=" + defaults,
        "-DSDKCONFIG=" + os.path.join(build_dir, "sdkconfig")] + targets

    print("building %s -> %s" % (board, build_dir))

    return subprocess.call(cmd, cwd=ROOT)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
