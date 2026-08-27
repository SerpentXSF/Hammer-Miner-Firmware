# Hammer Miner Firmware

Open-source firmware for Hammer BC-series Bitcoin miners — BC01, BC02,
BC04, BC06, and BC08 — built on the ESP32-S3.

This is a continuation of the BC04 firmware source that Chengdu Baichuan
published for Hammer under GPL obligation, restored to something that is
actually usable: correctly licensed and attributed, buildable entirely
from source, with the shipped security defects fixed and the BC01 support
that the vendor release omitted put back.

> **Licensing.** This firmware derives from
> [ESP-Miner](https://github.com/bitaxeorg/ESP-Miner), which is GPL-3.0.
> So is this. See [NOTICE.md](NOTICE.md) for attribution and
> [LICENSE](LICENSE) for terms.

> **Security.** Stock Hammer firmware has **no authentication on any HTTP
> endpoint** and accepts **unsigned firmware from any host on your LAN**.
> If you own one of these miners, read
> [docs/SECURITY.md](docs/SECURITY.md) — the mitigations apply whether or
> not you flash this firmware.

---

## Why this repository exists

Hammer sold a commercial Bitcoin miner running firmware built on
ESP-Miner, a GPL-3.0 project, and shipped it closed-source. After what
their README calls an internal compliance audit, they published the BC04
source at [`baichuan-org/BC04`](https://github.com/baichuan-org/BC04).

That release does not finish the job:

- **No license file. No attribution.** Not a single copyright notice
  crediting ESP-Miner anywhere in the tree, though 4,367 lines are still
  identical to it and six files match byte for byte.
- **A binary blob with no source.** `components/a/liba.a` is first-party
  product code, compiled and linked into the firmware, with only a header
  published. GPL-3.0 section 6 means all of the corresponding source.
- **The BC01 release is empty.** `BC01-APP-2.0.3-20260625.zip` is 392
  bytes containing a two-line README and nothing else — while the BC01
  ships the same codebase, as its own firmware binary proves.
- **Explicitly unmaintained.** The vendor states they will provide no
  support or further development.

The evidence for each of these, and how to reproduce it, is in
[docs/PROVENANCE.md](docs/PROVENANCE.md).

None of this is asserted on the vendor's word. It is measured from the
two archives they published and from a shipping firmware binary.

## What this repository does about it

| | |
|---|---|
| **Licensing** | Full GPL-3.0 text, upstream attribution, and a documented provenance chain. |
| **Blob removal** | `liba.a` reconstructed from source so the tree builds with no binary components — see [docs/ASIC-ABSTRACTION.md](docs/ASIC-ABSTRACTION.md). |
| **Security** | Real authentication, the OTA error-path bug fixed, shipped credentials removed. See [docs/SECURITY.md](docs/SECURITY.md). |
| **BC01 support** | The stripped BC01 code paths restored and verified on hardware. |
| **Tooling** | OTA image pack/unpack, upstream diffing, provisioning. |

The first commit in this repository is the vendor's tree, unmodified.
Every change since is visible as a diff against exactly what they
released.

---

## Hardware

| Model | ASICs | ASIC | Notes |
|---|---|---|---|
| BC01 | 1 | BM1370 | Single-chip, 5 nm Bitmain BM1370, ESP32-S3 |
| BC02 | 2 | BM1370 | |
| BC04 | 4 | BM1370 | |
| BC06 | 6 | BM1370 | Dual thermal sensor |
| BC08 | 8 | BM1370 | Dual thermal sensor |

Common to the family: ESP32-S3 host, TPS546 buck converter for the ASIC
core domain, TMP75 I²C thermal sensing, EMC2302 fan control, and an LVGL
display. An LT0051 scrypt ASIC path also exists in the tree.

---

## Building

Requires **ESP-IDF 5.5.1** or later.

```bash
git clone https://github.com/SerpentXSF/Hammer-Miner-Firmware.git
cd Hammer-Miner-Firmware
idf.py set-target esp32s3
idf.py build
```

The web UI is built separately and flashed to the `www` partition:

```bash
cd main/http_server/axe-os
npm install
npm run build
```

### Provisioning

`config.cvs` is a template. It contains no credentials and no payout
address — fill in your own before flashing, or the build will stop.

```bash
cp config.cvs.example config.cvs
$EDITOR config.cvs           # set stratumuser to YOUR address
./merge_bin.sh -c hammer-miner-all.bin
```

### Flashing

Full image over USB, for a blank or bricked device:

```bash
esptool.py --chip esp32s3 write_flash 0x0 hammer-miner-all.bin
```

Update over the network, for a working device:

```bash
python tools/ota_tool.py pack build/hammer-miner.bin -o update.bin --key key.bin
curl -X POST --data-binary @update.bin http://<miner>/api/system/OTA
```

---

## Tools

| Tool | Purpose |
|---|---|
| [`tools/ota_tool.py`](tools/ota_tool.py) | Inspect, unpack, and build OTA update images. |
| [`tools/compare_upstream.py`](tools/compare_upstream.py) | Measure how much of this tree is still ESP-Miner. |

---

## Documentation

- [docs/PROVENANCE.md](docs/PROVENANCE.md) — where this code came from, with evidence
- [docs/SECURITY.md](docs/SECURITY.md) — findings, fixes, and mitigations for stock firmware
- [docs/OTA-FORMAT.md](docs/OTA-FORMAT.md) — the update container, and why its obfuscation is not encryption
- [docs/ASIC-ABSTRACTION.md](docs/ASIC-ABSTRACTION.md) — reconstructing the withheld blob

---

## Relationship to Hammer

None. This is an independent continuation by owners of the hardware,
not affiliated with or endorsed by Hammer or Chengdu Baichuan. It is not
related to Hammer's own later firmware — THOR OS, NORN OS, or GLOD OS —
which the vendor states were developed separately.

Use of the Hammer and BC model names is descriptive, to identify the
hardware this firmware runs on.

## Contributing

Issues and pull requests welcome. Contributions are accepted under
GPL-3.0.

If you find further upstream code that is still unattributed, please open
an issue — getting the credit right is the point of this repository.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md).

Copyright of the upstream portions remains with the ESP-Miner, NerdQAxe+,
and LVGL authors.
