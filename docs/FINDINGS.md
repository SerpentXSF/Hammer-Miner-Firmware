# Findings

What was found taking a retail Hammer BC01 apart, building its firmware from
source, and getting it to mine. Everything here was measured on hardware or
read out of a shipped binary; where something is inferred it says so.

The detail behind each point is in the document named beside it.

---

## 1. The product was built on GPL-3.0 software and shipped closed

The firmware is a derivative of **ESP-Miner**, released under GPL-3.0 by the
Bitaxe project. It was sold as a closed-source product. Source appeared only
after the licence violation was raised publicly, including by
[D-Central](https://d-central.tech/hammer-miner/).

Measured rather than asserted: 41 files match upstream ESP-Miner, 4,367 lines
identical, including the misspelling `health_maintennance.c` carried through
unchanged. `tools/compare_upstream.py` reproduces the comparison.

→ [PROVENANCE.md](PROVENANCE.md) sections 2 and 3

## 2. The release carried no licence and no attribution

`github.com/baichuan-org/BC04` shipped **no LICENSE file and no copyright
notices of any kind**. GPL-3.0 section 4 requires anyone conveying the work to
convey the licence text with it and keep the notices intact. Publishing source
alone does not satisfy that.

The practical effect is that the independent developers whose work made the
product possible were not credited to the people who bought it. Naming them is
the point of the licence.

→ [NOTICE.md](../NOTICE.md), [PROVENANCE.md](PROVENANCE.md) section 5.1

## 3. The BC01 source was published somewhere else, unlinked

The BC01 sold to customers is not in the BC04 repository. Its source is in a
**different organisation** (`baichuan-org/BC01`) that the product, the web UI,
and the update endpoint never reference. A buyer following the vendor's own
links does not reach the source for the device they own.

This repository initially recorded that the BC01 source had never been
published. That was wrong, and the correction is kept in the history rather
than quietly edited away.

→ [PROVENANCE.md](PROVENANCE.md) section 4

## 4. The published source does not build

`main/CMakeLists.txt` embeds `secure/flash_encryption_key.bin` into the
application in three build configurations. That file is **not in the
repository**, and CMake stops at the generate step:

```
CMake Error in main/CMakeLists.txt:
  Cannot find source file: secure/flash_encryption_key.bin
```

The reference build used for the comparison work here only completed after a
placeholder was supplied. The key itself need not be published — but a release
that unconditionally requires it, with no documented way to build without it,
is not a buildable release.

→ [PROVENANCE.md](PROVENANCE.md) section 5.4

## 5. A binary blob ships with no source

`components/a/liba.a` provides the command packing, CRC, and response parsing
for the scrypt ASIC path: `pack_ms_job_hashJob1`, `generate_job_crc16`,
`crc16_btc`, `parse_return_type`, `set_top_reg`, and others. No source
accompanies it. GPL-3.0 defines corresponding source as everything needed to
generate the object code.

The BM1370 path used one symbol from it, `CRC5`. That was reimplemented from
the blob's own DWARF debug information and verified against the vendor's test
vectors, so the Bitcoin path in this tree no longer needs the blob. The scrypt
path still does.

→ [ASIC-ABSTRACTION.md](ASIC-ABSTRACTION.md)

## 6. Owners cannot run modified firmware, in either direction

Read off a retail BC01:

```
Secure Boot:      Enabled
JTAG:             Permanently Disabled
Key digest slots: slot 0 in use, slots 1 and 2 REVOKED
Flash encryption: Release mode
```

- The bootloader boots only images signed by the vendor's key.
- **Both spare key slots are revoked**, so an owner cannot burn their own.
- **JTAG is permanently disabled**, so there is no debug path in.
- eFuses cannot be un-burned.

And it does not go the other way either. Flashed to a module whose eFuses are
not burned, the stock application refuses to start:

```
E flash_encrypt: Flash encryption eFuse bit was not enabled in bootloader
but CONFIG_SECURE_FLASH_ENC_ENABLED is on
abort() was called
```

So the owner of a device built on GPL-3.0 software can read the source but
cannot run a modified version of it on the hardware they bought. That is the
arrangement GPL-3.0 section 6 addresses through its Installation Information
requirement, and no such information accompanies the release.

The only remaining route is physical: the ESP32-S3 is a socketed LilyGO
T-Display-S3, and Secure Boot lives in that module's eFuses, so a fresh module
runs rebuilt firmware with no exploit involved. That is a hardware workaround
for a software lock-out, and it is what this repository had to do.

→ [SECURE-BOOT.md](SECURE-BOOT.md), [HARDWARE-SWAP.md](HARDWARE-SWAP.md)

## 7. The flash encryption key is compiled into the application

`main/http_server/http_server.c` reads the key back out of its own image at
runtime:

```c
extern const uint8_t flash_encryption_key_bin_start[]
    asm("_binary_flash_encryption_key_bin_start");
```

A key embedded in a distributed binary is recoverable by anyone holding that
binary. Noted as an observation about the vendor's design, not as something
this repository depends on.

## 8. The shipped HTTP API had no authentication

Every endpoint on the retail firmware — including the ones that change pool
credentials, core voltage, and frequency, and the one that accepts a firmware
image — answered any request on the local network with no credential of any
kind. Anyone who could reach the miner could repoint its hashrate or write to
its flash.

Authentication was added here: bearer tokens, 32 bytes of entropy, idle
expiry, lockout after repeated failures, and comparison that does not leak
length. It is off only if the owner deliberately clears the password, and the
firmware says so at boot.

→ [SECURITY.md](SECURITY.md), [AUTH.md](AUTH.md)

## 9. The firmware phoned home by default

A vendor telemetry endpoint and its access token were compiled in as defaults,
so a miner reported to the vendor unless its owner discovered the setting and
turned it off. Removed from the compile-time defaults here; owners who want
telemetry can still configure it.

## 10. The OTA container is obfuscated, and the images are signed

Update images are wrapped in a container with a type byte, a 48-byte header,
and an XOR pad. It is obfuscation, not encryption, and it was broken here
without the key; `tools/ota_tool.py` inspects, unpacks, packs, and round-trips
the vendor's own image byte for byte.

An earlier revision of this repository stated that the images were unsigned
and therefore trivially replaceable. **That was wrong.** There is a valid
RSA-3072-PSS signature block at 0x270000, and on a unit with Secure Boot
enabled the bootloader enforces it. The correction is in the history.

→ [OTA-FORMAT.md](OTA-FORMAT.md)

## 11. Defects in the shipped firmware

Found while getting a BC01 to run, all fixed here:

| Defect | Effect |
|---|---|
| `self_test.c` had no BC01 case and a duplicated model dispatch | BC04 ↔ BC08 reboot loop; NVS showed ~30 alternating writes |
| `should_test()` inverted | self-test ran when it should not |
| `TMP75_I2CADDR_BC01` set to 0x48 | wrong thermal sensor address; 0x49 confirmed against the vendor's own BC01 tree |
| `network_eth_init()` used `ESP_ERROR_CHECK` | abort on boot when no W5500 is fitted |
| uninitialised `ota_handle` | undefined behaviour in the update path |
| `ASIC_get_asic_count()` returned the BC08 count for BC06 | every per-chip figure skewed |
| `idf_component.yml` at the project root | never read by the build |
| `SERIAL_send()` tested `if (false)` where it meant `if (debug)` | the transmit dump every caller requests could never be produced |

The BC04 release also has **no BC01 or BC02 case** in several model switches,
so those boards fall through to `default` and never initialise — despite
`nvs_device.c` already configuring them.

→ [BC01-BRINGUP.md](BC01-BRINGUP.md)

## 12. What the vendor's BC01 release got right

Worth stating plainly, because it is the part that made this work possible: the
BC01 tree contains the USB Power Delivery stage the BC04 release omitted
entirely. Without `HUSB238A.c` the hashboard rail never comes up and a BC01
cannot mine at all. That file is imported here verbatim and credited.

---

## Independent corroboration

D-Central's `DCENT_OS` reached three of the same conclusions from its own
disassembly, without contact between the two efforts:

- ASIC UART **TX GPIO18 / RX GPIO17** — "the OPPOSITE of BC04; pinout does not
  track the family"
- **TMP75 at 0x49**
- **HUSB238A** as the USB-PD sink controller

Their firmware registers the Hammer boards but refuses to mine on them — it
ships no PD driver, so on a BC01 it would never open the VBUS gate.

## Status of this repository

Licensing restored, notices reinstated, security defects fixed, the BM1370
path freed of the binary blob, and a BC01 mining at **1.6–1.7 TH/s** on
rebuilt firmware. Changes here are GPL-3.0 and offered back under the same
terms.
