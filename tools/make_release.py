#!/usr/bin/env python3
"""Build the artifacts published for other people to flash.

Produces, into dist/:

    serpentx-bc01-<ver>-full.bin    bootloader + tables + app + web UI
    serpentx-bc01-<ver>-app.bin     application only, for esptool at 0x20000
    serpentx-bc01-<ver>-www.bin     web UI only, for esptool at 0x9e0000
    serpentx-bc01-<ver>-ota.bin     wrapped for the miner's own updater
    manifest.json                   for the esp-web-tools flasher
    SHA256SUMS

The NVS image inside the full build is generated from **config.cvs.example**,
never from a local config.cvs. That distinction matters: config.cvs is flashed
verbatim and holds WiFi credentials and a payout address, so shipping one would
hand out the builder's network password and point other people's hashrate at
the builder's wallet. The published image therefore has no credentials in it,
and a freshly flashed miner comes up as an access point waiting to be told
where to mine.

Run after `idf.py build`, with the ESP-IDF environment exported.
"""

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build")
DIST = os.path.join(ROOT, "dist")

# offset, file — must match partitions.csv
LAYOUT = [
    (0x0, os.path.join(BUILD, "bootloader", "bootloader.bin")),
    (0xD000, os.path.join(BUILD, "partition_table", "partition-table.bin")),
    (0xE000, os.path.join(DIST, "config-dist.bin")),
    (0x16000, os.path.join(BUILD, "ota_data_initial.bin")),
    (0x20000, os.path.join(BUILD, "hammer-miner.bin")),
    (0x9E0000, os.path.join(BUILD, "www.bin")),
]


def version():
    sdk = os.path.join(ROOT, "sdkconfig")
    with open(sdk, encoding="utf-8", errors="replace") as fh:
        m = re.search(r'^CONFIG_APP_PROJECT_VER="([^"]+)"', fh.read(), re.M)
    return (m.group(1).split()[0] if m else "0.0.0")


def sh(cmd):
    print("  $", " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=ROOT)


def build_config():
    """NVS image from the example template, never from a local config.cvs."""
    idf = os.environ.get("IDF_PATH")
    if not idf:
        sys.exit("IDF_PATH is not set -- export the ESP-IDF environment first")
    gen = os.path.join(idf, "components", "nvs_flash",
                       "nvs_partition_generator", "nvs_partition_gen.py")
    src = os.path.join(ROOT, "config.cvs.example")
    out = os.path.join(DIST, "config-dist.bin")

    text = open(src, encoding="utf-8", errors="replace").read()
    if "REPLACE-WITH-YOUR-BTC-ADDRESS" not in text:
        sys.exit("config.cvs.example has been edited to contain a real "
                 "address; refusing to publish it")
    sh([sys.executable, gen, "generate", src, out, "0x6000"])


def main():
    ver = version()
    os.makedirs(DIST, exist_ok=True)
    print("building release artifacts for %s" % ver)

    build_config()

    for _, path in LAYOUT:
        if not os.path.exists(path):
            sys.exit("missing %s -- run idf.py build first" % path)

    base = "serpentx-bc01-%s" % ver
    full = os.path.join(DIST, base + "-full.bin")

    args = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "merge_bin",
            "-o", full, "--flash_mode", "dio", "--flash_size", "16MB",
            "--flash_freq", "80m"]
    for off, path in LAYOUT:
        args += [hex(off), path]
    sh(args)

    shutil.copy(os.path.join(BUILD, "hammer-miner.bin"),
                os.path.join(DIST, base + "-app.bin"))
    shutil.copy(os.path.join(BUILD, "www.bin"),
                os.path.join(DIST, base + "-www.bin"))

    # Image the miner's own updater accepts. The container is obfuscated with
    # a repeating 16-byte XOR pad; this is the vendor's, recovered from a
    # shipped image without the key (docs/OTA-FORMAT.md). It is not a secret
    # and protects nothing -- it is here because the firmware's updater expects
    # the format, so an update built any other way is rejected.
    sh([sys.executable, os.path.join(ROOT, "tools", "ota_tool.py"), "pack",
        os.path.join(BUILD, "hammer-miner.bin"),
        "--pad", "69cc74aeaf0ce683229d422f54428a54",
        "-o", os.path.join(DIST, base + "-ota.bin")])

    manifest = {
        "name": "SerpentX / Stay Open",
        "version": ver,
        "home_assistant_domain": None,
        "new_install_prompt_erase": False,
        "builds": [{
            "chipFamily": "ESP32-S3",
            "parts": [{"path": base + "-full.bin", "offset": 0}],
        }],
    }
    with open(os.path.join(DIST, "manifest.json"), "w",
              encoding="utf-8", newline="\n") as fh:
        json.dump(manifest, fh, indent=2)
        fh.write("\n")

    lines = []
    for name in sorted(os.listdir(DIST)):
        if name in ("SHA256SUMS", "config-dist.bin") or not name.endswith(
                (".bin", ".json")):
            continue
        with open(os.path.join(DIST, name), "rb") as fh:
            lines.append("%s  %s\n" % (hashlib.sha256(fh.read()).hexdigest(),
                                       name))
    with open(os.path.join(DIST, "SHA256SUMS"), "w",
              encoding="utf-8", newline="\n") as fh:
        fh.writelines(lines)

    os.remove(os.path.join(DIST, "config-dist.bin"))
    print("\ndist/ contains:")
    for line in lines:
        print("  " + line.strip())


if __name__ == "__main__":
    main()
