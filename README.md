# Hammer Miner Firmware

Open-source firmware for Hammer BC-series Bitcoin miners â€” BC01, BC02,
BC04, BC06, and BC08 â€” built on the ESP32-S3.

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
> endpoint**. Any host on your network can read the configuration, change
> the payout address, or start a firmware update. If you own one of these
> miners, read [docs/SECURITY.md](docs/SECURITY.md) â€” the mitigations
> apply whether or not you ever flash this firmware.

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
  bytes containing a two-line README and nothing else â€” while the BC01
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
| **Blob removal** | BM1370 builds â€” every BC board â€” no longer link the `liba.a` binary blob. The rest of it serves the LT0051 scrypt ASIC and is still required for those builds; see [docs/ASIC-ABSTRACTION.md](docs/ASIC-ABSTRACTION.md). |
| **Security** | Real authentication, the OTA error-path bug fixed, shipped credentials removed. See [docs/SECURITY.md](docs/SECURITY.md). |
| **BC01 support** | The stripped BC01 and BC02 code paths restored, and the USB-PD stage the BC04 release omitted merged in from the vendor's BC01 tree. Brings a BC01 up on hardware — see [Status](#status). |
| **Tooling** | OTA image pack/unpack, upstream diffing, provisioning. |

The first commit in this repository is the vendor's tree, unmodified.
Every change since is visible as a diff against exactly what they
released.

---

## Status

| | |
|---|---|
| Builds from source | Yes, ESP-IDF 5.5.1, no warnings from project sources |
| Web UI builds | Yes, typechecked |
| GPL compliance restored | Yes |
| OTA tooling verified | Yes â€” round-trips the vendor's own image byte for byte |
| BM1370 path free of the binary blob | Yes |
| LT0051 path free of the binary blob | No â€” see [docs/ASIC-ABSTRACTION.md](docs/ASIC-ABSTRACTION.md) |
| **Run on real hardware** | **Yes**, on a BC01 with a replacement LilyGO T-Display-S3 module. Boots, negotiates USB-PD, brings up the regulator and fan, detects the BM1370, ramps to 750 MHz, and connects to a pool. See [docs/BC01-BRINGUP.md](docs/BC01-BRINGUP.md) |
| **Hashing on a BC01** | **Yes** — 1.71 TH/s at 750 MHz, 0 hardware errors, shares accepted, 24 W, 54 C. `ASIC_send_work()` had no BC01 case, so no work ever reached the ASIC; see [docs/BC01-BRINGUP.md](docs/BC01-BRINGUP.md) |

Treat this as reviewed, building, and field-tested on a BC01: it boots, negotiates USB-PD, brings up the regulator and fan, drives the BM1370, and mines.

This firmware descends from work given away by other people first —
HAN, NerdMiner, Bitaxe and ESP-Miner, and the Open Source Miners United
community. They are named in [docs/CREDITS.md](docs/CREDITS.md).

**Check your device before flashing anything.** On a retail BC01 measured
here, Secure Boot is enforced and both spare key slots are revoked, so the
stock module will only ever boot Hammer-signed firmware. eFuses are
one-way. The one-line check is in [docs/SECURE-BOOT.md](docs/SECURE-BOOT.md).

That does not put this firmware out of reach. The ESP32-S3 is a socketed,
off-the-shelf **LilyGO T-Display-S3**, and Secure Boot lives in that
module's eFuses â€” so a fresh module runs this firmware with no exploit
involved. The per-unit ASIC calibration sits in the hashboard EEPROM, not
on the module, so it survives the swap. It is reversible: keep the original
and the miner goes back to stock in a minute.
[docs/HARDWARE-SWAP.md](docs/HARDWARE-SWAP.md) covers it.

Either way, [docs/SECURITY.md](docs/SECURITY.md) applies â€” the missing
authentication affects locked-down units exactly as much as open ones.

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
core domain, TMP75 IÂ²C thermal sensing, EMC2302 fan control, and an LVGL
display. An LT0051 scrypt ASIC path also exists in the tree.

---

## Installing

Three routes, in order of how little you need on your machine.

| | |
|---|---|
| **Browser** | [Web flasher](https://serpentxsf.github.io/StayOpen-Miner-Firmware/flasher/) — Chrome or Edge on a desktop, nothing to install |
| **esptool** | [Latest release](https://github.com/SerpentXSF/StayOpen-Miner-Firmware/releases/latest) — full, app-only, web-UI-only and OTA images, with `SHA256SUMS` |
| **From source** | [Building](#building) below, ESP-IDF 5.5.1 |

**Read [the eFuse warning](#status) before flashing anything.** On a retail BC01
with Secure Boot burned, this firmware will not boot and cannot be recovered.

The published images carry **no credentials**. Their settings partition is
generated from `config.cvs.example`, so a freshly flashed miner starts its own
access point and waits for your network, your pool and your own payout address.
Nothing mines to anyone else. `tools/make_release.py` builds the artifacts and
refuses to run if that template has been edited to hold a real address.

To update a miner already running this firmware, upload the `-ota.bin` through
its own update page; that keeps your settings.

## Building

Requires **ESP-IDF 5.5.1** or later.

```bash
git clone https://github.com/SerpentXSF/StayOpen-Miner-Firmware.git
cd StayOpen-Miner-Firmware
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
address â€” fill in your own before flashing, or the build will stop.

```bash
cp config.cvs.example config.cvs
$EDITOR config.cvs           # set stratumuser to YOUR address
./merge_bin.sh -c stayopen-miner-all.bin
```

### Flashing

> **Check your device first.** Signing an image and enforcing that
> signature are different things. If Secure Boot eFuses were burned at
> manufacture, this firmware **will not boot and cannot be recovered** â€”
> eFuses are permanent and USB access does not help. Ask the device:
>
> ```bash
> esptool.py --port COM3 get_security_info
> ```
>
> If `secure_boot_en` is set, do not flash. See
> [docs/SECURE-BOOT.md](docs/SECURE-BOOT.md).

Full image over USB, for a blank device or one you want to start clean:

```bash
esptool.py --chip esp32s3 write_flash 0x0 stayopen-miner-all.bin
```

Update over the network, for a working device. The container format is
unchanged, so this installs the same way vendor images do:

```bash
python tools/ota_tool.py pack build/stayopen-miner.bin -o update.bin \
    --pad 69cc74aeaf0ce683229d422f54428a54
curl -X POST --data-binary @update.bin \
     -H "Authorization: Bearer $TOKEN" \
     http://<miner>/api/system/OTA
```

Get `$TOKEN` from the login endpoint, or omit the header on a device with
no password set:

```bash
curl -s -X POST http://<miner>/api/system/login \
     -H 'Content-Type: application/json' \
     -d '{"password":"your-password"}'
```

Inspect any image, vendor or your own, before installing it:

```bash
python tools/ota_tool.py inspect update.bin
```

---

## Tools

| Tool | Purpose |
|---|---|
| [`tools/ota_tool.py`](tools/ota_tool.py) | Inspect, unpack, and build OTA update images. |
| [`tools/compare_upstream.py`](tools/compare_upstream.py) | Measure how much of this tree is still ESP-Miner. |
| [`tools/dwarf_dump.py`](tools/dwarf_dump.py) | Read the debug information left in `components/a/liba.a`. |
| [`tools/espimg.py`](tools/espimg.py) | Map an ESP32 image and cross-reference its literal pools, to find the code that uses a given string. |
| [`tools/xtensa_dis.py`](tools/xtensa_dis.py) | Small Xtensa disassembler, enough to read constants out of a driver. |

---

## Documentation

- [docs/PROVENANCE.md](docs/PROVENANCE.md) â€” where this code came from, with evidence
- [docs/SECURITY.md](docs/SECURITY.md) â€” findings, fixes, and mitigations for stock firmware
- [docs/OTA-FORMAT.md](docs/OTA-FORMAT.md) â€” the update container, and why its obfuscation is not encryption
- [docs/AUTH.md](docs/AUTH.md) â€” how API authentication works, and what it does not cover
- [docs/SECURE-BOOT.md](docs/SECURE-BOOT.md) â€” check before flashing; some of it is irreversible
- [docs/HARDWARE-SWAP.md](docs/HARDWARE-SWAP.md) â€” running this on a locked-down retail miner, without an exploit
- [docs/ASIC-ABSTRACTION.md](docs/ASIC-ABSTRACTION.md) â€” the withheld blob, and what replaced it

---

## Relationship to Hammer

None. This is an independent continuation by owners of the hardware,
not affiliated with or endorsed by Hammer or Chengdu Baichuan. It is not
related to Hammer's own later firmware â€” THOR OS, NORN OS, or GLOD OS â€”
which the vendor states were developed separately.

Use of the Hammer and BC model names is descriptive, to identify the
hardware this firmware runs on.

## Contributing

Issues and pull requests welcome. Contributions are accepted under
GPL-3.0.

If you find further upstream code that is still unattributed, please open
an issue â€” getting the credit right is the point of this repository.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md).

Copyright of the upstream portions remains with the ESP-Miner, NerdQAxe+,
and LVGL authors.
